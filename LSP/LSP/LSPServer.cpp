#include "LSPServer.h"

#include "Analysis/Completion.h"
#include "Analysis/DocumentSymbol.h"
#include "Analysis/Hover.h"
#include "Analysis/Location.h"
#include "Analysis/SemanticTokens.h"
#include "Diagnostics/Diagnostic.h"
#include "Protocol/Initialize.h"
#include "Protocol/Protocol.h"

#include <iostream>
#include <optional>
#include <print>

namespace Ryntra::LSP {
    namespace {
        constexpr int kExitSuccess = 0;
        constexpr int kExitFailure = 1;
    } // anonymous namespace

    LSPServer::LSPServer(JsonRpcTransport &transport, const LanguageProviders &providers)
        : transport(transport), providers(providers) {
    }

    int LSPServer::run() {
        while (state != ServerState::Exited) {
            std::optional<nlohmann::json> rawMessage;

            try {
                rawMessage = transport.readMessage();
            } catch (const JsonRPCException &exception) {
                const JsonRPCError &error = exception.getError();
                sendError(nullptr, error.code, error.message, error.data);
                continue;
            } catch (const std::exception &exception) {
                std::print(std::cerr, "Transport error: {}\n", exception.what());
                return kExitFailure;
            }

            if (!rawMessage.has_value()) {
                break;
            }

            try {
                dispatch(parseMessage(*rawMessage));
            } catch (const JsonRPCException &exception) {
                const JsonRPCError &error = exception.getError();
                sendError(nullptr, error.code, error.message, error.data);
            }
        }

        if (state == ServerState::Exited) {
            return shutdownRequested ? kExitSuccess : kExitFailure;
        }

        return kExitSuccess;
    }

    void LSPServer::dispatch(const JsonRPCMessage &message) {
        if (const auto *request = std::get_if<JsonRPCRequest>(&message)) {
            handleRequest(*request);
        } else if (const auto *notification = std::get_if<JsonRPCNotification>(&message)) {
            handleNotification(*notification);
        }
    }

    void LSPServer::handleRequest(const JsonRPCRequest &request) {
        if (request.method == Protocol::kInitialize) {
            if (state != ServerState::Uninitialized) {
                sendError(request.id, JsonRPCErrorCode::InvalidRequest, "Server has already been initialized.");
                return;
            }

            handleInitialize(request);
            return;
        }

        if (state == ServerState::Uninitialized) {
            sendError(request.id, JsonRPCErrorCode::ServerNotInitialized, "Server has not been initialized.");
            return;
        }

        if (state == ServerState::ShuttingDown) {
            sendError(request.id, JsonRPCErrorCode::InvalidRequest, "Server is shutting down.");
            return;
        }

        if (request.method == Protocol::kShutdown) {
            handleShutdown(request);
            return;
        }

        if (request.method == Protocol::kTextDocumentHover) {
            handleHover(request);
            return;
        }

        if (request.method == Protocol::kTextDocumentDefinition) {
            handleDefinition(request);
            return;
        }

        if (request.method == Protocol::kTextDocumentDocumentSymbol) {
            handleDocumentSymbol(request);
            return;
        }

        if (request.method == Protocol::kTextDocumentCompletion) {
            handleCompletion(request);
            return;
        }

        if (request.method == Protocol::kTextDocumentSemanticTokensFull) {
            handleSemanticTokens(request);
            return;
        }

        sendError(request.id, JsonRPCErrorCode::MethodNotFound, "Method not found: " + request.method);
    }

    void LSPServer::handleNotification(const JsonRPCNotification &notification) {
        if (notification.method == Protocol::kExit) {
            state = ServerState::Exited;
            return;
        }

        // The 'initialized' notification and all other notifications are dropped
        // until the client has finished the initialization handshake.
        if (state != ServerState::Initialized) {
            return;
        }

        try {
            if (notification.method == Protocol::kTextDocumentDidOpen) {
                handleDidOpen(Protocol::parseDidOpenParams(notification.params));
            } else if (notification.method == Protocol::kTextDocumentDidChange) {
                handleDidChange(Protocol::parseDidChangeParams(notification.params));
            } else if (notification.method == Protocol::kTextDocumentDidClose) {
                handleDidClose(Protocol::parseDidCloseParams(notification.params));
            }
        } catch (const JsonRPCException &exception) {
            std::print(std::cerr, "Invalid notification '{}': {}\n", notification.method, exception.what());
        }
    }

