#pragma once

#include "Analysis/DocumentSymbol.h"

#include <string>
#include <vector>

namespace Ryntra::LSP {
    /// \brief Boundary for "document symbols" queries. The server knows only this interface.
    class DocumentSymbolProvider {
    public:
        virtual ~DocumentSymbolProvider() = default;

        [[nodiscard]] virtual std::vector<DocumentSymbol> documentSymbols(const std::string &uri, const std::string &text) = 0;
    };
} // namespace Ryntra::LSP
