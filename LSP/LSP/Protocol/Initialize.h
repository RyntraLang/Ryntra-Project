#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace Ryntra::LSP::Protocol {
    enum class TextDocumentSyncKind : std::int32_t {
        None = 0,
        Full = 1,
        Incremental = 2,
    };

    struct ClientInfo {
        std::string name;
        std::optional<std::string> version;
    };

    struct InitializeParams {
        std::optional<std::int32_t> processId;
        std::optional<ClientInfo> clientInfo;
        std::optional<std::string> rootUri;
        nlohmann::json initializationOptions = nullptr;
    };

    struct ServerCapabilities {
        TextDocumentSyncKind textDocumentSync = TextDocumentSyncKind::None;
    };

    struct ServerInfo {
        std::string name;
        std::optional<std::string> version;
    };

    struct InitializeResult {
        ServerCapabilities capabilities;
        ServerInfo serverInfo;
    };

    InitializeParams parseInitializeParams(const nlohmann::json &params);

    nlohmann::json serializeInitializeResult(const InitializeResult &result);
} // namespace Ryntra::LSP::Protocol
