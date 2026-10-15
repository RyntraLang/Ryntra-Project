// ========== ImportGraph.h =========================================== *- C++ -* //
// Copyright (c) 2026 Remimwen Studio. All rights reserved.
// Part of the Ryntra Project. Licensed under Apache-2.0 License.
//
// Resolves a target's entry file into the whole import graph (translation units)
// by following `import` declarations, with deduplication and cycle detection.
// ================================================================================ //

#pragma once

#include "Manifest.h"
#include "Semantic/ProgramSet.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace Ryntra::BuildSystem {
    /// \brief The result of walking a target's import graph.
    struct ImportGraph {
        // Translation units in a deterministic topological order (a unit appears
        // after every package it imports).
        std::vector<Compiler::Semantic::TranslationUnit> units;
        // Derived package name of the entry unit.
        std::string entryPackage;
        bool ok = true;
    };

    /// \brief Map a dotted import path (e.g. "utility.dataStructure.ForwardList")
    /// to a source file under \p packageRoot, one segment per path component.
    std::filesystem::path resolvePackageFile(const std::filesystem::path &packageRoot, const std::string &dotted);

    /// \brief Derive a package name from a source file path relative to
    /// \p packageRoot (separators and extension removed, joined with '.').
    std::string derivePackageName(const std::filesystem::path &packageRoot, const std::filesystem::path &file);

    /// \brief Parse a source file into an AST. \p okOut (optional) is set to false on
    /// any lex/parse error. Callers should set the current file on the ErrorHandler
    /// first so diagnostics are attributed.
    std::shared_ptr<Compiler::ProgramNode> parseSourceFile(const std::filesystem::path &file, bool *okOut = nullptr);

    /// \brief Walk the import graph for \p target starting at its entry file.
    /// Diagnostics (missing files, cycles, parse errors) are reported through the
    /// ErrorHandler with file attribution; \c ImportGraph::ok is false on failure.
    ImportGraph buildImportGraph(const Target &target, const std::filesystem::path &manifestDir);
} // namespace Ryntra::BuildSystem
