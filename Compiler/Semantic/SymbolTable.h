#pragma once

#include "SourceLocation/SourceRange.h"
#include <algorithm>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Ryntra::Compiler::Semantic {
    class Scope;
    class Symbol;
    class FunctionSymbol;
    class FieldSymbol;

    namespace STType {
        // clang-format off
        enum class TypeKind {
            Void,
            Bool,                     // i1
            Char,                     // unsigned i16, implicitly cast from int or to int is not allowed
            Int8,                     // byte
            Int16,
            Int32,                    // int or long (depends on the platform that vm runs on)
            Int64,                    // long long

            UnsignedInt8,
            UnsignedInt16,
            UnsignedInt32,
            UnsignedInt64,

            Float32,                  // float
            Float64,                  // double
            Float128,                 // decimal

            String,
            Array,
            Reference,
            Pointer,
            Function,
            Struct
        };
        // clang-format on

        class Type {
        public:
            virtual ~Type() = default;
            virtual TypeKind getKind() const = 0;

            bool isNumeric() const {
                TypeKind k = this->getKind();
                return (k >= TypeKind::Int8 && k <= TypeKind::Float128);
            }

            bool isIntegral() const {
                TypeKind k = this->getKind();
                return (k >= TypeKind::Int8 && k <= TypeKind::UnsignedInt64);
            }

            bool isCharacter() const {
                TypeKind k = this->getKind();
                return k == TypeKind::Char;
            }

            // Maybe there's more in the future
        };

        // Natural alignment (in bytes) of a type. Struct types also honour an
        // explicit `[AlignAs(N)]` alignment.
        inline int alignmentOf(const Type &type);

        class VoidType : public Type {
        public:
            TypeKind getKind() const override { return TypeKind::Void; }
        };

        class Int32Type : public Type {
        public:
            TypeKind getKind() const override { return TypeKind::Int32; }
        };

        class Int64Type : public Type {
        public:
            TypeKind getKind() const override { return TypeKind::Int64; }
        };

        class StringType : public Type {
        public:
            TypeKind getKind() const override { return TypeKind::String; }
        };

        class BoolType : public Type {
        public:
            TypeKind getKind() const override { return TypeKind::Bool; }
        };

        class ArrayType : public Type {
        public:
            ArrayType(std::shared_ptr<Type> elementType) : elementType(std::move(elementType)) {}
            TypeKind getKind() const override { return TypeKind::Array; }
            const std::shared_ptr<Type> &getElementType() const { return elementType; }
        private:
            std::shared_ptr<Type> elementType;
        };

        class ReferenceType : public Type {
        public:
            ReferenceType(std::shared_ptr<Type> elementType) : elementType(std::move(elementType)) {}
            TypeKind getKind() const override { return TypeKind::Reference; }
            const std::shared_ptr<Type> &getElementType() const { return elementType; }
        private:
            std::shared_ptr<Type> elementType;
        };

        class PointerType : public Type {
        public:
            PointerType(std::shared_ptr<Type> elementType) : elementType(std::move(elementType)) {}
            TypeKind getKind() const override { return TypeKind::Pointer; }
            const std::shared_ptr<Type> &getElementType() const { return elementType; }
        private:
            std::shared_ptr<Type> elementType;
        };

        class FunctionType : public Type {
        public:
            FunctionType(std::shared_ptr<Type> returnType, std::vector<std::shared_ptr<Type>> paramTypes)
                : returnType(std::move(returnType)), paramTypes(std::move(paramTypes)) {}
            TypeKind getKind() const override { return TypeKind::Function; }
            const std::shared_ptr<Type> &getReturnType() const { return returnType; }
            const std::vector<std::shared_ptr<Type>> &getParamTypes() const { return paramTypes; }
        private:
            std::shared_ptr<Type> returnType;
            std::vector<std::shared_ptr<Type>> paramTypes;
        };

        class StructType : public Type {
        public:
            explicit StructType(std::string name);
            ~StructType() override;

            TypeKind getKind() const override { return TypeKind::Struct; }
            const std::string &getName() const { return name; }

            // Registers a field type and the matching FieldSymbol in the member scope.
            // The optional range records where the field was declared (for tooling).
            void addField(const std::string &fieldName, std::shared_ptr<Type> fieldType, const SourceRange &range = {});
            std::shared_ptr<Type> getField(const std::string &fieldName) const {
                auto it = fields.find(fieldName);
                return it == fields.end() ? nullptr : it->second;
            }
            const std::unordered_map<std::string, std::shared_ptr<Type>> &getFields() const { return fields; }
            // Source-declaration order of field names (deterministic layout for codegen).
            const std::vector<std::string> &getFieldOrder() const { return fieldOrder; }

            // Explicit alignment requested by an `[AlignAs(N)]` annotation (0 = natural).
            void setExplicitAlignment(int value) { explicitAlignment = value; }
            int getExplicitAlignment() const { return explicitAlignment; }

            // Effective alignment: the larger of the explicit alignment and the
            // natural alignment of every field.
            int getAlignment() const {
                int result = explicitAlignment > 0 ? explicitAlignment : 1;
                for (const auto &fieldName : fieldOrder) {
                    auto it = fields.find(fieldName);
                    if (it != fields.end() && it->second)
                        result = std::max(result, alignmentOf(*it->second));
                }
                return result;
            }

            // Member symbol table (fields and methods). The scope has `Scope::Kind::Class`.
            void defineMethod(std::shared_ptr<FunctionSymbol> method, const SourceRange &range = {});
            std::shared_ptr<Symbol> lookupMember(const std::string &memberName) const;
            Scope &getMemberScope() const;

            // Declaration ranges of registered members (fields, methods, constructors),
            // in registration order, for go-to-definition / hover.
            const std::vector<std::pair<std::string, SourceRange>> &getMemberDeclarations() const { return memberDeclarations; }

        private:
            void ensureMemberScope() const;

            std::string name;
            std::unordered_map<std::string, std::shared_ptr<Type>> fields;
            std::vector<std::string> fieldOrder;
            mutable std::shared_ptr<Scope> memberScope;
            std::vector<std::pair<std::string, SourceRange>> memberDeclarations;
            int explicitAlignment = 0;
        };

        inline int alignmentOf(const Type &type) {
            switch (type.getKind()) {
            case TypeKind::Void:
                return 1;
            case TypeKind::Bool:
            case TypeKind::Int8:
            case TypeKind::UnsignedInt8:
                return 1;
            case TypeKind::Char:
            case TypeKind::Int16:
            case TypeKind::UnsignedInt16:
                return 2;
            case TypeKind::Int32:
            case TypeKind::UnsignedInt32:
            case TypeKind::Float32:
                return 4;
            case TypeKind::Int64:
            case TypeKind::UnsignedInt64:
            case TypeKind::Float64:
                return 8;
            case TypeKind::Float128:
                return 16;
            case TypeKind::String:
            case TypeKind::Array:
            case TypeKind::Reference:
            case TypeKind::Pointer:
            case TypeKind::Function:
                return 8;
            case TypeKind::Struct:
                return static_cast<const StructType &>(type).getAlignment();
            default:
                return 1;
            }
        }
    } // namespace STType

    using TypePtr = std::shared_ptr<STType::Type>;

    enum class SymbolKind {
        Variable,
        Function,
        OverloadSet,
        Type,
        Field
    };

    class Symbol {
    public:
        virtual ~Symbol() = default;
        explicit Symbol(std::string name) : name(std::move(name)) {}
        const std::string &getName() const { return name; }

        virtual SymbolKind getKind() const = 0;

    private:
        std::string name;
    };

    class FunctionSymbol : public Symbol {
    public:
        FunctionSymbol(std::string name, TypePtr returnType, std::vector<TypePtr> paramTypes)
            : Symbol(std::move(name)), returnType(std::move(returnType)), paramTypes(std::move(paramTypes)) {}

        const TypePtr &getReturnType() const { return returnType; }
        const std::vector<TypePtr> &getParamTypes() const { return paramTypes; }
        SymbolKind getKind() const override { return SymbolKind::Function; }

    private:
        TypePtr returnType;
        std::vector<TypePtr> paramTypes;
    };

    class VariableSymbol : public Symbol {
    public:
        VariableSymbol(std::string name, TypePtr type, bool parameter = false)
            : Symbol(std::move(name)), type(std::move(type)), parameter(parameter) {}

        const TypePtr &getType() const { return type; }
        bool isParameter() const { return parameter; }
        SymbolKind getKind() const override { return SymbolKind::Variable; }

    private:
        TypePtr type;
        bool parameter = false;
    };

    class FieldSymbol : public Symbol {
    public:
        FieldSymbol(std::string name, TypePtr type)
            : Symbol(std::move(name)), type(std::move(type)) {}

        const TypePtr &getType() const { return type; }
        SymbolKind getKind() const override { return SymbolKind::Field; }

    private:
        TypePtr type;
    };

    class OverloadSet : public Symbol {
    public:
        OverloadSet(std::string name) : Symbol(std::move(name)) {}

        void addFunction(std::shared_ptr<FunctionSymbol> fn);
        const std::vector<std::shared_ptr<FunctionSymbol>> &getFunctions() const {
            return functions;
        }
        SymbolKind getKind() const override { return SymbolKind::OverloadSet; }

    private:
        std::vector<std::shared_ptr<FunctionSymbol>> functions;
    };

    class TypeSymbol : public Symbol {
    public:
        TypeSymbol(std::string name, TypePtr type)
            : Symbol(std::move(name)), type(std::move(type)) {}

        const TypePtr &getType() const { return type; }
        SymbolKind getKind() const override { return SymbolKind::Type; }

    private:
        TypePtr type;
    };

    class Scope {
    public:
        Scope *parent;

        enum class Kind {
            Global,
            Function,
            Block,
            Class
        } kind;

        // Source span of the construct that opened this scope (for tooling).
        SourceRange range;

        std::shared_ptr<Symbol> find(const std::string &name) {
            auto iterator = symbols.find(name);
            if (iterator != symbols.end()) {
                return iterator->second;
            }

            if (parent) {
                return parent->find(name);
            }

            return nullptr;
        }

        std::unordered_map<std::string, std::shared_ptr<Symbol>> symbols;
    };

    /// \brief A symbol declaration captured while analysing, kept for tooling
    /// (go-to-definition / hover / completion). Holds the declaration range, the
    /// symbol, and the scope it is visible in.
    struct SymbolDefinition {
        std::string name;
        SourceRange range;
        std::shared_ptr<Symbol> symbol;
        // True for symbols in the global scope (always visible).
        bool global = true;
        // Span of the enclosing scope, used to decide visibility at a position.
        SourceRange scopeRange;
    };

    class SymbolTable {
    public:
        SymbolTable();

        void enterScope(Scope::Kind kind = Scope::Kind::Global, const SourceRange &range = {});
        void exitScope();

        void define(std::shared_ptr<Symbol> symbol, const SourceRange &range);
        std::shared_ptr<Symbol> resolve(const std::string &name);

        // Every declaration registered through define(), in source order.
        [[nodiscard]] const std::vector<SymbolDefinition> &getDefinitions() const { return definitions; }

        // True if the symbol is an entity whose address can be taken
        // (a variable or a function; `Type`/`Field` symbols are not addressable).
        static bool isAddressable(const std::shared_ptr<Symbol> &symbol);

    private:
        std::vector<std::unique_ptr<Scope>> scopes;
        std::vector<SymbolDefinition> definitions;
    };
} // namespace Ryntra::Compiler::Semantic
