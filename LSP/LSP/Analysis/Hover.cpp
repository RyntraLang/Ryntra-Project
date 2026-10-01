#include "Analysis/Hover.h"

namespace Ryntra::LSP {
    nlohmann::json serialize(const Hover &hover) {
        nlohmann::json result = {
            {"contents", {
                {"kind", "markdown"},
                {"value", hover.contents},
            }},
        };

        if (hover.range.has_value()) {
            result["range"] = Protocol::serialize(*hover.range);
        }

        return result;
    }
} // namespace Ryntra::LSP
