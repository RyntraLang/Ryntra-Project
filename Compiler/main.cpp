#include "AST/ASTBuilder.h"
#include "ArgParsing/ArgParsing.h"
#include "Driver/CompilePipeline.h"
#include "ErrorHandler/ErrorHandler.h"
#include "ErrorHandler/LexParseErrorHandler.h"
#include "ErrorHandler/RuntimeError.h"
#include "IR/IRGenerator.h"
#include "Semantic/SemanticAnalyzer.h"
#include "VM/BytecodeGenerator.h"
#include "VM/VirtualMachine.h"
#include <antlr/RyntraLexer.h>
#include <antlr/RyntraParser.h>
#include <antlr4-runtime.h>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace Ryntra;
using namespace Ryntra::Compiler;

// #define SHOW_LOG

namespace {
    bool errorHandlerHasErrors() {
        for (const auto &error : ErrorHandler::getInstance().getErrorObjects()) {
            if (error.type == kError) {
                return true;
            }
        }
        return false;
    }

    // Compile a single source file (the classic `rync <file>` mode). Diagnostics use
    // the file's name and the program entry point is `main`.
    int runSingleFile(const std::string &sourcePath, const bool emitIR, const bool emitBytecode,
                      const bool emitAST, const bool checkOnly) {
        std::ifstream sourceFile(sourcePath);
        if (!sourceFile.is_open()) {
            std::print(std::cerr, "Error: cannot open source file '{}'\n", sourcePath);
            return 1;
        }
        const std::string source((std::istreambuf_iterator<char>(sourceFile)), std::istreambuf_iterator<char>());

        // Attribute every diagnostic to the file being compiled and include the
        // file name in the printed output (e.g. `[Error]: "main.rynt": (1:2) ...`).
        ErrorHandler::getInstance().setShowFileNames(true);
        ErrorHandler::getInstance().setCurrentFile(std::filesystem::path(sourcePath).filename().string());

        antlr4::ANTLRInputStream input(source);
        Ryntra::antlr::RyntraLexer lexer(&input);
        antlr4::CommonTokenStream tokens(&lexer);
        tokens.fill();
        Ryntra::antlr::RyntraParser parser(&tokens);
        parser.removeErrorListeners();
        parser.addErrorListener(new LexParseErrorHandler());

        auto tree = parser.program();

        if (errorHandlerHasErrors()) {
            ErrorHandler::getInstance().print();
            return 1;
        }

        ASTBuilder builder;
        auto ast = builder.visitProgram(tree);

        if (emitAST) {
            std::cout << ast->toString() << std::endl;
            return 0;
        }

        Semantic::SemanticAnalyzer analyzer;
        analyzer.setCompilationMode(CompilationMode::CLI);
        analyzer.analyze(ast);

        ErrorHandler::getInstance().print();
        if (errorHandlerHasErrors()) {
            return 1;
        }

        auto typedAST = analyzer.getTypedAST();
        if (!typedAST) {
            return 1;
        }

        IR::IRGenerator irGen;
        auto module = irGen.generate(*typedAST, "HelloWorld");

        if (emitIR) {
            std::cout << module->toString() << std::endl;
            return 0;
        }

        VM::BytecodeGenerator bcGen;
        auto bytecode = bcGen.generate(module);

        if (emitBytecode) {
            VM::VirtualMachine dumpVM;
            dumpVM.load(bytecode, bcGen.getConstantPool());
            dumpVM.disassemble();
            return 0;
        }

        if (checkOnly) {
            return 0;
        }

        VM::VirtualMachine vm;
        vm.load(bytecode, bcGen.getConstantPool());
        vm.execute("main");
        return 0;
    }

    // Compile a whole program from its entry file, resolving imports under
    // `packageRoot` (multi-file / package mode). The entry point is the entry
    // package's `main` (e.g. `calc.main$main`).
    int runMultiFile(const std::filesystem::path &entryFile, const std::filesystem::path &packageRoot,
                     const bool emitIR, const bool emitBytecode, const bool checkOnly) {
        Driver::CompileResult result = Driver::compileProgram(entryFile, packageRoot, "module");

        ErrorHandler::getInstance().print();
        if (!result.ok) {
            return 1;
        }

        if (emitIR) {
            std::cout << result.module->toString() << std::endl;
            return 0;
        }
        if (emitBytecode) {
            VM::VirtualMachine dumpVM;
            dumpVM.load(result.bytecode, result.constantPool);
            dumpVM.disassemble();
            return 0;
        }
        if (checkOnly) {
            return 0;
        }

        VM::VirtualMachine vm;
        vm.load(result.bytecode, result.constantPool);
        vm.execute(result.entrySymbol);
        return 0;
    }
} // namespace

int main(int argc, char **argv) {
    using namespace Ryntra::Utility;
    try {
        ArgParser argParser(argc, argv);
        auto packageRootOpt = argParser.option<std::string>("--package-root");
        auto entryOpt = argParser.option<std::string>("--entry");
        auto emitIR = argParser.flag("--emit-ir");
        auto emitBytecode = argParser.flag("--emit-bytecode");
        auto emitAST = argParser.flag("--emit-ast");
        auto checkOnly = argParser.flag("--check");

        if (!argParser.parse()) {
            return 1;
        }

        const auto &positional = argParser.positional();
        const bool multiFile = entryOpt.isSet() || packageRootOpt.isSet();

        if (multiFile) {
            const std::string entry = entryOpt.value_or(positional.empty() ? std::string{} : positional.front());
            if (entry.empty()) {
                std::print(std::cerr, "Error: --entry <file> is required for multi-file compilation.\n");
                return 1;
            }
            const std::filesystem::path entryFile = entry;
            const std::filesystem::path packageRoot = packageRootOpt.isSet()
                                                           ? std::filesystem::path(packageRootOpt.get())
                                                           : entryFile.parent_path();
            return runMultiFile(entryFile, packageRoot, emitIR.get(), emitBytecode.get(), checkOnly.get());
        }

        if (positional.empty()) {
            std::print(std::cerr,
                       "Usage: {} <source-file> [--emit-ast] [--emit-ir] [--emit-bytecode] [--check]\n"
                       "       {} --package-root <dir> --entry <file> [--emit-ir] [--emit-bytecode] [--check]\n",
                       argv[0], argv[0]);
            return 1;
        }

        return runSingleFile(positional.front(), emitIR.get(), emitBytecode.get(), emitAST.get(), checkOnly.get());
    } catch (const RuntimeErrorException &e) {
        ErrorHandler &handler = ErrorHandler::getInstance();
        handler.makeError(e.getDescription(), e.getRange());
        for (const auto &frame : e.getTrace()) {
            handler.makeHint("Call from '" + frame.callerName + "'", frame.callSite);
        }
        handler.print();
        return 1;
    } catch (const std::exception &e) {
        std::print(std::cerr, "Error: {}\n", e.what());
        return 1;
    }
}
