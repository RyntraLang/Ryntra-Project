#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace Ryntra::LSP {
    // Values match the LSP `CompletionItemKind` enumeration.
    enum class CompletionItemKind : std::int32_t {
        Text = 1,
        Method = 2,
        Function = 3,
        Constructor = 4,
        Field = 5,
        Variable = 6,
        Class = 7,
        Interface = 8,
        Module = 9,
        Property = 10,
        Unit = 11,
        Value = 12,
        Enum = 13,
        Keyword = 14,
        Snippet = 15,
        Color = 16,
        File = 17,
        Reference = 18,
        Folder = 19,
        EnumMember = 20,
        Constant = 21,
        Struct = 22,
        Event = 23,
        Operator = 24,
        TypeParameter = 25,
    };

    struct CompletionItem {
        std::string label;
        CompletionItemKind kind = CompletionItemKind::Text;
        std::string detail;
    };

    nlohmann::json serialize(const CompletionItem &item);

    nlohmann::json serialize(const std::vector<CompletionItem> &items);
} // namespace Ryntra::LSP
