#include "DocumentManager.h"

#include "Text/TextOffset.h"

namespace Ryntra::LSP {
    namespace {
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
