#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>

namespace Ryntra::LSP::Protocol {
    struct Position {
        std::uint32_t line = 0;
        std::uint32_t character = 0;
    };

    struct Range {
        Position start;
        Position end;
    };

    nlohmann::json serialize(const Position &position);

    nlohmann::json serialize(const Range &range);
} // namespace Ryntra::LSP::Protocol
