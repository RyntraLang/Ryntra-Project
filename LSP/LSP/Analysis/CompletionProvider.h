#pragma once

#include "Analysis/Completion.h"
#include "Protocol/LSPTypes.h"

#include <string>
#include <vector>

namespace Ryntra::LSP {
    /// \brief Boundary for "completion" queries. The server knows only this interface.
    class CompletionProvider {
    public:
        virtual ~CompletionProvider() = default;

        [[nodiscard]] virtual std::vector<CompletionItem> completion(const std::string &uri, const std::string &text, const Protocol::Position &position) = 0;
    };
} // namespace Ryntra::LSP
