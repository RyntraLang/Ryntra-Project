// ========== Pipeline.h ============================================== *- C++ -* //
// Copyright (c) 2026 Remimwen Studio. All rights reserved.
// Part of the Ryntra Project. Licensed under Apache-2.0 License.
//
// Compilation pipeline glue: manifest + import graph + package-aware semantics +
// IR merge + bytecode. Used by the Riko CLI.
// ================================================================================ //

#pragma once

#include "ImportGraph.h"
#include "Manifest.h"

#include "IR/Module.h"
#include "Semantic/TypedAST.h"
#include "VM/Bytecode.h"
#include "VM/VMValue.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace Ryntra::BuildSystem {
    /// \brief Compiler options derived from the manifest / CLI.
    struct CompileOptions {
        bool strict = false;  // [CompileFlags] strict (reserved; behavior deferred)
        bool verbose = false; // -v
    };

    /// \brief Everything produced by compiling a target.
    struct CompileResult {
        bool ok = false;
        std::shared_ptr<Compiler::Semantic::TypedProgramNode> typedProgram;
        std::shared_ptr<IR::Module> module;
        std::vector<std::shared_ptr<VM::BytecodeFunction>> bytecode;
        std::vector<VM::VMValue> constantPool;
        std::string entrySymbol = "main";
    };

    /// \brief Run the full compilation pipeline for a target. Diagnostics are sent
    /// to the ErrorHandler (file-attributed); \c CompileResult::ok is false when any
    /// error was reported.
    CompileResult compileTarget(const Manifest &manifest, const Target &target,
                                const std::filesystem::path &manifestDir,
                                const CompileOptions &options = {});
} // namespace Ryntra::BuildSystem
