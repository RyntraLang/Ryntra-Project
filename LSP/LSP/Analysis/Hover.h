#pragma once

#include "Protocol/LSPTypes.h"

#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace Ryntra::LSP {
    struct Hover {
        std::string contents;
        std::optional<Protocol::Range> range;
    };

    nlohmann::json serialize(const Hover &hover);
} // namespace Ryntra::LSP
