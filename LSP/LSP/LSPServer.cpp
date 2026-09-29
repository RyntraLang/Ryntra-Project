#include "LSPServer.h"

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

    LSPServer::LSPServer(JsonRpcTransport &transport) : transport(transport) {
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
    }

    void LSPServer::handleDidChange(const Protocol::DidChangeTextDocumentParams &params) {
        if (!documentManager.change(params.textDocument.uri, params.textDocument.version, params.contentChanges)) {
            std::print(std::cerr, "Received didChange for an unopened document: {}\n", params.textDocument.uri);
        }
    }

    void LSPServer::handleDidClose(const Protocol::DidCloseTextDocumentParams &params) {
        if (!documentManager.close(params.textDocument.uri)) {
            std::print(std::cerr, "Received didClose for an unopened document: {}\n", params.textDocument.uri);
        }
    }

    void LSPServer::sendResponse(const nlohmann::json &id, const nlohmann::json &result) {
        transport.writeMessage(serialize(JsonRPCResponse{id, result}));
    }

    void LSPServer::sendError(const nlohmann::json &id, JsonRPCErrorCode code, const std::string &message, const nlohmann::json &data) {
        transport.writeMessage(serialize(JsonRPCErrorResponse{id, JsonRPCError{code, message, data}}));
    }
} // namespace Ryntra::LSP