    void LSPServer::handleInitialize(const JsonRPCRequest &request) {
        const Protocol::InitializeParams params = Protocol::parseInitializeParams(request.params);

        Protocol::InitializeResult result;
        result.capabilities.textDocumentSync = Protocol::TextDocumentSyncKind::Full;
        result.capabilities.hoverProvider = true;
        result.capabilities.definitionProvider = true;
        result.capabilities.documentSymbolProvider = true;
        result.capabilities.completionProvider = true;
        result.capabilities.completionTriggerCharacters = {"."};
        result.capabilities.semanticTokensProvider = true;
        result.serverInfo = {std::string(Protocol::kServerName), std::string(Protocol::kServerVersion)};

        sendResponse(request.id, Protocol::serializeInitializeResult(result));

        state = ServerState::Initialized;

        if (params.clientInfo.has_value()) {
            std::print(std::cerr, "Client connected: {}\n", params.clientInfo->name);
        }
    }

    void LSPServer::handleShutdown(const JsonRPCRequest &request) {
        shutdownRequested = true;
        state = ServerState::ShuttingDown;

        sendResponse(request.id, nullptr);
    }

    void LSPServer::handleDidOpen(const Protocol::DidOpenTextDocumentParams &params) {
        Document document;
        document.uri = params.textDocument.uri;
        document.languageId = params.textDocument.languageId;
        document.version = params.textDocument.version;
        document.text = params.textDocument.text;

        documentManager.open(document);
        publishDiagnostics(document.uri);
    }

    void LSPServer::handleDidChange(const Protocol::DidChangeTextDocumentParams &params) {
        if (!documentManager.change(params.textDocument.uri, params.textDocument.version, params.contentChanges)) {
            std::print(std::cerr, "Received didChange for an unopened document: {}\n", params.textDocument.uri);
        }

        publishDiagnostics(params.textDocument.uri);
    }

    void LSPServer::handleDidClose(const Protocol::DidCloseTextDocumentParams &params) {
        if (!documentManager.close(params.textDocument.uri)) {
            std::print(std::cerr, "Received didClose for an unopened document: {}\n", params.textDocument.uri);
        }

        publishDiagnostics(params.textDocument.uri);
    }

    void LSPServer::handleHover(const JsonRPCRequest &request) {
        try {
            const Protocol::TextDocumentPositionParams params = Protocol::parseTextDocumentPositionParams(request.params);
            const Document *document = documentManager.get(params.textDocument.uri);

            if (document == nullptr || providers.hover == nullptr) {
                sendResponse(request.id, nullptr);
                return;
            }

            const std::optional<Hover> hover = providers.hover->hover(params.textDocument.uri, document->text, params.position);
            sendResponse(request.id, hover.has_value() ? serialize(*hover) : nlohmann::json(nullptr));
        } catch (const JsonRPCException &exception) {
            const JsonRPCError &error = exception.getError();
            sendError(request.id, error.code, error.message, error.data);
        } catch (const std::exception &exception) {
            std::print(std::cerr, "Failed to compute hover: {}\n", exception.what());
            sendResponse(request.id, nullptr);
        }
    }

    void LSPServer::handleDefinition(const JsonRPCRequest &request) {
        try {
            const Protocol::TextDocumentPositionParams params = Protocol::parseTextDocumentPositionParams(request.params);
            const Document *document = documentManager.get(params.textDocument.uri);

            if (document == nullptr || providers.definition == nullptr) {
                sendResponse(request.id, nullptr);
                return;
            }

            const std::optional<Location> location = providers.definition->definition(params.textDocument.uri, document->text, params.position);
            sendResponse(request.id, location.has_value() ? serialize(*location) : nlohmann::json(nullptr));
        } catch (const JsonRPCException &exception) {
            const JsonRPCError &error = exception.getError();
            sendError(request.id, error.code, error.message, error.data);
        } catch (const std::exception &exception) {
            std::print(std::cerr, "Failed to compute definition: {}\n", exception.what());
            sendResponse(request.id, nullptr);
        }
    }

