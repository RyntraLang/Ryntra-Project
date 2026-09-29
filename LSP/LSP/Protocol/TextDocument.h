#pragma once

#include "Protocol/LSPTypes.h"

#include <cstdint>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace Ryntra::LSP::Protocol {
    struct TextDocumentIdentifier {
        std::string uri;
    };

    struct TextDocumentPositionParams {
        TextDocumentIdentifier textDocument;
        Position position;
    };

    struct VersionedTextDocumentIdentifier {
        std::string uri;
        std::int32_t version = 0;
    };

    struct TextDocumentItem {
        std::string uri;
        std::string languageId;
        std::int32_t version = 0;
        std::string text;
    };

    struct TextDocumentContentChangeEvent {
        std::optional<Range> range;
        std::string text;
    };

    struct DidOpenTextDocumentParams {
        TextDocumentItem textDocument;
    };

    struct DidChangeTextDocumentParams {
        VersionedTextDocumentIdentifier textDocument;
        std::vector<TextDocumentContentChangeEvent> contentChanges;
    };

    struct DidCloseTextDocumentParams {
        TextDocumentIdentifier textDocument;
    };

    TextDocumentItem parseTextDocumentItem(const nlohmann::json &json);

    DidOpenTextDocumentParams parseDidOpenParams(const nlohmann::json &params);

    DidChangeTextDocumentParams parseDidChangeParams(const nlohmann::json &params);

    DidCloseTextDocumentParams parseDidCloseParams(const nlohmann::json &params);

    TextDocumentPositionParams parseTextDocumentPositionParams(const nlohmann::json &params);
} // namespace Ryntra::LSP::Protocol
