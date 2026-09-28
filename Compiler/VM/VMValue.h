#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <string>
#include <variant>
#include <vector>

namespace Ryntra::VM {
    class VMValue;
    struct ArrayData {
        std::vector<VMValue> elements;
    };
    struct ArrayElementRef {
        std::shared_ptr<ArrayData> array;
        int32_t index;
    };

    struct StructData;

    // A reference to one field of a struct instance (produced by FieldRef). The
    // offset is a byte offset into the struct's storage.
    struct StructFieldRef {
        std::shared_ptr<StructData> data;
        int32_t offset;
    };

    // Runtime value representation
    class VMValue {
    public:
        enum class Type {
            Void,
            Uninitialized, // storage that has never been assigned (reading it traps)
            Int32,
            Int64,
            String,
            FunctionPtr,
            Array,
            Reference,    // holds an int32 slot index (local)
            Pointer,      // holds an int32 slot index (local) or array element index
            HeapPointer,   // holds an int32 slot index (heap)
            ArrayElementRef, // reference to an array element (arr[i])
            Struct,          // a struct instance (shared aggregate storage)
            StructFieldRef   // reference to a struct field (s.field)
        };

        using ValueData = std::variant<std::monostate, int32_t, int64_t, std::string, void *, std::shared_ptr<ArrayData>, ArrayElementRef, std::shared_ptr<StructData>, StructFieldRef>;

        VMValue() : type_(Type::Void), data_(std::monostate{}) {}

        /// \brief Create a value that represents never-assigned storage.
        static VMValue uninitialized() {
            VMValue value;
            value.type_ = Type::Uninitialized;
            return value;
        }

        explicit VMValue(int32_t val) : type_(Type::Int32), data_(val) {}
        explicit VMValue(int64_t val) : type_(Type::Int64), data_(val) {}
        explicit VMValue(const std::string &val) : type_(Type::String), data_(val) {}
        explicit VMValue(void *ptr) : type_(Type::FunctionPtr), data_(ptr) {}
        explicit VMValue(std::shared_ptr<ArrayData> arr) : type_(Type::Array), data_(std::move(arr)) {}
        explicit VMValue(ArrayElementRef elemRef) : type_(Type::ArrayElementRef), data_(elemRef) {}
        explicit VMValue(std::shared_ptr<StructData> strct) : type_(Type::Struct), data_(std::move(strct)) {}
        explicit VMValue(StructFieldRef fieldRef) : type_(Type::StructFieldRef), data_(fieldRef) {}

        Type getType() const { return type_; }

        int32_t asInt32() const { return std::get<int32_t>(data_); }
        int64_t asInt64() const { return std::get<int64_t>(data_); }
        const std::string &asString() const { return std::get<std::string>(data_); }
        void *asFunctionPtr() const { return std::get<void *>(data_); }
        std::shared_ptr<ArrayData> asArray() const { return std::get<std::shared_ptr<ArrayData>>(data_); }

        // Function pointer support — function pointers are stored as int32
        // indices into the VM's function table.
        void setFunctionIndex(int32_t index) { type_ = Type::FunctionPtr; data_ = index; }
        int32_t getFunctionIndex() const { return std::get<int32_t>(data_); }
        bool isFunctionPtr() const { return type_ == Type::FunctionPtr; }

        bool isVoid() const { return type_ == Type::Void; }
        bool isUninitialized() const { return type_ == Type::Uninitialized; }
        bool isInt32() const { return type_ == Type::Int32; }
        bool isInt64() const { return type_ == Type::Int64; }
        bool isString() const { return type_ == Type::String; }
        bool isArray() const { return type_ == Type::Array; }
        bool isArrayElementRef() const { return type_ == Type::ArrayElementRef; }
        ArrayElementRef asArrayElementRef() const { return std::get<ArrayElementRef>(data_); }

        bool isStruct() const { return type_ == Type::Struct; }
        std::shared_ptr<StructData> asStruct() const { return std::get<std::shared_ptr<StructData>>(data_); }
        bool isStructFieldRef() const { return type_ == Type::StructFieldRef; }
        StructFieldRef asStructFieldRef() const { return std::get<StructFieldRef>(data_); }

