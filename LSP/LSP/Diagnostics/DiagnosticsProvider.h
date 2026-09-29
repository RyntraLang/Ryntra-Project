#pragma once

#include "Diagnostics/Diagnostic.h"

#include <string>
#include <vector>

namespace Ryntra::LSP {
    /// \brief Boundary between the language server and whatever produces diagnostics.
    ///
    /// The server only knows this interface, so it stays decoupled from the compiler
    /// front-end. A concrete provider (for example the Ryntra compiler front-end) is
    /// injected by the composition root.
    class DiagnosticsProvider {
    public:
        virtual ~DiagnosticsProvider() = default;

        /// \brief Analyse a document and return the diagnostics for it.
        /// \param uri The document URI (context for providers that need it)
        /// \param text The current document text
        /// \return The diagnostics, ordered by position
        [[nodiscard]] virtual std::vector<Diagnostic> analyze(const std::string &uri, const std::string &text) = 0;
    };
} // namespace Ryntra::LSP
