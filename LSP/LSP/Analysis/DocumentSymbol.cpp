#include "Analysis/DocumentSymbol.h"

namespace Ryntra::LSP {
    nlohmann::json serialize(const DocumentSymbol &symbol) {
        nlohmann::json result = {
            {"name", symbol.name},
            {"kind", static_cast<std::int32_t>(symbol.kind)},
            {"range", Protocol::serialize(symbol.range)},
            {"selectionRange", Protocol::serialize(symbol.selectionRange)},
        };

        if (!symbol.children.empty()) {
            result["children"] = serialize(symbol.children);
        }

        return result;
    }

    nlohmann::json serialize(const std::vector<DocumentSymbol> &symbols) {
        nlohmann::json array = nlohmann::json::array();

        for (const DocumentSymbol &symbol : symbols) {
            array.push_back(serialize(symbol));
        }

        return array;
    }
} // namespace Ryntra::LSP
