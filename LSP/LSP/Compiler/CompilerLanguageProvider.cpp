#include "Compiler/CompilerLanguageProvider.h"

#include "AST/ASTBuilder.h"
#include "CompilationMode.h"
#include "ErrorHandler/ErrorHandler.h"
#include "ErrorHandler/LexParseErrorHandler.h"
#include "Semantic/SemanticAnalyzer.h"
#include "Text/TextOffset.h"

#include <antlr/RyntraLexer.h>
#include <antlr/RyntraParser.h>
#include <antlr4-runtime.h>

#include <algorithm>
#include <cctype>
#include <memory>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace Ryntra::LSP {
    namespace {
        using Compiler::Semantic::SymbolKind;

        std::uint32_t toZeroBasedLine(const std::uint32_t line) {
            return line == 0 ? 0 : line - 1;
        }

        Protocol::Range toProtocolRange(const Compiler::SourceRange &range) {
            Protocol::Range result;
            result.start.line = toZeroBasedLine(range.begin.line);
            result.start.character = range.begin.column;
            result.end.line = toZeroBasedLine(range.end.line);
            result.end.character = range.end.column;

            // A point diagnostic has no width; give clients a single character to
            // highlight instead of nothing.
            if (result.start.line == result.end.line && result.start.character == result.end.character) {
                result.end.character += 1;
            }

            return result;
        }

        DiagnosticSeverity toSeverity(const Compiler::ErrorType type) {
            switch (type) {
                case Compiler::kError:
                    return DiagnosticSeverity::Error;
                case Compiler::kWarning:
                    return DiagnosticSeverity::Warning;
                case Compiler::kHint:
                    return DiagnosticSeverity::Hint;
            }

            return DiagnosticSeverity::Error;
        }

        Diagnostic toDiagnostic(const Compiler::ErrorObject &error) {
            Diagnostic diagnostic;
            diagnostic.range = toProtocolRange(error.range);
            diagnostic.severity = toSeverity(error.type);
            diagnostic.message = error.description;
            diagnostic.source = "ryntra";
            return diagnostic;
        }

        std::vector<Diagnostic> convert(const std::vector<Compiler::ErrorObject> &errors) {
            std::vector<Diagnostic> diagnostics;
            diagnostics.reserve(errors.size());

            for (const Compiler::ErrorObject &error : errors) {
                diagnostics.push_back(toDiagnostic(error));
            }

            return diagnostics;
        }

        std::string describeType(const Compiler::Semantic::STType::Type &type) {
            using Compiler::Semantic::STType::TypeKind;

            switch (type.getKind()) {
                case TypeKind::Void:
                    return "void";
                case TypeKind::Bool:
                    return "bool";
                case TypeKind::Char:
                    return "char";
                case TypeKind::Int8:
                    return "int8";
                case TypeKind::Int16:
                    return "int16";
                case TypeKind::Int32:
                    return "int";
                case TypeKind::Int64:
                    return "long";
                case TypeKind::UnsignedInt8:
                    return "uint8";
                case TypeKind::UnsignedInt16:
                    return "uint16";
                case TypeKind::UnsignedInt32:
                    return "uint32";
                case TypeKind::UnsignedInt64:
                    return "uint64";
                case TypeKind::Float32:
                    return "float";
                case TypeKind::Float64:
                    return "double";
                case TypeKind::Float128:
                    return "decimal";
                case TypeKind::String:
                    return "string";
                case TypeKind::Array:
                    return describeType(*static_cast<const Compiler::Semantic::STType::ArrayType &>(type).getElementType()) + "[]";
                case TypeKind::Reference:
                    return "ref<" + describeType(*static_cast<const Compiler::Semantic::STType::ReferenceType &>(type).getElementType()) + ">";
                case TypeKind::Pointer:
                    return "ptr<" + describeType(*static_cast<const Compiler::Semantic::STType::PointerType &>(type).getElementType()) + ">";
                case TypeKind::Function: {
                    const auto &function = static_cast<const Compiler::Semantic::STType::FunctionType &>(type);
                    std::string result = "fn(";
                    const std::vector<Compiler::Semantic::TypePtr> &params = function.getParamTypes();

                    for (std::size_t index = 0; index < params.size(); ++index) {
                        if (index > 0) {
                            result += ", ";
                        }
                        result += describeType(*params[index]);
                    }

                    return result + ") -> " + describeType(*function.getReturnType());
                }
                case TypeKind::Struct:
                    return static_cast<const Compiler::Semantic::STType::StructType &>(type).getName();
            }

            return "unknown";
        }

        std::string describeFunction(const Compiler::Semantic::FunctionSymbol &function) {
            std::string result = describeType(*function.getReturnType()) + " " + function.getName() + "(";
            const std::vector<Compiler::Semantic::TypePtr> &params = function.getParamTypes();

            for (std::size_t index = 0; index < params.size(); ++index) {
                if (index > 0) {
                    result += ", ";
                }
                result += describeType(*params[index]);
            }

            return result + ")";
        }

        std::string describeConstructorSignature(const Compiler::Semantic::FunctionSymbol &constructor) {
            std::string result = constructor.getName() + "(";
            const std::vector<Compiler::Semantic::TypePtr> &params = constructor.getParamTypes();

            for (std::size_t index = 0; index < params.size(); ++index) {
                if (index > 0) {
                    result += ", ";
                }
                result += describeType(*params[index]);
            }

            return result + ")";
        }

        std::string describeConstructor(const Compiler::Semantic::Symbol &symbol) {
            std::string text = "**(constructor)** `" + symbol.getName() + "`\n\n```ryntra\n";

            if (symbol.getKind() == SymbolKind::OverloadSet) {
                const auto &overloads = static_cast<const Compiler::Semantic::OverloadSet &>(symbol);

                for (const auto &function : overloads.getFunctions()) {
                    text += describeConstructorSignature(*function) + "\n";
                }
            } else if (symbol.getKind() == SymbolKind::Function) {
                text += describeConstructorSignature(static_cast<const Compiler::Semantic::FunctionSymbol &>(symbol)) + "\n";
            }

            return text + "```";
        }

        std::string describeSymbol(const Compiler::Semantic::Symbol &symbol) {
            switch (symbol.getKind()) {
                case SymbolKind::Variable: {
                    const auto &variable = static_cast<const Compiler::Semantic::VariableSymbol &>(symbol);
                    const std::string label = variable.isParameter() ? "parameter" : "variable";
                    return "**(" + label + ")** `" + symbol.getName() + "`\n\n```ryntra\n" + describeType(*variable.getType()) + "\n```";
                }
                case SymbolKind::Field: {
                    const auto &field = static_cast<const Compiler::Semantic::FieldSymbol &>(symbol);
                    return "**(field)** `" + symbol.getName() + "`\n\n```ryntra\n" + describeType(*field.getType()) + "\n```";
                }
                case SymbolKind::Function:
                    return "**(function)**\n\n```ryntra\n" + describeFunction(static_cast<const Compiler::Semantic::FunctionSymbol &>(symbol)) + "\n```";
                case SymbolKind::OverloadSet: {
                    const auto &overloads = static_cast<const Compiler::Semantic::OverloadSet &>(symbol);
                    std::string text = "**(function)** `" + symbol.getName() + "`\n\n```ryntra\n";

                    for (const auto &function : overloads.getFunctions()) {
                        text += describeFunction(*function) + "\n";
                    }

                    return text + "```";
                }
                case SymbolKind::Type: {
                    const auto &type = static_cast<const Compiler::Semantic::TypeSymbol &>(symbol);
                    return "**(type)** `" + describeType(*type.getType()) + "`";
                }
            }

            return symbol.getName();
        }

        struct MemberDeclaration {
            std::string name;
            Compiler::SourceRange range;
            std::shared_ptr<Compiler::Semantic::Symbol> symbol;
            bool isConstructor = false;
        };

        struct SymbolIndex {
            std::vector<Compiler::Semantic::SymbolDefinition> globals;
            std::vector<MemberDeclaration> members;
        };

        // Members live in their own StructType member scope, not in the SymbolTable
        // index, so collect them through the struct types referenced by type symbols.
        std::vector<MemberDeclaration> collectMembers(const std::vector<Compiler::Semantic::SymbolDefinition> &definitions) {
            std::vector<MemberDeclaration> members;

            for (const Compiler::Semantic::SymbolDefinition &definition : definitions) {
                if (definition.symbol == nullptr || definition.symbol->getKind() != SymbolKind::Type) {
                    continue;
                }

                const auto &typeSymbol = static_cast<const Compiler::Semantic::TypeSymbol &>(*definition.symbol);
                const auto structType = std::dynamic_pointer_cast<Compiler::Semantic::STType::StructType>(typeSymbol.getType());

                if (structType == nullptr) {
                    continue;
                }

                const auto &scope = structType->getMemberScope();

                for (const auto &[name, range] : structType->getMemberDeclarations()) {
                    const auto iterator = scope.symbols.find(name);

                    if (iterator == scope.symbols.end()) {
                        continue;
                    }

                    // A member whose name equals the struct name is a constructor.
                    const bool isConstructor = name == structType->getName();
                    members.push_back({name, range, iterator->second, isConstructor});
                }
            }

            return members;
        }

        std::optional<SymbolIndex> analyzeSymbols(const std::string &text) {
            Compiler::ErrorHandler &errorHandler = Compiler::ErrorHandler::getInstance();
            errorHandler.clear();

            try {
                Compiler::LexParseErrorHandler parseErrorHandler;

                antlr4::ANTLRInputStream input(text);
                Ryntra::antlr::RyntraLexer lexer(&input);
                antlr4::CommonTokenStream tokens(&lexer);
                tokens.fill();

                Ryntra::antlr::RyntraParser parser(&tokens);
                lexer.removeErrorListeners();
                lexer.addErrorListener(&parseErrorHandler);
                parser.removeErrorListeners();
                parser.addErrorListener(&parseErrorHandler);

                Ryntra::antlr::RyntraParser::ProgramContext *tree = parser.program();

                if (tree == nullptr) {
                    return std::nullopt;
                }

                for (const Compiler::ErrorObject &error : errorHandler.getErrorObjects()) {
                    if (error.type == Compiler::kError) {
                        return std::nullopt;
                    }
                }

                Compiler::ASTBuilder builder;
                const std::shared_ptr<Compiler::ProgramNode> program = builder.visitProgram(tree);

                Compiler::Semantic::SemanticAnalyzer analyzer;
                analyzer.setCompilationMode(Compiler::CompilationMode::Editor);
                analyzer.analyze(program);

                SymbolIndex index;
                index.globals = analyzer.getDefinitions();
                index.members = collectMembers(index.globals);
                return index;
            } catch (const std::exception &) {
                return std::nullopt;
            }
        }

        template <typename Entry>
        const Entry *closestByOffset(const std::vector<Entry> &entries, const std::string &name, const std::size_t offset) {
            const Entry *best = nullptr;

            for (const Entry &entry : entries) {
                if (entry.name != name || entry.range.begin.offset > offset) {
                    continue;
                }

                if (best == nullptr || entry.range.begin.offset >= best->range.begin.offset) {
                    best = &entry;
                }
            }

            if (best != nullptr) {
                return best;
            }

            for (const Entry &entry : entries) {
                if (entry.name == name) {
                    return &entry;
                }
            }

            return nullptr;
        }

        struct ResolvedSymbol {
            std::shared_ptr<Compiler::Semantic::Symbol> symbol;
            Compiler::SourceRange range;
            bool isConstructor = false;
        };

        std::optional<ResolvedSymbol> resolveSymbol(const SymbolIndex &index, const std::string &name, const std::size_t offset, const bool memberContext, const bool callContext) {
            const auto *global = closestByOffset(index.globals, name, offset);
            const auto *member = closestByOffset(index.members, name, offset);

            const bool globalIsType = global != nullptr && global->symbol != nullptr && global->symbol->getKind() == SymbolKind::Type;

            // Member access (`obj.field`) prefers the member namespace. A call whose
            // global match is only a type (`Rectangle(...)`) prefers the matching
            // constructor over the struct declaration.
            const bool preferMember = memberContext || (callContext && globalIsType);

            const Compiler::Semantic::SymbolDefinition *chosenGlobal = preferMember ? nullptr : global;
            const MemberDeclaration *chosenMember = preferMember ? member : nullptr;

            // Fall back to the other namespace when the preferred one has no such name.
            if (chosenGlobal == nullptr && chosenMember == nullptr) {
                chosenGlobal = global;
                chosenMember = member;
            }

            if (chosenGlobal != nullptr) {
                return ResolvedSymbol{chosenGlobal->symbol, chosenGlobal->range, false};
            }

            if (chosenMember != nullptr) {
                return ResolvedSymbol{chosenMember->symbol, chosenMember->range, chosenMember->isConstructor};
            }

            return std::nullopt;
        }

        std::optional<ResolvedSymbol> resolveAt(const SymbolIndex &index, const std::string &text, const IdentifierSpan &identifier, const Protocol::Position &position) {
            const std::size_t start = offsetAt(text, identifier.range.start);
            const std::size_t end = offsetAt(text, identifier.range.end);

            const bool memberContext = start > 0 && text[start - 1] == '.';

            std::size_t next = end;
            while (next < text.size() && (text[next] == ' ' || text[next] == '\t')) {
                ++next;
            }

            const bool callContext = next < text.size() && text[next] == '(';
            return resolveSymbol(index, identifier.name, offsetAt(text, position), memberContext, callContext);
        }

        bool isMemberAccess(const std::string &text, const std::size_t offset) {
            std::size_t index = std::min(offset, text.size());

            while (index > 0) {
                const unsigned char byte = static_cast<unsigned char>(text[index - 1]);

                if (std::isalnum(byte) != 0 || byte == '_') {
                    --index;
                } else {
                    break;
                }
            }

            while (index > 0 && (text[index - 1] == ' ' || text[index - 1] == '\t')) {
                --index;
            }

            return index > 0 && text[index - 1] == '.';
        }

        struct SanitizedSource {
            std::string text;
            std::size_t offset = 0;
        };

        // Completion is requested while the user is mid-typing, so the document can be
        // momentarily invalid (e.g. `rect.`). Replace the incomplete member name with a
        // placeholder so the front-end can still parse and index the rest of the file.
        SanitizedSource sanitizeForCompletion(const std::string &text, const Protocol::Position &position) {
            const std::size_t offset = std::min(offsetAt(text, position), text.size());

            std::size_t start = offset;
            while (start > 0) {
                const unsigned char byte = static_cast<unsigned char>(text[start - 1]);

                if (std::isalnum(byte) != 0 || byte == '_') {
                    --start;
                } else {
                    break;
                }
            }

            std::size_t end = offset;
            while (end < text.size()) {
                const unsigned char byte = static_cast<unsigned char>(text[end]);

                if (std::isalnum(byte) != 0 || byte == '_') {
                    ++end;
                } else {
                    break;
                }
            }

            std::size_t before = start;
            while (before > 0 && (text[before - 1] == ' ' || text[before - 1] == '\t')) {
                --before;
            }

            if (before == 0 || text[before - 1] != '.') {
                return {text, offset};
            }

            static const std::string placeholder = "__completion";

            std::string result = text;
            result.replace(start, end - start, placeholder);

            const std::size_t after = start + placeholder.size();

            // The statement is also likely unterminated while typing (e.g. `r.`), so
            // make sure it ends with a semicolon before the end of the line.
            std::size_t lineEnd = after;
            while (lineEnd < result.size() && result[lineEnd] != '\n') {
                ++lineEnd;
            }

            bool terminated = false;
            for (std::size_t index = after; index < lineEnd; ++index) {
                if (result[index] == ';') {
                    terminated = true;
                    break;
                }
            }

            if (!terminated) {
                result.insert(after, ";");
            }

            return {std::move(result), after};
        }

        std::vector<DocumentSymbol> describeMembers(const Compiler::Semantic::STType::StructType &structType) {
            std::vector<DocumentSymbol> children;
            const auto &scope = structType.getMemberScope();

            for (const auto &[name, range] : structType.getMemberDeclarations()) {
                const auto iterator = scope.symbols.find(name);

                if (iterator == scope.symbols.end()) {
                    continue;
                }

                DocumentSymbol child;
                child.name = name;
                child.kind = name == structType.getName()
                                 ? DocumentSymbolKind::Constructor
                                 : (iterator->second->getKind() == SymbolKind::Field ? DocumentSymbolKind::Field : DocumentSymbolKind::Method);
                child.range = toProtocolRange(range);
                child.selectionRange = child.range;
                children.push_back(std::move(child));
            }

            return children;
        }

        bool containsOffset(const Compiler::SourceRange &range, const std::size_t offset) {
            return offset >= range.begin.offset && offset <= range.end.offset;
        }

        bool isVisible(const Compiler::Semantic::SymbolDefinition &definition, const std::size_t offset) {
            return definition.global || containsOffset(definition.scopeRange, offset);
        }

        const Compiler::Semantic::STType::StructType *asStruct(const Compiler::Semantic::STType::Type *type) {
            while (type != nullptr) {
                switch (type->getKind()) {
                    case Compiler::Semantic::STType::TypeKind::Struct:
                        return static_cast<const Compiler::Semantic::STType::StructType *>(type);
                    case Compiler::Semantic::STType::TypeKind::Pointer:
                        type = static_cast<const Compiler::Semantic::STType::PointerType *>(type)->getElementType().get();
                        break;
                    case Compiler::Semantic::STType::TypeKind::Reference:
                        type = static_cast<const Compiler::Semantic::STType::ReferenceType *>(type)->getElementType().get();
                        break;
                    default:
                        return nullptr;
                }
            }

            return nullptr;
        }

        std::optional<std::string> memberReceiverName(const std::string &text, const std::size_t offset) {
            std::size_t index = std::min(offset, text.size());

            while (index > 0) {
                const unsigned char byte = static_cast<unsigned char>(text[index - 1]);

                if (std::isalnum(byte) != 0 || byte == '_') {
                    --index;
                } else {
                    break;
                }
            }

            while (index > 0 && (text[index - 1] == ' ' || text[index - 1] == '\t')) {
                --index;
            }

            if (index == 0 || text[index - 1] != '.') {
                return std::nullopt;
            }

            --index;

            while (index > 0 && (text[index - 1] == ' ' || text[index - 1] == '\t')) {
                --index;
            }

            const std::size_t end = index;

            while (index > 0) {
                const unsigned char byte = static_cast<unsigned char>(text[index - 1]);

                if (std::isalnum(byte) != 0 || byte == '_') {
                    --index;
                } else {
                    break;
                }
            }

            if (index == end) {
                return std::nullopt;
            }

            return text.substr(index, end - index);
        }

        // Resolve the struct type behind `receiver.` at the cursor, if possible.
        const Compiler::Semantic::STType::StructType *receiverStruct(const SymbolIndex &index, const std::string &text, const std::size_t offset) {
            const std::optional<std::string> receiver = memberReceiverName(text, offset);

            if (!receiver.has_value()) {
                return nullptr;
            }

            if (*receiver == "self") {
                for (const Compiler::Semantic::SymbolDefinition &definition : index.globals) {
                    if (definition.symbol == nullptr || definition.symbol->getKind() != SymbolKind::Type || !containsOffset(definition.range, offset)) {
                        continue;
                    }

                    const auto &typeSymbol = static_cast<const Compiler::Semantic::TypeSymbol &>(*definition.symbol);
                    return asStruct(typeSymbol.getType().get());
                }

                return nullptr;
            }

            for (const Compiler::Semantic::SymbolDefinition &definition : index.globals) {
                if (definition.name != *receiver || definition.symbol == nullptr || !isVisible(definition, offset)) {
                    continue;
                }

                std::shared_ptr<Compiler::Semantic::STType::Type> type;

                switch (definition.symbol->getKind()) {
                    case SymbolKind::Variable:
                        type = static_cast<const Compiler::Semantic::VariableSymbol &>(*definition.symbol).getType();
                        break;
                    case SymbolKind::Field:
                        type = static_cast<const Compiler::Semantic::FieldSymbol &>(*definition.symbol).getType();
                        break;
                    default:
                        continue;
                }

                if (const auto *structure = asStruct(type.get())) {
                    return structure;
                }
            }

            return nullptr;
        }

        void appendStructMembers(const Compiler::Semantic::STType::StructType &structType, std::unordered_set<std::string> &seen, std::vector<CompletionItem> &items) {
            const auto &scope = structType.getMemberScope();

            for (const auto &[name, range] : structType.getMemberDeclarations()) {
                (void) range;

                if (!seen.insert(name).second) {
                    continue;
                }

                const auto iterator = scope.symbols.find(name);

                if (iterator == scope.symbols.end()) {
                    continue;
                }

                CompletionItem item;
                item.label = name;
                item.kind = name == structType.getName()
                                ? CompletionItemKind::Constructor
                                : (iterator->second->getKind() == SymbolKind::Field ? CompletionItemKind::Field : CompletionItemKind::Method);
                items.push_back(std::move(item));
            }
        }

        // Keywords are read straight from the lexer vocabulary so the list can never
        // drift from the grammar.
        const std::vector<std::string> &ryntraKeywords() {
            static const std::vector<std::string> keywords = [] {
                std::vector<std::string> result;

                antlr4::ANTLRInputStream input("");
                Ryntra::antlr::RyntraLexer lexer(&input);
                const antlr4::dfa::Vocabulary &vocabulary = lexer.getVocabulary();

                for (std::size_t type = 1; type <= vocabulary.getMaxTokenType(); ++type) {
                    const std::string_view literal = vocabulary.getLiteralName(type);

                    if (literal.size() < 2 || literal.front() != '\'' || literal.back() != '\'') {
                        continue;
                    }

                    const std::string keyword(literal.substr(1, literal.size() - 2));

                    if (keyword.empty() || (std::isalpha(static_cast<unsigned char>(keyword.front())) == 0 && keyword.front() != '_')) {
                        continue;
                    }

                    bool identifierLike = true;

                    for (const char character : keyword) {
                        if (std::isalnum(static_cast<unsigned char>(character)) == 0 && character != '_') {
                            identifierLike = false;
                            break;
                        }
                    }

                    if (identifierLike) {
                        result.push_back(keyword);
                    }
                }

                return result;
            }();

            return keywords;
        }
    } // namespace

    std::vector<Diagnostic> CompilerLanguageProvider::analyze(const std::string &uri, const std::string &text) {
        (void) uri;

        Compiler::ErrorHandler &errorHandler = Compiler::ErrorHandler::getInstance();
        errorHandler.clear();

        try {
            Compiler::LexParseErrorHandler parseErrorHandler;

            antlr4::ANTLRInputStream input(text);
            Ryntra::antlr::RyntraLexer lexer(&input);
            antlr4::CommonTokenStream tokens(&lexer);
            tokens.fill();

            Ryntra::antlr::RyntraParser parser(&tokens);
            lexer.removeErrorListeners();
            lexer.addErrorListener(&parseErrorHandler);
            parser.removeErrorListeners();
            parser.addErrorListener(&parseErrorHandler);

            Ryntra::antlr::RyntraParser::ProgramContext *tree = parser.program();

            bool hasSyntaxError = false;
            for (const Compiler::ErrorObject &error : errorHandler.getErrorObjects()) {
                if (error.type == Compiler::kError) {
                    hasSyntaxError = true;
                    break;
                }
            }

            if (!hasSyntaxError) {
                Compiler::ASTBuilder builder;
                const std::shared_ptr<Compiler::ProgramNode> program = builder.visitProgram(tree);

                Compiler::Semantic::SemanticAnalyzer analyzer;
                analyzer.setCompilationMode(Compiler::CompilationMode::Editor);
                analyzer.analyze(program);
            }
        } catch (const std::exception &exception) {
            errorHandler.makeError(std::string("Internal front-end error: ") + exception.what(), Compiler::SourceRange());
        }

        return convert(errorHandler.getErrorObjects());
    }

    std::optional<Hover> CompilerLanguageProvider::hover(const std::string &uri, const std::string &text, const Protocol::Position &position) {
        (void) uri;

        const std::optional<IdentifierSpan> identifier = identifierAt(text, position);

        if (!identifier.has_value()) {
            return std::nullopt;
        }

        const std::optional<SymbolIndex> index = analyzeSymbols(text);

        if (!index.has_value()) {
            return std::nullopt;
        }

        const std::optional<ResolvedSymbol> resolved = resolveAt(*index, text, *identifier, position);

        if (!resolved.has_value()) {
            return std::nullopt;
        }

        Hover result;
        result.contents = resolved->isConstructor ? describeConstructor(*resolved->symbol) : describeSymbol(*resolved->symbol);
        result.range = identifier->range;
        return result;
    }

    std::optional<Location> CompilerLanguageProvider::definition(const std::string &uri, const std::string &text, const Protocol::Position &position) {
        const std::optional<IdentifierSpan> identifier = identifierAt(text, position);

        if (!identifier.has_value()) {
            return std::nullopt;
        }

        const std::optional<SymbolIndex> index = analyzeSymbols(text);

        if (!index.has_value()) {
            return std::nullopt;
        }

        const std::optional<ResolvedSymbol> resolved = resolveAt(*index, text, *identifier, position);

        // Builtins (e.g. `int`, `print`) have no real declaration site.
        if (!resolved.has_value() || resolved->range.isEmpty()) {
            return std::nullopt;
        }

        return Location{uri, toProtocolRange(resolved->range)};
    }

    std::vector<DocumentSymbol> CompilerLanguageProvider::documentSymbols(const std::string &uri, const std::string &text) {
        (void) uri;

        const std::optional<SymbolIndex> index = analyzeSymbols(text);

        if (!index.has_value()) {
            return {};
        }

        std::vector<DocumentSymbol> symbols;

        for (const Compiler::Semantic::SymbolDefinition &definition : index->globals) {
            if (definition.symbol == nullptr) {
                continue;
            }

            const Compiler::Semantic::SymbolKind kind = definition.symbol->getKind();

            if (kind == Compiler::Semantic::SymbolKind::Type) {
                const auto &typeSymbol = static_cast<const Compiler::Semantic::TypeSymbol &>(*definition.symbol);
                const auto structType = std::dynamic_pointer_cast<Compiler::Semantic::STType::StructType>(typeSymbol.getType());

                if (structType == nullptr) {
                    continue;
                }

                DocumentSymbol symbol;
                symbol.name = definition.name;
                symbol.kind = DocumentSymbolKind::Struct;
                symbol.range = toProtocolRange(definition.range);
                symbol.selectionRange = symbol.range;
                symbol.children = describeMembers(*structType);
                symbols.push_back(std::move(symbol));
            } else if (kind == Compiler::Semantic::SymbolKind::Function || kind == Compiler::Semantic::SymbolKind::OverloadSet) {
                DocumentSymbol symbol;
                symbol.name = definition.name;
                symbol.kind = DocumentSymbolKind::Function;
                symbol.range = toProtocolRange(definition.range);
                symbol.selectionRange = symbol.range;
                symbols.push_back(std::move(symbol));
            }
        }

        return symbols;
    }

    std::vector<CompletionItem> CompilerLanguageProvider::completion(const std::string &uri, const std::string &text, const Protocol::Position &position) {
        (void) uri;

        std::vector<CompletionItem> items;

        const SanitizedSource sanitized = sanitizeForCompletion(text, position);
        const std::optional<SymbolIndex> index = analyzeSymbols(sanitized.text);

        if (!index.has_value()) {
            return items;
        }

        std::unordered_set<std::string> seen;
        const std::string &source = sanitized.text;
        const std::size_t offset = sanitized.offset;

        if (isMemberAccess(source, offset)) {
            if (const auto *structure = receiverStruct(*index, source, offset)) {
                appendStructMembers(*structure, seen, items);
                return items;
            }

            for (const MemberDeclaration &member : index->members) {
                if (!seen.insert(member.name).second) {
                    continue;
                }

                CompletionItem item;
                item.label = member.name;
                item.kind = member.isConstructor
                                ? CompletionItemKind::Constructor
                                : (member.symbol != nullptr && member.symbol->getKind() == SymbolKind::Field ? CompletionItemKind::Field : CompletionItemKind::Method);
                items.push_back(std::move(item));
            }

            return items;
        }

        for (const Compiler::Semantic::SymbolDefinition &definition : index->globals) {
            if (definition.symbol == nullptr || !isVisible(definition, offset)) {
                continue;
            }

            if (!seen.insert(definition.name).second) {
                continue;
            }

            CompletionItem item;
            item.label = definition.name;

            switch (definition.symbol->getKind()) {
                case SymbolKind::Type: {
                    const auto &typeSymbol = static_cast<const Compiler::Semantic::TypeSymbol &>(*definition.symbol);
                    item.kind = asStruct(typeSymbol.getType().get()) != nullptr ? CompletionItemKind::Struct : CompletionItemKind::Class;
                    break;
                }
                case SymbolKind::Function:
                case SymbolKind::OverloadSet:
                    item.kind = CompletionItemKind::Function;
                    break;
                case SymbolKind::Variable:
                    item.kind = CompletionItemKind::Variable;
                    break;
                case SymbolKind::Field:
                    item.kind = CompletionItemKind::Field;
                    break;
            }

            items.push_back(std::move(item));
        }

        for (const std::string &keyword : ryntraKeywords()) {
            if (!seen.insert(keyword).second) {
                continue;
            }

            CompletionItem item;
            item.label = keyword;
            item.kind = CompletionItemKind::Keyword;
            items.push_back(std::move(item));
        }

        return items;
    }
} // namespace Ryntra::LSP
