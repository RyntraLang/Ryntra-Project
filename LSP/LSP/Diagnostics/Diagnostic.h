#pragma once

#include "Protocol/LSPTypes.h"

#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace Ryntra::LSP {
    enum class DiagnosticSeverity : std::int32_t {
        Error = 1,
        Warning = 2,
        Information = 3,
        Hint = 4,
    };

    struct Diagnostic {
        Protocol::Range range;
        DiagnosticSeverity severity = DiagnosticSeverity::Error;
        std::string message;
        std::string source;
    };

    struct PublishDiagnosticsParams {
        std::string uri;
        std::vector<Diagnostic> diagnostics;
    };

    nlohmann::json serialize(const Diagnostic &diagnostic);

    nlohmann::json serialize(const PublishDiagnosticsParams &params);
} // namespace Ryntra::LSP