    void LSPServer::handleDocumentSymbol(const JsonRPCRequest &request) {
        try {
            const Protocol::DocumentSymbolParams params = Protocol::parseDocumentSymbolParams(request.params);
            const Document *document = documentManager.get(params.textDocument.uri);

            if (document == nullptr || providers.documentSymbols == nullptr) {
                sendResponse(request.id, nlohmann::json::array());
                return;
            }

            const std::vector<DocumentSymbol> symbols = providers.documentSymbols->documentSymbols(params.textDocument.uri, document->text);
            sendResponse(request.id, serialize(symbols));
        } catch (const JsonRPCException &exception) {
            const JsonRPCError &error = exception.getError();
            sendError(request.id, error.code, error.message, error.data);
        } catch (const std::exception &exception) {
            std::print(std::cerr, "Failed to compute document symbols: {}\n", exception.what());
            sendResponse(request.id, nlohmann::json::array());
        }
    }

    void LSPServer::handleCompletion(const JsonRPCRequest &request) {
        try {
            const Protocol::TextDocumentPositionParams params = Protocol::parseTextDocumentPositionParams(request.params);
            const Document *document = documentManager.get(params.textDocument.uri);

            if (document == nullptr || providers.completion == nullptr) {
                sendResponse(request.id, nlohmann::json::array());
                return;
            }

            const std::vector<CompletionItem> items = providers.completion->completion(params.textDocument.uri, document->text, params.position);
            sendResponse(request.id, serialize(items));
        } catch (const JsonRPCException &exception) {
            const JsonRPCError &error = exception.getError();
            sendError(request.id, error.code, error.message, error.data);
        } catch (const std::exception &exception) {
            std::print(std::cerr, "Failed to compute completion: {}\n", exception.what());
            sendResponse(request.id, nlohmann::json::array());
        }
    }

    void LSPServer::handleSemanticTokens(const JsonRPCRequest &request) {
        try {
            const Protocol::SemanticTokensParams params = Protocol::parseSemanticTokensParams(request.params);
            const Document *document = documentManager.get(params.textDocument.uri);

            if (document == nullptr || providers.semanticTokens == nullptr) {
                sendResponse(request.id, serialize(SemanticTokens{}));
                return;
            }

            const SemanticTokens tokens = encodeSemanticTokens(providers.semanticTokens->semanticTokens(params.textDocument.uri, document->text));
            sendResponse(request.id, serialize(tokens));
        } catch (const JsonRPCException &exception) {
            const JsonRPCError &error = exception.getError();
            sendError(request.id, error.code, error.message, error.data);
        } catch (const std::exception &exception) {
            std::print(std::cerr, "Failed to compute semantic tokens: {}\n", exception.what());
            sendResponse(request.id, serialize(SemanticTokens{}));
        }
    }

    void LSPServer::publishDiagnostics(const std::string &uri) {
        std::vector<Diagnostic> diagnostics;

        if (const Document *document = documentManager.get(uri); document != nullptr && providers.diagnostics != nullptr) {
            try {
                diagnostics = providers.diagnostics->analyze(uri, document->text);
            } catch (const std::exception &exception) {
                std::print(std::cerr, "Failed to compute diagnostics for {}: {}\n", uri, exception.what());
            }
        }

        sendNotification(std::string(Protocol::kTextDocumentPublishDiagnostics), serialize(PublishDiagnosticsParams{uri, diagnostics}));
    }

    void LSPServer::sendResponse(const nlohmann::json &id, const nlohmann::json &result) {
        transport.writeMessage(serialize(JsonRPCResponse{id, result}));
    }

    void LSPServer::sendError(const nlohmann::json &id, JsonRPCErrorCode code, const std::string &message, const nlohmann::json &data) {
        transport.writeMessage(serialize(JsonRPCErrorResponse{id, JsonRPCError{code, message, data}}));
    }

    void LSPServer::sendNotification(const std::string &method, const nlohmann::json &params) {
        transport.writeMessage(serialize(JsonRPCNotification{method, params}));
    }
} // namespace Ryntra::LSP
