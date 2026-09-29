#pragma once

#include "Protocol/LSPTypes.h"

#include <nlohmann/json.hpp>
#include <string>

namespace Ryntra::LSP {
    struct Location {
        std::string uri;
        Protocol::Range range;
    };

    nlohmann::json serialize(const Location &location);
} // namespace Ryntra::LSP
