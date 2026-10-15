// ========== Pipeline.cpp ============================================ *- C++ -* //
// Copyright (c) 2026 Remimwen Studio. All rights reserved.
// Part of the Ryntra Project. Licensed under Apache-2.0 License.
// ================================================================================ //

#include "Pipeline.h"

#include "ErrorHandler/ErrorHandler.h"
#include "IR/IRGenerator.h"
#include "Semantic/SemanticAnalyzer.h"
#include "VM/BytecodeGenerator.h"

namespace Ryntra::BuildSystem {
    namespace {
        bool errorHandlerHasErrors() {
            for (const auto &error : Compiler::ErrorHandler::getInstance().getErrorObjects()) {
                if (error.type == Compiler::kError) {
                    return true;
                }
            }
            return false;
        }
    } // namespace

    CompileResult compileTarget(const Manifest &manifest, const Target &target,
                                const std::filesystem::path &manifestDir,
                                const CompileOptions &options) {
        (void)manifest;
        (void)options;

        CompileResult result;
        auto &handler = Compiler::ErrorHandler::getInstance();
        handler.setShowFileNames(true);

        // 1. Resolve the import graph.
        auto graph = buildImportGraph(target, manifestDir);
        if (!graph.ok) {
            return result;
        }

        // 2. Package-aware semantic analysis across all units.
        Compiler::Semantic::ProgramSet programSet;
        programSet.units = std::move(graph.units);
        programSet.entryPackage = graph.entryPackage;

        Compiler::Semantic::SemanticAnalyzer analyzer;
        analyzer.setCompilationMode(Compiler::CompilationMode::CLI);
        analyzer.analyzeProgramSet(programSet);

        if (errorHandlerHasErrors()) {
            return result;
        }

        result.typedProgram = analyzer.getTypedAST();
        if (!result.typedProgram) {
            return result;
        }

        // 3. Merge into a single IR module with package-qualified names.
        result.entrySymbol = graph.entryPackage.empty() ? "main" : graph.entryPackage + "$main";
        IR::IRGenerator irGenerator;
        irGenerator.setEntrySymbolName(result.entrySymbol);
        result.module = irGenerator.generateInto(nullptr, *result.typedProgram);

        if (errorHandlerHasErrors()) {
            return result;
        }

        // 4. Lower to bytecode.
        VM::BytecodeGenerator bytecodeGenerator;
        result.bytecode = bytecodeGenerator.generate(result.module);
        result.constantPool = bytecodeGenerator.getConstantPool();

        result.ok = true;
        return result;
    }
} // namespace Ryntra::BuildSystem
