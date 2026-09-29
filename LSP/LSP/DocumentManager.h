#pragma once

#include "Protocol/TextDocument.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Ryntra::LSP {
    struct Document {
        std::string uri;
        std::string languageId;
        std::int32_t version = 0;
        std::string text;
    };

    class DocumentManager {
    public:
        void open(const Document &document);

        bool change(const std::string &uri, std::int32_t version, const std::vector<Protocol::TextDocumentContentChangeEvent> &changes);

        bool close(const std::string &uri);

        [[nodiscard]] bool contains(const std::string &uri) const;

        [[nodiscard]] const Document *get(const std::string &uri) const;

        [[nodiscard]] std::size_t count() const;

    private:
        std::unordered_map<std::string, Document> documents;
    };
} // namespace Ryntra::LSP
