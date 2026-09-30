#include "Protocol/TextDocument.h"

#include "JsonRPC/JsonRPC.h"

#include <utility>

namespace Ryntra::LSP::Protocol {
    namespace {
        const nlohmann::json &field(const nlohmann::json &object, const char *name) {
            if (!object.is_object() || !object.contains(name)) {
                throw JsonRPCException({JsonRPCErrorCode::InvalidParams, std::string("Missing '") + name + "' field."});
            }

            return object.at(name);
        }

        std::string stringField(const nlohmann::json &object, const char *name) {
            const nlohmann::json &value = field(object, name);

            if (!value.is_string()) {
                throw JsonRPCException({JsonRPCErrorCode::InvalidParams, std::string("'") + name + "' must be a string."});
            }

            return value.get<std::string>();
        }

        std::int32_t intField(const nlohmann::json &object, const char *name) {
            const nlohmann::json &value = field(object, name);

            if (!value.is_number_integer()) {
                throw JsonRPCException({JsonRPCErrorCode::InvalidParams, std::string("'") + name + "' must be an integer."});
            }

            return value.get<std::int32_t>();
        }

        std::uint32_t uintField(const nlohmann::json &object, const char *name) {
            const nlohmann::json &value = field(object, name);

            if (!value.is_number_integer()) {
                throw JsonRPCException({JsonRPCErrorCode::InvalidParams, std::string("'") + name + "' must be an integer."});
            }

            const std::int64_t number = value.get<std::int64_t>();

            if (number < 0) {
                throw JsonRPCException({JsonRPCErrorCode::InvalidParams, std::string("'") + name + "' must not be negative."});
            }

            return static_cast<std::uint32_t>(number);
        }

        Position parsePosition(const nlohmann::json &json) {
            Position position;
            position.line = uintField(json, "line");
            position.character = uintField(json, "character");
            return position;
        }

        Range parseRange(const nlohmann::json &json) {
            Range range;
            range.start = parsePosition(field(json, "start"));
            range.end = parsePosition(field(json, "end"));
            return range;
        }
    } // namespace

    TextDocumentItem parseTextDocumentItem(const nlohmann::json &json) {
        TextDocumentItem item;
        item.uri = stringField(json, "uri");
        item.languageId = stringField(json, "languageId");
        item.version = intField(json, "version");
        item.text = stringField(json, "text");
        return item;
    }

    DidOpenTextDocumentParams parseDidOpenParams(const nlohmann::json &params) {
        DidOpenTextDocumentParams result;
        result.textDocument = parseTextDocumentItem(field(params, "textDocument"));
        return result;
    }

    DidChangeTextDocumentParams parseDidChangeParams(const nlohmann::json &params) {
        DidChangeTextDocumentParams result;

        const nlohmann::json &document = field(params, "textDocument");
        result.textDocument.uri = stringField(document, "uri");
        result.textDocument.version = intField(document, "version");

        const nlohmann::json &changes = field(params, "contentChanges");

        if (!changes.is_array()) {
            throw JsonRPCException({JsonRPCErrorCode::InvalidParams, "'contentChanges' must be an array."});
        }

        for (const nlohmann::json &change : changes) {
            TextDocumentContentChangeEvent event;
            event.text = stringField(change, "text");

            if (change.contains("range") && !change.at("range").is_null()) {
                event.range = parseRange(change.at("range"));
            }

            result.contentChanges.push_back(std::move(event));
        }

        return result;
    }

    DidCloseTextDocumentParams parseDidCloseParams(const nlohmann::json &params) {
        DidCloseTextDocumentParams result;
        result.textDocument.uri = stringField(field(params, "textDocument"), "uri");
        return result;
    }

    TextDocumentPositionParams parseTextDocumentPositionParams(const nlohmann::json &params) {
        TextDocumentPositionParams result;
        result.textDocument.uri = stringField(field(params, "textDocument"), "uri");
        result.position = parsePosition(field(params, "position"));
        return result;
    }

    DocumentSymbolParams parseDocumentSymbolParams(const nlohmann::json &params) {
        DocumentSymbolParams result;
        result.textDocument.uri = stringField(field(params, "textDocument"), "uri");
        return result;
    }
} // namespace Ryntra::LSP::Protocol
