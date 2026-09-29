#pragma once

#include "JsonRPC/JsonRPC.h"
#include "JsonRPCTransport/Transport.h"

#include <nlohmann/json.hpp>

namespace Ryntra::LSP {
    enum class ServerState {
        Uninitialized,
        Initialized,
        ShuttingDown,
        Exited,
    };

    class LSPServer {
    public:
        explicit LSPServer(JsonRpcTransport &transport);

        int run();

    private:
        void dispatch(const JsonRPCMessage &message);

        void handleRequest(const JsonRPCRequest &request);

        void handleNotification(const JsonRPCNotification &notification);

        void handleInitialize(const JsonRPCRequest &request);

        void handleShutdown(const JsonRPCRequest &request);

        void sendResponse(const nlohmann::json &id, const nlohmann::json &result);

        void sendError(const nlohmann::json &id, JsonRPCErrorCode code, const std::string &message, const nlohmann::json &data = nullptr);

        JsonRpcTransport &transport;
        ServerState state = ServerState::Uninitialized;
        bool shutdownRequested = false;
    };
} // namespace Ryntra::LSP
