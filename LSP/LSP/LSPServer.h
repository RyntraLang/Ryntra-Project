#pragma once

#include "Analysis/LanguageProviders.h"
#include "DocumentManager.h"
#include "JsonRPC/JsonRPC.h"
#include "JsonRPCTransport/Transport.h"
#include "Protocol/TextDocument.h"

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
        LSPServer(JsonRpcTransport &transport, const LanguageProviders &providers);

        int run();

    private:
        void dispatch(const JsonRPCMessage &message);

        void handleRequest(const JsonRPCRequest &request);

        void handleNotification(const JsonRPCNotification &notification);

        void handleInitialize(const JsonRPCRequest &request);

        void handleShutdown(const JsonRPCRequest &request);

        void handleDidOpen(const Protocol::DidOpenTextDocumentParams &params);

        void handleDidChange(const Protocol::DidChangeTextDocumentParams &params);

        void handleDidClose(const Protocol::DidCloseTextDocumentParams &params);

        void handleHover(const JsonRPCRequest &request);

        void handleDefinition(const JsonRPCRequest &request);

        void handleDocumentSymbol(const JsonRPCRequest &request);

        void handleCompletion(const JsonRPCRequest &request);

        void handleSemanticTokens(const JsonRPCRequest &request);

        void publishDiagnostics(const std::string &uri);

        void sendResponse(const nlohmann::json &id, const nlohmann::json &result);

        void sendError(const nlohmann::json &id, JsonRPCErrorCode code, const std::string &message, const nlohmann::json &data = nullptr);

        void sendNotification(const std::string &method, const nlohmann::json &params);

        JsonRpcTransport &transport;
        LanguageProviders providers;
        DocumentManager documentManager;
        ServerState state = ServerState::Uninitialized;
        bool shutdownRequested = false;
    };
} // namespace Ryntra::LSP
