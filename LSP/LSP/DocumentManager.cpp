#include "DocumentManager.h"

namespace Ryntra::LSP {
    namespace {
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

                std::size_t length = 1;
                std::uint32_t units = 1;

                if ((byte & 0xE0u) == 0xC0u) {
                    length = 2;
                } else if ((byte & 0xF0u) == 0xE0u) {
                    length = 3;
                } else if ((byte & 0xF8u) == 0xF0u) {
                    length = 4;
                    units = 2;
                }

                if (character + units > position.character) {
                    break;
                }

                character += units;
                index += length;
            }

            return index;
        }

        void applyChange(std::string &text, const Protocol::TextDocumentContentChangeEvent &change) {
            if (!change.range.has_value()) {
                text = change.text;
                return;
            }

            const std::size_t start = offsetAt(text, change.range->start);
            const std::size_t end = offsetAt(text, change.range->end);

            if (end <= start) {
                text.insert(start, change.text);
                return;
            }

            text.replace(start, end - start, change.text);
        }
    } // namespace

    void DocumentManager::open(const Document &document) {
        documents[document.uri] = document;
    }

    bool DocumentManager::change(const std::string &uri, std::int32_t version, const std::vector<Protocol::TextDocumentContentChangeEvent> &changes) {
        const auto iterator = documents.find(uri);

        if (iterator == documents.end()) {
            return false;
        }

        Document &document = iterator->second;

        for (const Protocol::TextDocumentContentChangeEvent &change : changes) {
            applyChange(document.text, change);
        }

        document.version = version;
        return true;
    }

    bool DocumentManager::close(const std::string &uri) {
        return documents.erase(uri) > 0;
    }

    bool DocumentManager::contains(const std::string &uri) const {
        return documents.contains(uri);
    }

    const Document *DocumentManager::get(const std::string &uri) const {
        const auto iterator = documents.find(uri);
        return iterator == documents.end() ? nullptr : &iterator->second;
    }

    std::size_t DocumentManager::count() const {
        return documents.size();
    }
} // namespace Ryntra::LSP
