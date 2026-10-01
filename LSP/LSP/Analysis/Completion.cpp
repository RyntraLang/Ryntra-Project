#include "Analysis/Completion.h"

namespace Ryntra::LSP {
    nlohmann::json serialize(const CompletionItem &item) {
        nlohmann::json result = {
            {"label", item.label},
            {"kind", static_cast<std::int32_t>(item.kind)},
        };

        if (!item.detail.empty()) {
            result["detail"] = item.detail;
        }

        return result;
    }

    nlohmann::json serialize(const std::vector<CompletionItem> &items) {
        nlohmann::json array = nlohmann::json::array();

        for (const CompletionItem &item : items) {
            array.push_back(serialize(item));
        }

        return array;
    }
} // namespace Ryntra::LSP
