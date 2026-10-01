// ========== CompilationMode.h ============================== *- C++ -* ========== //
// Copyright (c) 2026 Remimwen Studio. All rights reserved.
// Part of the Ryntra Project. Licensed under Apache-2.0 License.
// ================================================================================ //

#pragma once

namespace Ryntra::Compiler {
    /**
     * @brief How the front-end is being driven, which selects the set of diagnostics
     * that apply. This is intentionally a compile-time-facing switch pushed into
     * the frontend, so consumers (CLI, language server etc.) never post-filter diagnostics.
     */
    enum class CompilationMode {
        /**
         * Full compilation to an executable: every check applies, including
         * program-entry checks such as "`main` function is not defined."
         */
        CLI,
        /**
         * Editor / language server analysis: program entry checks that only matter
         * when producing an executable (e.g. a missing `main`) are suppressed.
         */
        Editor,
    };
} // namespace Ryntra::Compiler
