#pragma once

#include "Analysis/SemanticTokens.h"

#include <string>
#include <vector>

namespace Ryntra::LSP {
    /// \brief Boundary for semantic-token queries. The server knows only this interface.
    class SemanticTokensProvider {
    public:
        virtual ~SemanticTokensProvider() = default;

        [[nodiscard]] virtual std::vector<SemanticToken> semanticTokens(const std::string &uri, const std::string &text) = 0;
    };
} // namespace Ryntra::LSP
