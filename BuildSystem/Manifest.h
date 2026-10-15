// ========== Manifest.h ============================================== *- C++ -* //
// Copyright (c) 2026 Remimwen Studio. All rights reserved.
// Part of the Ryntra Project. Licensed under Apache-2.0 License.
//
// Riko.toml manifest model and loader (backed by toml++).
// ================================================================================ //

#pragma once

#include <filesystem>
#include <map>
#include <string>

namespace Ryntra::BuildSystem {
    /// \brief A build target: its name, entry source file and package root.
    struct Target {
        std::string name;
        std::string entry;              // relative to the manifest directory
        std::string packageRoot = "src"; // relative to the manifest directory
    };

    /// \brief Parsed Riko.toml.
    struct Manifest {
        std::string projectName;
        std::string version;
        std::map<std::string, Target> targets;
        bool strict = false; // [CompileFlags] strict
    };

    /// \brief Load and validate a Riko.toml manifest.
    /// \param manifestPath Path to the manifest file.
    /// \return The parsed manifest.
    /// \throws std::runtime_error when the file is missing, malformed, or a target
    /// is missing its required `entry` key.
    Manifest loadManifest(const std::filesystem::path &manifestPath);
} // namespace Ryntra::BuildSystem
