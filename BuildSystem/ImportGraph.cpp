// ========== ImportGraph.cpp ========================================= *- C++ -* //
// Copyright (c) 2026 Remimwen Studio. All rights reserved.
// Part of the Ryntra Project. Licensed under Apache-2.0 License.
// ================================================================================ //

#include "ImportGraph.h"

#include "AST/ASTBuilder.h"
#include "ErrorHandler/ErrorHandler.h"
#include "ErrorHandler/LexParseErrorHandler.h"

#include <antlr/RyntraLexer.h>
#include <antlr/RyntraParser.h>
#include <antlr4-runtime.h>

#include <fstream>
#include <map>
#include <set>

namespace Ryntra::BuildSystem {
    namespace {
        namespace Compiler = Ryntra::Compiler;

        bool errorHandlerHasErrors() {
            for (const auto &error : Compiler::ErrorHandler::getInstance().getErrorObjects()) {
                if (error.type == Compiler::kError) {
                    return true;
                }
            }
            return false;
        }

        size_t errorHandlerErrorCount() {
            size_t count = 0;
            for (const auto &error : Compiler::ErrorHandler::getInstance().getErrorObjects()) {
                if (error.type == Compiler::kError) {
                    ++count;
                }
            }
            return count;
        }

        std::filesystem::path normalized(const std::filesystem::path &path) {
            return std::filesystem::absolute(path).lexically_normal();
        }

        /**
         * @brief A path shown in diagnostics: relative to the manifest directory
         * when possible, otherwise just the file name.
         */
        std::string displayPath(const std::filesystem::path &path, const std::filesystem::path &manifestDir) {
            std::error_code ec;
            auto relative = std::filesystem::relative(path, manifestDir, ec);
            if (!ec && !relative.empty()) {
                const std::string text = relative.generic_string();
                if (text.rfind("..", 0) != 0) {
                    return text;
                }
            }
            return path.filename().string();
        }

        /**
         * @brief Depth-first walker that resolves a target's import graph.
         */
        struct GraphBuilder {
            std::filesystem::path packageRoot;
            std::filesystem::path manifestDir;
            std::set<std::string> visiting;
            std::set<std::string> visited;
            std::vector<std::string> stack; // normalized keys on the current path
            std::vector<Compiler::Semantic::TranslationUnit> units;
            bool ok = true;

            std::string display(const std::filesystem::path &path) const {
                return displayPath(path, manifestDir);
            }

            void visit(const std::filesystem::path &file, const std::string &importerDisplay) {
                const std::string key = normalized(file).string();
                if (visited.count(key)) {
                    return;
                }

                auto &handler = Compiler::ErrorHandler::getInstance();

                if (visiting.count(key)) {
                    handler.setCurrentFile(importerDisplay);
                    std::string chain;
                    bool started = false;
                    for (const auto &entry : stack) {
                        if (!started && entry != key) {
                            continue;
                        }
                        started = true;
                        if (!chain.empty()) {
                            chain += " -> ";
                        }
                        chain += display(std::filesystem::path(entry));
                    }
                    if (!chain.empty()) {
                        chain += " -> ";
                    }
                    chain += display(file);
                    handler.makeError("[RCE115]: Import cycle detected: " + chain + ".", Compiler::SourceRange{});
                    ok = false;
                    return;
                }

                const std::string selfDisplay = display(file);
                handler.setCurrentFile(selfDisplay);

                if (!std::filesystem::exists(file)) {
                    handler.makeError(
                        "[RCE116]: Cannot open imported package file '" + selfDisplay + "'.",
                        Compiler::SourceRange{});
                    ok = false;
                    return;
                }

                bool parsed = false;
                auto ast = parseSourceFile(file, &parsed);
                if (!parsed || !ast) {
                    ok = false;
                    return;
                }

                std::vector<std::string> importList;
                for (const auto &importDecl : ast->getImports()) {
                    if (importDecl && importDecl->getName()) {
                        importList.push_back(importDecl->getName()->getDottedName());
                    }
                }

                visiting.insert(key);
                stack.push_back(key);
                for (const auto &importDecl : ast->getImports()) {
                    if (!importDecl || !importDecl->getName()) {
                        continue;
                    }
                    const std::string dotted = importDecl->getName()->getDottedName();
                    const auto dependency = normalized(resolvePackageFile(packageRoot, dotted));
                    if (!std::filesystem::exists(dependency)) {
                        handler.setCurrentFile(selfDisplay);
                        handler.makeError(
                            "[RCE116]: Cannot open imported package '" + dotted + "' (expected '" +
                                display(dependency) + "').",
                            importDecl->getRange());
                        ok = false;
                        continue;
                    }
                    if (visiting.count(dependency.string())) {
                        handler.setCurrentFile(selfDisplay);
                        handler.makeError(
                            "[RCE115]: Import cycle detected: import of package '" + dotted +
                                "' forms a cycle.",
                            importDecl->getRange());
                        ok = false;
                        continue;
                    }
                    visit(dependency, selfDisplay);
                }
                stack.pop_back();
                visiting.erase(key);
                visited.insert(key);

                Compiler::Semantic::TranslationUnit unit;
                unit.filePath = selfDisplay;
                unit.packageName = derivePackageName(packageRoot, file);
                unit.imports = std::move(importList);
                unit.ast = ast;
                units.push_back(std::move(unit));
            }
        };
    } // namespace

