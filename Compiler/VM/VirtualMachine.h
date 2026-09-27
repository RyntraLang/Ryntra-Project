#pragma once

#include "Bytecode.h"
#include "VMValue.h"
#include "ErrorHandler/RuntimeError.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Ryntra::VM {
    using NativeFunction = std::function<VMValue(const std::vector<VMValue> &)>;

    struct CallFrame {
        BytecodeFunction *func;
        size_t ip;
        std::vector<VMValue> locals;
        size_t stackBase;
        // Where this frame was entered from, for runtime tracebacks.
        std::string callerName;
        Compiler::SourceRange callSiteRange;
    };

    class VirtualMachine {
    public:
        VirtualMachine();

        void load(const std::vector<std::shared_ptr<BytecodeFunction>> &funcs,
                  const std::vector<VMValue> &constantPool);

        VMValue execute(const std::string &entryPoint = "main");

        void disassemble() const;

    private:
        std::vector<VMValue> stack_;
        std::vector<VMValue> constantPool_;
        std::vector<std::shared_ptr<BytecodeFunction>> functionList_;
        std::unordered_map<std::string, std::shared_ptr<BytecodeFunction>> functionMap_;

        std::vector<CallFrame> callStack_;
        std::vector<VMValue> heap_; // Separate heap storage
        std::vector<NativeFunction> builtins_;
        std::vector<int> builtinArgCounts_;

        // Source range of the instruction currently executing, used to locate
        // runtime errors.
        Compiler::SourceRange currentRange_;

        void push(const VMValue &value);
        VMValue pop();

        /// \brief Stop execution before the VM can continue in an inconsistent
        /// state. Always throws a \c RuntimeErrorException carrying the current
        /// instruction's source range and the call chain that led to it.
        [[noreturn]] void trap(Compiler::RuntimeErrorKind kind, const std::string &description) const;

        /// \brief Build a traceback from the live call stack, innermost call first.
        [[nodiscard]] std::vector<Compiler::RuntimeStackFrame> buildTraceback() const;

        /// \brief Trap when \p value is uninitialized storage being read.
        void ensureInitialized(const VMValue &value, const char *what) const;
    };
} // namespace Ryntra::VM
