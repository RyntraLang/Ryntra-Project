#include "Text/TextOffset.h"

#include <algorithm>
#include <cctype>

namespace Ryntra::LSP {
    namespace {
        std::size_t utf8Length(const unsigned char byte) {
            if ((byte & 0xE0u) == 0xC0u) {
                return 2;
            }
            if ((byte & 0xF0u) == 0xE0u) {
                return 3;
            }
            if ((byte & 0xF8u) == 0xF0u) {
                return 4;
            }
            return 1;
        }

        bool isWordContinuation(const unsigned char byte) {
            return std::isalnum(byte) != 0 || byte == '_';
        }

        bool isWordStart(const unsigned char byte) {
            return std::isalpha(byte) != 0 || byte == '_';
        }
    } // namespace

    std::size_t offsetAt(const std::string &text, const Protocol::Position &position) {
        std::uint32_t line = 0;
        std::uint32_t character = 0;
        std::size_t index = 0;

        while (index < text.size() && line < position.line) {
            if (text[index] == '\n') {
                ++line;
            }

            ++index;
        }

        while (index < text.size() && character < position.character) {
            const unsigned char byte = static_cast<unsigned char>(text[index]);

            if (byte == '\r' || byte == '\n') {
                break;
            }

            const std::size_t length = utf8Length(byte);
            const std::uint32_t units = length == 4 ? 2 : 1;

            if (character + units > position.character) {
                break;
            }

            character += units;
            index += length;
        }

        return index;
    }

    Protocol::Position positionAt(const std::string &text, const std::size_t offset) {
        const std::size_t limit = std::min(offset, text.size());

        Protocol::Position position;
        std::size_t lineStart = 0;

        for (std::size_t index = 0; index < limit; ++index) {
            if (text[index] == '\n') {
                position.line += 1;
                lineStart = index + 1;
            }
        }

        std::uint32_t character = 0;

        for (std::size_t index = lineStart; index < limit;) {
            const unsigned char byte = static_cast<unsigned char>(text[index]);
            const std::size_t length = utf8Length(byte);

            character += length == 4 ? 2 : 1;
            index += length;
        }

        position.character = character;
        return position;
    }

    std::optional<IdentifierSpan> identifierAt(const std::string &text, const Protocol::Position &position) {
        if (text.empty()) {
            return std::nullopt;
        }

        std::size_t offset = offsetAt(text, position);

        if (offset > text.size()) {
            return std::nullopt;
        }

        // When the cursor sits just past an identifier, step back onto it.
        if (offset == text.size() || !isWordContinuation(static_cast<unsigned char>(text[offset]))) {
            if (offset > 0 && isWordContinuation(static_cast<unsigned char>(text[offset - 1]))) {
                offset -= 1;
            } else {
                return std::nullopt;
            }
        }

        std::size_t start = offset;
        while (start > 0 && isWordContinuation(static_cast<unsigned char>(text[start - 1]))) {
            start -= 1;
        }

        std::size_t end = offset;
        while (end < text.size() && isWordContinuation(static_cast<unsigned char>(text[end]))) {
            end += 1;
        }

        if (!isWordStart(static_cast<unsigned char>(text[start]))) {
            return std::nullopt;
        }

        IdentifierSpan span;
        span.name = text.substr(start, end - start);
        span.range.start = positionAt(text, start);
        span.range.end = positionAt(text, end);
        return span;
    }
} // namespace Ryntra::LSP
