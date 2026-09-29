#pragma once

#include "Protocol/LSPTypes.h"

#include <cstddef>
#include <optional>
#include <string>

namespace Ryntra::LSP {
    struct IdentifierSpan {
        std::string name;
        Protocol::Range range;
    };

    /// \brief Convert an LSP position (line + UTF-16 character) to a byte offset.
    [[nodiscard]] std::size_t offsetAt(const std::string &text, const Protocol::Position &position);

    /// \brief Convert a byte offset back to an LSP position.
    [[nodiscard]] Protocol::Position positionAt(const std::string &text, std::size_t offset);

    /// \brief Find the identifier under the given position.
    [[nodiscard]] std::optional<IdentifierSpan> identifierAt(const std::string &text, const Protocol::Position &position);
} // namespace Ryntra::LSP
