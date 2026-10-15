// ========== Manifest.cpp ============================================ *- C++ -* //
// Copyright (c) 2026 Remimwen Studio. All rights reserved.
// Part of the Ryntra Project. Licensed under Apache-2.0 License.
// ================================================================================ //

#include "Manifest.h"

#include <sstream>
#include <stdexcept>
#include <toml++/toml.hpp>

namespace Ryntra::BuildSystem {
    Manifest loadManifest(const std::filesystem::path &manifestPath) {
        toml::table table;
        try {
            table = toml::parse_file(manifestPath.string());
        } catch (const toml::parse_error &e) {
            std::ostringstream oss;
            oss << "Failed to parse manifest '" << manifestPath.string() << "': " << e.description();
            throw std::runtime_error(oss.str());
        }

        Manifest manifest;

        if (const auto *project = table["Project"].as_table()) {
            manifest.projectName = (*project)["name"].value_or(std::string{});
            manifest.version = (*project)["version"].value_or(std::string{});
        }

        if (const auto *targets = table["Targets"].as_table()) {
            for (const auto &[key, node] : *targets) {
                const auto *targetTable = node.as_table();
                if (!targetTable) {
                    continue;
                }
                Target target;
                target.name = std::string(key.str());
                target.entry = (*targetTable)["entry"].value_or(std::string{});
                target.packageRoot = (*targetTable)["package_root"].value_or(std::string{"src"});
                if (target.entry.empty()) {
                    throw std::runtime_error(
                        "Target '" + target.name + "' is missing the required key 'entry'.");
                }
                manifest.targets[target.name] = target;
            }
        }

        if (const auto *flags = table["CompileFlags"].as_table()) {
            manifest.strict = (*flags)["strict"].value_or(false);
        }

        return manifest;
    }
} // namespace Ryntra::BuildSystem
