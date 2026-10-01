#include "Analysis/SemanticTokens.h"

#include <algorithm>

namespace Ryntra::LSP {
    const std::vector<std::string> &semanticTokenTypeNames() {
        static const std::vector<std::string> names = {
            "keyword",
            "type",
            "function",
            "variable",
            "property",
            "parameter",
            "struct",
            "comment",
            "string",
            "number",
            "operator",
        };
        return names;
    }

    const std::vector<std::string> &semanticTokenModifierNames() {
        static const std::vector<std::string> names = {
            "declaration",
        };
        return names;
    }

    nlohmann::json serialize(const SemanticTokens &tokens) {
        return {
            {"data", tokens.data},
        };
    }

    SemanticTokens encodeSemanticTokens(std::vector<SemanticToken> tokens) {
        std::sort(tokens.begin(), tokens.end(), [](const SemanticToken &left, const SemanticToken &right) {
            if (left.start.line != right.start.line) {
                return left.start.line < right.start.line;
            }
            return left.start.character < right.start.character;
        });

        SemanticTokens result;
        std::uint32_t previousLine = 0;
        std::uint32_t previousCharacter = 0;
        bool first = true;

        for (const SemanticToken &token : tokens) {
            if (token.length == 0) {
                continue;
            }

            std::uint32_t deltaLine = 0;
            std::uint32_t deltaCharacter = 0;

            if (first) {
                deltaLine = token.start.line;
                deltaCharacter = token.start.character;
                first = false;
            } else if (token.start.line == previousLine) {
                deltaLine = 0;
                deltaCharacter = token.start.character - previousCharacter;
            } else {
                deltaLine = token.start.line - previousLine;
                deltaCharacter = token.start.character;
            }

            result.data.push_back(deltaLine);
            result.data.push_back(deltaCharacter);
            result.data.push_back(token.length);
            result.data.push_back(static_cast<std::uint32_t>(token.type));
            result.data.push_back(token.modifiers);

            previousLine = token.start.line;
            previousCharacter = token.start.character;
        }

        return result;
    }
} // namespace Ryntra::LSP
