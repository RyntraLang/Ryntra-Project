#include "Analysis/Location.h"

namespace Ryntra::LSP {
    nlohmann::json serialize(const Location &location) {
        return {
            {"uri", location.uri},
            {"range", Protocol::serialize(location.range)},
        };
    }
} // namespace Ryntra::LSP
