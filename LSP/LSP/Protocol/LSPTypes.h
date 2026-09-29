#pragma once

#include <cstdint>

namespace Ryntra::LSP::Protocol {
    struct Position {
        std::uint32_t line = 0;
        std::uint32_t character = 0;
    };

    struct Range {
        Position start;
        Position end;
    };
} // namespace Ryntra::LSP::Protocol
