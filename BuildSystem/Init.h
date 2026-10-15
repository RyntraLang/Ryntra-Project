// ========== Init.h ================================================== *- C++ -* //
// Copyright (c) 2026 Remimwen Studio. All rights reserved.
// Part of the Ryntra Project. Licensed under Apache-2.0 License.
//
// `riko init <path>` scaffolding: create a Riko.toml manifest and a starter
// src/main.rynt.
// ================================================================================ //

#pragma once

#include <filesystem>
#include <string>

namespace Ryntra::BuildSystem {
    /// \brief Outcome of scaffolding a project.
    struct InitResult {
        bool ok = false;
        std::filesystem::path projectDir;
        std::string projectName;
        std::string targetName;
        /// \brief Success summary when \c ok, otherwise the error message.
        std::string message;
    };

    /// \brief Scaffold a new Ryntra project at \p path: write `Riko.toml` and
    /// `src/main.rynt`. Relative paths (including ".") are resolved against the
    /// current working directory; missing directories are created. Existing
    /// `Riko.toml` / `src/main.rynt` are never overwritten.
    InitResult initializeProject(const std::filesystem::path &path);
} // namespace Ryntra::BuildSystem
