#pragma once

#include "Protocol/LSPTypes.h"

#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace Ryntra::LSP {
    // Order defines the legend advertised during `initialize`. Clients map these
    // standard names onto their theme.
    enum class SemanticTokenType : std::uint32_t {
        Keyword = 0,
        Type,
        Function,
        Variable,
        Property,
        Parameter,
        Struct,
        Comment,
        String,
        Number,
        Operator,
    };

    enum class SemanticTokenModifier : std::uint32_t {
        Declaration = 1u << 0,
    };

    struct SemanticToken {
        Protocol::Position start;
        std::uint32_t length = 0;
        SemanticTokenType type = SemanticTokenType::Variable;
        std::uint32_t modifiers = 0;
    };

    struct SemanticTokens {
        std::vector<std::uint32_t> data;
    };

    const std::vector<std::string> &semanticTokenTypeNames();

    const std::vector<std::string> &semanticTokenModifierNames();

    nlohmann::json serialize(const SemanticTokens &tokens);

    // Encode tokens into the LSP relative-position format (sorted, 5 integers each).
    SemanticTokens encodeSemanticTokens(std::vector<SemanticToken> tokens);
} // namespace Ryntra::LSP
