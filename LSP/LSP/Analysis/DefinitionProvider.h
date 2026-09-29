#pragma once

#include "Analysis/Location.h"
#include "Protocol/LSPTypes.h"

#include <optional>
#include <string>

namespace Ryntra::LSP {
    /// \brief Boundary for "go to definition" queries. The server knows only this interface.
    class DefinitionProvider {
    public:
        virtual ~DefinitionProvider() = default;

        [[nodiscard]] virtual std::optional<Location> definition(const std::string &uri, const std::string &text, const Protocol::Position &position) = 0;
    };
} // namespace Ryntra::LSP