    std::filesystem::path resolvePackageFile(const std::filesystem::path &packageRoot, const std::string &dotted) {
        std::filesystem::path result = packageRoot;
        size_t start = 0;
        while (start <= dotted.size()) {
            auto dot = dotted.find('.', start);
            if (dot == std::string::npos) {
                dot = dotted.size();
            }
            const std::string segment = dotted.substr(start, dot - start);
            if (!segment.empty()) {
                result /= segment;
            }
            if (dot == dotted.size()) {
                break;
            }
            start = dot + 1;
        }
        result += ".rynt";
        return result;
    }

    std::string derivePackageName(const std::filesystem::path &packageRoot, const std::filesystem::path &file) {
        std::error_code ec;
        auto relative = std::filesystem::relative(file, packageRoot, ec);
        if (ec || relative.empty()) {
            relative = file.filename();
        }
        std::string name = relative.generic_string();
        static constexpr std::string_view extension = ".rynt";
        if (name.size() >= extension.size() &&
            name.compare(name.size() - extension.size(), extension.size(), extension) == 0) {
            name.erase(name.size() - extension.size());
        }
        for (char &c : name) {
            if (c == '/' || c == '\\') {
                c = '.';
            }
        }
        return name;
    }

    std::shared_ptr<Compiler::ProgramNode> parseSourceFile(const std::filesystem::path &file, bool *okOut) {
        if (okOut) {
            *okOut = false;
        }

        std::ifstream input(file, std::ios::binary);
        if (!input.is_open()) {
            return nullptr;
        }
        const std::string source((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());

        antlr4::ANTLRInputStream antlrInput(source);
        Ryntra::antlr::RyntraLexer lexer(&antlrInput);
        antlr4::CommonTokenStream tokens(&lexer);
        tokens.fill();
        Ryntra::antlr::RyntraParser parser(&tokens);
        parser.removeErrorListeners();
        parser.addErrorListener(new Compiler::LexParseErrorHandler());

        const size_t errorsBefore = errorHandlerErrorCount();
        auto tree = parser.program();
        Compiler::ASTBuilder builder;
        auto ast = builder.visitProgram(tree);

        if (okOut) {
            *okOut = errorHandlerErrorCount() == errorsBefore;
        }
        return ast;
    }

    ImportGraph buildImportGraph(const Target &target, const std::filesystem::path &manifestDir) {
        ImportGraph graph;
        auto &handler = Compiler::ErrorHandler::getInstance();
        handler.setShowFileNames(true);

        GraphBuilder builder;
        builder.manifestDir = normalized(manifestDir);
        builder.packageRoot = normalized(manifestDir / target.packageRoot);

        const auto entryFile = normalized(manifestDir / target.entry);
        if (!std::filesystem::exists(entryFile)) {
            handler.setCurrentFile(target.entry);
            handler.makeError(
                "[RCE117]: Entry file '" + target.entry + "' was not found.", Compiler::SourceRange{});
            graph.ok = false;
            return graph;
        }

        builder.visit(entryFile, builder.display(entryFile));

        graph.units = std::move(builder.units);
        graph.entryPackage = derivePackageName(builder.packageRoot, entryFile);
        graph.ok = builder.ok && !errorHandlerHasErrors();
        return graph;
    }
} // namespace Ryntra::BuildSystem
