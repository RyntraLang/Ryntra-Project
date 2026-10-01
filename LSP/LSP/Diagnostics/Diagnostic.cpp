#include "Diagnostics/Diagnostic.h"

#include <utility>

namespace Ryntra::LSP {
    nlohmann::json serialize(const Diagnostic &diagnostic) {
        return {
            {"range", Protocol::serialize(diagnostic.range)},
            {"severity", static_cast<std::int32_t>(diagnostic.severity)},
            {"source", diagnostic.source},
            {"message", diagnostic.message},
        };
    }

    nlohmann::json serialize(const PublishDiagnosticsParams &params) {
        nlohmann::json diagnostics = nlohmann::json::array();

        for (const Diagnostic &diagnostic : params.diagnostics) {
            diagnostics.push_back(serialize(diagnostic));
        }

        return {
            {"uri", params.uri},
            {"diagnostics", std::move(diagnostics)},
        };
    }
} // namespace Ryntra::LSP
