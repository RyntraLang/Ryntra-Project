#pragma once

#include "Bytecode.h"
#include "Compiler/IR/Module.h"
#include "VMValue.h"
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Ryntra::VM {
    class BytecodeGenerator {
    public:
        BytecodeGenerator();

        std::vector<std::shared_ptr<BytecodeFunction>> generate(const std::shared_ptr<IR::Module> &module);
        const std::vector<VMValue> &getConstantPool() const { return constantPool_; }

    private:
        void generateFunction(const std::shared_ptr<IR::Function> &func);
        void generateBasicBlock(const std::shared_ptr<IR::BasicBlock> &block);
        void generateInstruction(const std::shared_ptr<IR::Instruction> &inst);

        void pushOperandValue(const std::shared_ptr<IR::Value> &operand);

        // Append a bytecode instruction, stamping it with the source range of the
        // IR instruction currently being lowered.
        void emit(OpCode op, int32_t operand = 0, int32_t operand2 = 0);

        int32_t addConstant(const VMValue &value);
        int32_t getFunctionIndex(const std::string &name);
        int32_t getBuiltinIndex(const std::string &name);

        struct Fixup {
            size_t instructionIndex;
            std::string targetBlockName;
        };

        std::vector<VMValue> constantPool_;
        std::vector<std::shared_ptr<BytecodeFunction>> functions_;
        std::unordered_map<std::string, int32_t> functionIndices_;
        std::shared_ptr<BytecodeFunction> currentFunction_;
        std::shared_ptr<IR::Module> currentModule_;
        std::unordered_map<const IR::Value *, int32_t> instructionSlots_;
        std::unordered_map<const IR::Value *, int32_t> allocaSlotMap_;
        int32_t nextSlot_;
        std::unordered_map<std::string, int32_t> blockOffsets_;
        std::vector<Fixup> fixups_;
        Compiler::SourceRange currentRange_;
    };
} // namespace Ryntra::VM
