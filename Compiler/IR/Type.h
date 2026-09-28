#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Ryntra::IR {
    class Type {
    public:
        enum class Kind {
            Void,
            Int32,
            Int64,
            Bool,
            String,
            Function,
            Array,
            Ref,
            Ptr,
            Struct
        };

        Type(Kind kind) : kind_(kind) {}
        virtual ~Type() = default;

        Kind getKind() const { return kind_; }

        virtual std::string toString() const = 0;
        virtual bool isEqual(const Type *other) const = 0;

        bool isVoid() const { return kind_ == Kind::Void; }
        bool isInt32() const { return kind_ == Kind::Int32; }
        bool isInt64() const { return kind_ == Kind::Int64; }
        bool isBool() const { return kind_ == Kind::Bool; }
        bool isString() const { return kind_ == Kind::String; }
        bool isFunction() const { return kind_ == Kind::Function; }
        bool isArray() const { return kind_ == Kind::Array; }
        bool isRef() const { return kind_ == Kind::Ref; }
        bool isPtr() const { return kind_ == Kind::Ptr; }
        bool isStruct() const { return kind_ == Kind::Struct; }

        static std::shared_ptr<Type> getVoidType();
        static std::shared_ptr<Type> getInt32Type();
        static std::shared_ptr<Type> getInt64Type();
        static std::shared_ptr<Type> getBoolType();
        static std::shared_ptr<Type> getStringType();

    private:
        Kind kind_;
    };

    class VoidType : public Type {
    public:
        VoidType() : Type(Kind::Void) {}

        std::string toString() const override {
            return "void";
        }

        bool isEqual(const Type *other) const override {
            return other->isVoid();
        }
    };

    class Int32Type : public Type {
    public:
        Int32Type() : Type(Kind::Int32) {}

        std::string toString() const override {
            return "i32";
        }

        bool isEqual(const Type *other) const override {
            return other->isInt32();
        }
    };

    class Int64Type : public Type {
    public:
        Int64Type() : Type(Kind::Int64) {}

        std::string toString() const override {
            return "i64";
        }

        bool isEqual(const Type *other) const override {
            return other->isInt64();
        }
    };

    class BoolType : public Type {
    public:
        BoolType() : Type(Kind::Bool) {}

        std::string toString() const override {
            return "i1";
        }

        bool isEqual(const Type *other) const override {
            return other->isBool();
        }
    };

    class StringType : public Type {
    public:
        StringType() : Type(Kind::String) {}

        std::string toString() const override {
            return "string";
        }

        bool isEqual(const Type *other) const override {
            return other->isString();
        }
    };

    class ArrayType : public Type {
    public:
        ArrayType(std::shared_ptr<Type> elementType)
            : Type(Kind::Array), elementType_(std::move(elementType)) {}

        std::shared_ptr<Type> getElementType() const { return elementType_; }

        std::string toString() const override {
            return elementType_->toString() + "[]";
        }

        bool isEqual(const Type *other) const override {
            if (!other->isArray())
                return false;
            auto *arrType = static_cast<const ArrayType *>(other);
            return elementType_->isEqual(arrType->elementType_.get());
        }

    private:
        std::shared_ptr<Type> elementType_;
    };

    class RefType : public Type {
    public:
        RefType(std::shared_ptr<Type> elementType)
            : Type(Kind::Ref), elementType_(std::move(elementType)) {}

        std::shared_ptr<Type> getElementType() const { return elementType_; }

        std::string toString() const override {
            return "ref<" + elementType_->toString() + ">";
        }

        bool isEqual(const Type *other) const override {
            if (!other->isRef())
                return false;
            auto *refType = static_cast<const RefType *>(other);
            return elementType_->isEqual(refType->elementType_.get());
        }

    private:
        std::shared_ptr<Type> elementType_;
    };

    class PtrType : public Type {
    public:
        PtrType(std::shared_ptr<Type> elementType)
            : Type(Kind::Ptr), elementType_(std::move(elementType)) {}

        std::shared_ptr<Type> getElementType() const { return elementType_; }

        std::string toString() const override {
            return "ptr<" + elementType_->toString() + ">";
        }

        bool isEqual(const Type *other) const override {
            if (!other->isPtr())
                return false;
            auto *ptrType = static_cast<const PtrType *>(other);
            return elementType_->isEqual(ptrType->elementType_.get());
        }

    private:
        std::shared_ptr<Type> elementType_;
    };

    // A named aggregate with a byte-level layout. Fields are laid out in source
    // declaration order using their natural alignment, inserting padding as
    // needed. The layout (field byte offsets, total size, alignment) is computed
    // by the compiler before codegen; consumers use the offset accessors below
    // rather than recomputing offsets.
    class StructType : public Type {
    public:
        struct Field {
            std::string name;
            std::shared_ptr<Type> type;
            int32_t offset = 0;    // byte offset from the start of the struct
            int32_t size = 0;      // size in bytes
            int32_t alignment = 1; // natural alignment in bytes

            Field(std::string name, std::shared_ptr<Type> type)
                : name(std::move(name)), type(std::move(type)) {}
        };

        explicit StructType(std::string name)
            : Type(Kind::Struct), name_(std::move(name)) {}

        const std::string &getName() const { return name_; }

        void addField(const std::string &fieldName, std::shared_ptr<Type> fieldType) {
            for (auto &field : fields_) {
                if (field.name == fieldName) {
                    field.type = std::move(fieldType);
                    layoutComputed_ = false;
                    return;
                }
            }
            fields_.emplace_back(fieldName, std::move(fieldType));
            layoutComputed_ = false;
        }

        // Explicit alignment from an `[AlignAs(N)]` annotation (1 = natural only).
        void setExplicitAlignment(int32_t alignment) {
            explicitAlignment_ = alignment > 0 ? alignment : 1;
            layoutComputed_ = false;
        }
        int32_t getExplicitAlignment() const { return explicitAlignment_; }

        // Compute (or recompute) this struct's byte layout. Safe to call more than
        // once; nested struct layouts are computed on demand.
        void computeLayout() const { ensureLayout(); }

        const std::vector<Field> &getFields() const {
            ensureLayout();
            return fields_;
        }

        int getFieldIndex(const std::string &fieldName) const {
            ensureLayout();
            for (size_t i = 0; i < fields_.size(); ++i) {
                if (fields_[i].name == fieldName)
                    return static_cast<int>(i);
            }
            return -1;
        }

        // Byte offset of a field, or -1 when the field is unknown.
        int32_t getFieldOffset(const std::string &fieldName) const {
            ensureLayout();
            for (const auto &field : fields_) {
                if (field.name == fieldName)
                    return field.offset;
            }
            return -1;
        }

        std::shared_ptr<Type> getFieldType(const std::string &fieldName) const {
            ensureLayout();
            for (const auto &field : fields_) {
                if (field.name == fieldName)
                    return field.type;
            }
            return nullptr;
        }

        int32_t getSize() const {
            ensureLayout();
            return size_;
        }

        int32_t getAlignment() const {
            ensureLayout();
            return alignment_;
        }

        std::string toString() const override { return "%" + name_; }

        // Nominal typing: two structs are the same type iff their names match,
        // even when their field layouts are identical.
        bool isEqual(const Type *other) const override {
            if (!other->isStruct())
                return false;
            return name_ == static_cast<const StructType *>(other)->name_;
        }

    private:
        static int32_t alignUp(int32_t value, int32_t alignment) {
            if (alignment <= 1)
                return value;
            return (value + alignment - 1) / alignment * alignment;
        }

        static void sizeAndAlignmentOf(const std::shared_ptr<Type> &type,
                                       int32_t &size, int32_t &alignment) {
            size = 1;
            alignment = 1;
            if (!type)
                return;

            switch (type->getKind()) {
            case Kind::Int32:
                size = 4;
                alignment = 4;
                break;
            case Kind::Int64:
                size = 8;
                alignment = 8;
                break;
            case Kind::Bool:
                size = 1;
                alignment = 1;
                break;
            case Kind::String:
                size = 8;
                alignment = 8;
                break;
            case Kind::Void:
                size = 1;
                alignment = 1;
                break;
            case Kind::Function:
            case Kind::Array:
            case Kind::Ref:
            case Kind::Ptr:
                size = 8;
                alignment = 8;
                break;
            case Kind::Struct: {
                auto structType = std::static_pointer_cast<StructType>(type);
                size = structType->getSize();
                alignment = structType->getAlignment();
                break;
            }
            default:
                break;
            }
        }

        void ensureLayout() const {
            if (layoutComputed_)
                return;
            // Mark as computed before recursing so self-referential types terminate.
            layoutComputed_ = true;

            int32_t offset = 0;
            int32_t maxAlignment = explicitAlignment_ > 0 ? explicitAlignment_ : 1;

            for (auto &field : fields_) {
                int32_t fieldSize = 1;
                int32_t fieldAlignment = 1;
                sizeAndAlignmentOf(field.type, fieldSize, fieldAlignment);

                field.size = fieldSize;
                field.alignment = fieldAlignment;
                offset = alignUp(offset, fieldAlignment);
                field.offset = offset;
                offset += fieldSize;
                maxAlignment = std::max(maxAlignment, fieldAlignment);
            }

            alignment_ = maxAlignment;
            size_ = alignUp(offset, maxAlignment);
        }

        std::string name_;
        mutable std::vector<Field> fields_;
        mutable int32_t size_ = 0;
        mutable int32_t alignment_ = 1;
        mutable bool layoutComputed_ = false;
        int32_t explicitAlignment_ = 1;
    };

    class FunctionType : public Type {
    public:
        FunctionType(std::shared_ptr<Type> returnType,
                     std::vector<std::shared_ptr<Type>> paramTypes)
            : Type(Kind::Function),
              returnType_(returnType),
              paramTypes_(paramTypes) {}

        std::shared_ptr<Type> getReturnType() const { return returnType_; }
        const std::vector<std::shared_ptr<Type>> &getParamTypes() const { return paramTypes_; }

        std::string toString() const override {
            std::string result = "(";
            for (size_t i = 0; i < paramTypes_.size(); ++i) {
                if (i > 0)
                    result += ", ";
                result += paramTypes_[i]->toString();
            }
            result += ") -> " + returnType_->toString();
            return result;
        }

        bool isEqual(const Type *other) const override {
            if (!other->isFunction())
                return false;
            const FunctionType *funcType = static_cast<const FunctionType *>(other);

            if (!returnType_->isEqual(funcType->returnType_.get()))
                return false;
            if (paramTypes_.size() != funcType->paramTypes_.size())
                return false;

            for (size_t i = 0; i < paramTypes_.size(); ++i) {
                if (!paramTypes_[i]->isEqual(funcType->paramTypes_[i].get()))
                    return false;
            }
            return true;
        }

    private:
        std::shared_ptr<Type> returnType_;
        std::vector<std::shared_ptr<Type>> paramTypes_;
    };

    inline std::shared_ptr<Type> Type::getVoidType() {
        static std::shared_ptr<Type> voidType = std::make_shared<VoidType>();
        return voidType;
    }

    inline std::shared_ptr<Type> Type::getInt32Type() {
        static std::shared_ptr<Type> int32Type = std::make_shared<Int32Type>();
        return int32Type;
    }

    inline std::shared_ptr<Type> Type::getStringType() {
        static std::shared_ptr<Type> stringType = std::make_shared<StringType>();
        return stringType;
    }

    inline std::shared_ptr<Type> Type::getInt64Type() {
        static std::shared_ptr<Type> int64Type = std::make_shared<Int64Type>();
        return int64Type;
    }

    inline std::shared_ptr<Type> Type::getBoolType() {
        static std::shared_ptr<Type> boolType = std::make_shared<BoolType>();
        return boolType;
    }
} // namespace Ryntra::IR
