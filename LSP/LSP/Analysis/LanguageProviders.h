#pragma once

#include "Analysis/CompletionProvider.h"
#include "Analysis/DefinitionProvider.h"
#include "Analysis/DocumentSymbolProvider.h"
#include "Analysis/HoverProvider.h"
#include "Diagnostics/DiagnosticsProvider.h"

namespace Ryntra::LSP {
    /// \brief The language features the server exposes, all behind abstractions so
    /// the server never depends on a concrete (compiler-backed) implementation.
    struct LanguageProviders {
        DiagnosticsProvider *diagnostics = nullptr;
        HoverProvider *hover = nullptr;
        DefinitionProvider *definition = nullptr;
        DocumentSymbolProvider *documentSymbols = nullptr;
        CompletionProvider *completion = nullptr;
    };
} // namespace Ryntra::LSP
