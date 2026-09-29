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

    void LSPServer::sendResponse(const nlohmann::json &id, const nlohmann::json &result) {
        transport.writeMessage(serialize(JsonRPCResponse{id, result}));
    }

    void LSPServer::sendError(const nlohmann::json &id, JsonRPCErrorCode code, const std::string &message, const nlohmann::json &data) {
        transport.writeMessage(serialize(JsonRPCErrorResponse{id, JsonRPCError{code, message, data}}));
    }
} // namespace Ryntra::LSP
