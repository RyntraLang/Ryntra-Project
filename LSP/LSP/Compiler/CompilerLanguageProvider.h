#pragma once

#include "Analysis/DefinitionProvider.h"
#include "Analysis/HoverProvider.h"
#include "Diagnostics/DiagnosticsProvider.h"

namespace Ryntra::LSP {
    /// \brief Language features backed by the Ryntra compiler front-end.
    ///
    /// This is the only translation unit that understands the compiler types, which
    /// keeps the rest of the language server front-end agnostic.
    class CompilerLanguageProvider final : public DiagnosticsProvider, public HoverProvider, public DefinitionProvider {
    public:
        std::vector<Diagnostic> analyze(const std::string &uri, const std::string &text) override;

        std::optional<Hover> hover(const std::string &uri, const std::string &text, const Protocol::Position &position) override;

        std::optional<Location> definition(const std::string &uri, const std::string &text, const Protocol::Position &position) override;
    };
} // namespace Ryntra::LSP
