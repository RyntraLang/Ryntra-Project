#pragma once

#include "Analysis/Hover.h"
#include "Protocol/LSPTypes.h"

#include <optional>
#include <string>

namespace Ryntra::LSP {
    /// \brief Boundary for "hover" queries. The server knows only this interface.
    class HoverProvider {
    public:
        virtual ~HoverProvider() = default;

        [[nodiscard]] virtual std::optional<Hover> hover(const std::string &uri, const std::string &text, const Protocol::Position &position) = 0;
    };
} // namespace Ryntra::LSP
