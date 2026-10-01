#include "Protocol/LSPTypes.h"

namespace Ryntra::LSP::Protocol {
    nlohmann::json serialize(const Position &position) {
        return {
            {"line", position.line},
            {"character", position.character},
        };
    }

    nlohmann::json serialize(const Range &range) {
        return {
            {"start", serialize(range.start)},
            {"end", serialize(range.end)},
        };
    }
} // namespace Ryntra::LSP::Protocol