        // Reference support — refs are stored as int32 slot indices
        void setReferenceSlot(int32_t slot) { type_ = Type::Reference; data_ = slot; }
        int32_t getReferenceSlot() const { return std::get<int32_t>(data_); }
        bool isReference() const { return type_ == Type::Reference; }

        // Pointer support — pointers are stored as int32 slot indices
        // May also have an optional array ref for array element pointers (from ptr(arr))
        void setPointerSlot(int32_t slot) { type_ = Type::Pointer; data_ = slot; ptrArrayData_ = nullptr; }
        int32_t getPointerSlot() const { return std::get<int32_t>(data_); }
        bool isPointer() const { return type_ == Type::Pointer; }

        void setArrayPointer(int32_t index, std::shared_ptr<ArrayData> arr) { type_ = Type::Pointer; data_ = index; ptrArrayData_ = std::move(arr); }
        bool isArrayPointer() const { return isPointer() && ptrArrayData_ != nullptr; }
        std::shared_ptr<ArrayData> getArrayPointerData() const { return ptrArrayData_; }

        // Heap pointer support — heap pointers are stored as int32 heap indices
        void setHeapPointerSlot(int32_t slot) { type_ = Type::HeapPointer; data_ = slot; }
        int32_t getHeapPointerSlot() const { return std::get<int32_t>(data_); }
        bool isHeapPointer() const { return type_ == Type::HeapPointer; }

    private:
        Type type_;
        ValueData data_;
        std::shared_ptr<ArrayData> ptrArrayData_; // optional: non-null for array element pointers
    };

    // Aggregate storage: a struct instance owns a byte-addressable array of field
    // slots (one slot per byte). Field values are addressed by the byte offsets
    // computed by the compiler's Struct Layout phase, so no offset arithmetic is
    // performed at runtime. The backing storage is allocated with the struct's
    // alignment so the instance satisfies its alignment requirement.
    struct StructData {
        VMValue *fields = nullptr;
        int32_t size = 0;      // total size in bytes
        int32_t alignment = 1; // alignment in bytes

        StructData(int32_t sizeBytes, int32_t align)
            : size(sizeBytes > 0 ? sizeBytes : 0),
              alignment(align > 0 ? align : 1) {
            if (size <= 0)
                return;
            fields = static_cast<VMValue *>(
                allocate(static_cast<size_t>(size) * sizeof(VMValue), static_cast<size_t>(alignment)));
            for (int32_t i = 0; i < size; ++i)
                new (&fields[i]) VMValue(VMValue::uninitialized());
        }

        ~StructData() {
            if (!fields)
                return;
            for (int32_t i = 0; i < size; ++i)
                fields[i].~VMValue();
            deallocate(fields);
        }

        StructData(const StructData &) = delete;
        StructData &operator=(const StructData &) = delete;

        // Field-wise copy used when a whole struct value is assigned to another.
        StructData &assignFrom(const StructData &other) {
            int32_t count = size < other.size ? size : other.size;
            for (int32_t i = 0; i < count; ++i)
                fields[i] = other.fields[i];
            return *this;
        }

        VMValue &at(int32_t offset) { return fields[offset]; }
        const VMValue &at(int32_t offset) const { return fields[offset]; }

        bool inBounds(int32_t offset) const { return offset >= 0 && offset < size; }

    private:
        static void *allocate(size_t bytes, size_t align) {
            if (align <= static_cast<size_t>(__STDCPP_DEFAULT_NEW_ALIGNMENT__))
                return ::operator new(bytes);
            return ::operator new(bytes, std::align_val_t(align));
        }

        void deallocate(void *ptr) const {
            if (static_cast<size_t>(alignment) <= static_cast<size_t>(__STDCPP_DEFAULT_NEW_ALIGNMENT__))
                ::operator delete(ptr);
            else
                ::operator delete(ptr, std::align_val_t(static_cast<size_t>(alignment)));
        }
    };
} // namespace Ryntra::VM
