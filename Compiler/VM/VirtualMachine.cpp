#include "VirtualMachine.h"
#include "ErrorHandler/RuntimeError.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace Ryntra::VM {
    using Compiler::RuntimeErrorKind;
    VirtualMachine::VirtualMachine() {
        // Builtin table - index must match BytecodeGenerator::getBuiltinIndex
        builtins_ = {
            // 0: __builtin_print (generic, handles all types at runtime)
            [](const std::vector<VMValue> &args) -> VMValue {
                if (!args.empty()) {
                    if (args[0].isString()) {
                        const std::string &s = args[0].asString();
                        for (char c : s) {
                            if (c == '\0')
                                break;
                            std::cout << c;
                        }
                    } else if (args[0].isInt32()) {
                        std::cout << args[0].asInt32();
                    } else if (args[0].isInt64()) {
                        std::cout << args[0].asInt64();
                    }
                }
                return {};
            },
            // 1: __builtin_print_i32 — prints int32
            [](const std::vector<VMValue> &args) -> VMValue {
                if (!args.empty() && args[0].isInt32()) {
                    std::cout << args[0].asInt32();
                }
                return {};
            },
            // 2: __builtin_print_i64 — prints int64
            [](const std::vector<VMValue> &args) -> VMValue {
                if (!args.empty() && args[0].isInt64()) {
                    std::cout << args[0].asInt64();
                }
                return {};
            },
            // 3: __builtin_print_bool — prints "true" or "false"
            [](const std::vector<VMValue> &args) -> VMValue {
                if (!args.empty() && args[0].isInt32()) {
                    std::cout << (args[0].asInt32() ? "true" : "false");
                }
                return {};
            },
            // 4: __builtin_print_string — prints string
            [](const std::vector<VMValue> &args) -> VMValue {
                if (!args.empty() && args[0].isString()) {
                    const std::string &s = args[0].asString();
                    for (char c : s) {
                        if (c == '\0')
                            break;
                        std::cout << c;
                    }
                }
                return VMValue();
            },
            // 5: __builtin_scan_bool — reads bool from stdin
            [](const std::vector<VMValue> &args) -> VMValue {
                std::string input;
                std::cin >> input;
                std::transform(input.begin(), input.end(), input.begin(), ::tolower);
                if (input == "true") {
                    return VMValue(static_cast<int32_t>(1));
                } else if (input == "false") {
                    return VMValue(static_cast<int32_t>(0));
                } else {
                    try {
                        int32_t val = std::stoi(input);
                        return VMValue(val != 0 ? static_cast<int32_t>(1) : static_cast<int32_t>(0));
                    } catch (...) {
                        return VMValue(static_cast<int32_t>(0));
                    }
                }
            },
            // 6: __builtin_scan_i32 — reads int32 from stdin
            [](const std::vector<VMValue> &args) -> VMValue {
                int32_t val;
                std::cin >> val;
                return VMValue(val);
            },
            // 7: __builtin_scan_i64 — reads int64 from stdin
            [](const std::vector<VMValue> &args) -> VMValue {
                int64_t val;
                std::cin >> val;
                return VMValue(val);
            },
        };

        builtinArgCounts_ = {1, 1, 1, 1, 1, 0, 0, 0}; // arg count per builtin index
    }

    void VirtualMachine::load(const std::vector<std::shared_ptr<BytecodeFunction>> &funcs,
                              const std::vector<VMValue> &constantPool) {
        functionList_ = funcs;
        constantPool_ = constantPool;
        functionMap_.clear();
        for (const auto &f : funcs) {
            functionMap_[f->name] = f;
        }
    }

    VMValue VirtualMachine::execute(const std::string &entryPoint) {
        auto it = functionMap_.find(entryPoint);
        if (it == functionMap_.end()) {
            trap(RuntimeErrorKind::InvalidFunction, "entry point not found: " + entryPoint);
        }

        callStack_.clear();
        stack_.clear();

        callStack_.push_back(CallFrame{
            it->second.get(),
            0,
            std::vector<VMValue>(it->second->paramCount),
            0
        });

        VMValue result;

        while (!callStack_.empty()) {
            auto &frame = callStack_.back();

            if (frame.ip >= frame.func->instructions.size()) {
                callStack_.pop_back();
                continue;
            }

            const auto &inst = frame.func->instructions[frame.ip];
            currentRange_ = inst.range;

            switch (inst.opcode) {
            case OpCode::LoadConst: {
                if (inst.operand >= 0 && inst.operand < static_cast<int32_t>(constantPool_.size())) {
                    push(constantPool_[inst.operand]);
                }
                break;
            }

            case OpCode::LoadFunc: {
                if (inst.operand < 0 || inst.operand >= static_cast<int32_t>(functionList_.size())) {
                    trap(RuntimeErrorKind::InvalidFunction,
                         "invalid function index: " + std::to_string(inst.operand));
                }
                VMValue funcVal;
                funcVal.setFunctionIndex(inst.operand);
                push(funcVal);
                break;
            }

            case OpCode::Call: {
                if (inst.operand < 0 || inst.operand >= static_cast<int32_t>(functionList_.size())) {
                    trap(RuntimeErrorKind::InvalidFunction,
                         "invalid function index: " + std::to_string(inst.operand));
                }
                auto *callee = functionList_[inst.operand].get();

                size_t argCount = static_cast<size_t>(callee->paramCount);

                std::vector<VMValue> callArgs(argCount);
                for (int i = static_cast<int>(argCount) - 1; i >= 0; --i) {
                    callArgs[i] = pop();
                }

                CallFrame newFrame{callee, 0, std::move(callArgs), stack_.size()};
                newFrame.callerName = frame.func->name;
                newFrame.callSiteRange = currentRange_;
                ++frame.ip;
                callStack_.push_back(std::move(newFrame));
                continue;
            }

            case OpCode::ICall: {
                auto calleeVal = pop();
                if (!calleeVal.isFunctionPtr()) {
                    trap(RuntimeErrorKind::TypeMismatch, "ICall: callee is not a function pointer");
                }
                int32_t calleeIdx = calleeVal.getFunctionIndex();
                if (calleeIdx < 0 || calleeIdx >= static_cast<int32_t>(functionList_.size())) {
                    trap(RuntimeErrorKind::InvalidFunction,
                         "ICall: invalid function index: " + std::to_string(calleeIdx));
                }
                auto *callee = functionList_[calleeIdx].get();

                size_t argCount = static_cast<size_t>(callee->paramCount);
                std::vector<VMValue> callArgs(argCount);
                for (int i = static_cast<int>(argCount) - 1; i >= 0; --i) {
                    callArgs[i] = pop();
                }

                CallFrame newFrame{callee, 0, std::move(callArgs), stack_.size()};
                newFrame.callerName = frame.func->name;
                newFrame.callSiteRange = currentRange_;
                ++frame.ip;
                callStack_.push_back(std::move(newFrame));
                continue;
            }

            case OpCode::BCall: {
                if (inst.operand < 0 || inst.operand >= static_cast<int32_t>(builtins_.size())) {
                    trap(RuntimeErrorKind::InvalidBuiltin,
                         "invalid builtin index: " + std::to_string(inst.operand));
                }
                size_t argCount = static_cast<size_t>(builtinArgCounts_[inst.operand]);
                std::vector<VMValue> callArgs(argCount);
                for (int i = static_cast<int>(argCount) - 1; i >= 0; --i) {
                    callArgs[i] = pop();
                }
                VMValue builtinResult = builtins_[inst.operand](callArgs);
                if (!builtinResult.isVoid())
                    push(builtinResult);
                break;
            }

            case OpCode::Return: {
                VMValue retVal;
                if (!stack_.empty()) {
                    retVal = pop();
                }
                size_t retStackBase = frame.stackBase;
                callStack_.pop_back();
                if (callStack_.empty()) {
                    result = retVal;
                } else {
                    stack_.resize(retStackBase);
                    if (!retVal.isVoid()) {
                        push(retVal);
                    }
                }
                continue;
            }

            case OpCode::Add: {
                auto b = pop();
                auto a = pop();
                ensureInitialized(a, "Use of an uninitialized value in '+'");
                ensureInitialized(b, "Use of an uninitialized value in '+'");
                if (a.isInt64() && b.isInt64())
                    push(VMValue(a.asInt64() + b.asInt64()));
                else if (a.isInt32() && b.isInt32())
                    push(VMValue(a.asInt32() + b.asInt32()));
                else if (a.isPointer() && b.isInt32()) {
                    if (a.isArrayPointer()) {
                        VMValue result;
                        result.setArrayPointer(a.getPointerSlot() + b.asInt32(), a.getArrayPointerData());
                        push(result);
                    } else {
                        push(VMValue(a.getPointerSlot() + b.asInt32()));
                    }
                } else if (a.isPointer() && b.isInt64()) {
                    auto offset = static_cast<int32_t>(b.asInt64());
                    if (a.isArrayPointer()) {
                        VMValue result;
                        result.setArrayPointer(a.getPointerSlot() + offset, a.getArrayPointerData());
                        push(result);
                    } else {
                        push(VMValue(a.getPointerSlot() + offset));
                    }
                } else if (a.isInt32() && b.isPointer()) {
                    if (b.isArrayPointer()) {
                        VMValue result;
                        result.setArrayPointer(a.asInt32() + b.getPointerSlot(), b.getArrayPointerData());
                        push(result);
                    } else {
                        push(VMValue(a.asInt32() + b.getPointerSlot()));
                    }
                } else if (a.isHeapPointer() && b.isInt32()) {
                    VMValue result;
                    result.setHeapPointerSlot(a.getHeapPointerSlot() + b.asInt32());
                    push(result);
                } else if (a.isHeapPointer() && b.isInt64()) {
                    VMValue result;
                    result.setHeapPointerSlot(a.getHeapPointerSlot() + static_cast<int32_t>(b.asInt64()));
                    push(result);
                } else if (a.isInt32() && b.isHeapPointer()) {
                    VMValue result;
                    result.setHeapPointerSlot(a.asInt32() + b.getHeapPointerSlot());
                    push(result);
                }
                break;
            }
            case OpCode::Sub: {
                auto b = pop();
                auto a = pop();
                ensureInitialized(a, "Use of an uninitialized value in '-'");
                ensureInitialized(b, "Use of an uninitialized value in '-'");
                if (a.isInt64() && b.isInt64())
                    push(VMValue(a.asInt64() - b.asInt64()));
                else if (a.isInt32() && b.isInt32())
                    push(VMValue(a.asInt32() - b.asInt32()));
                else if (a.isPointer() && b.isInt32()) {
                    if (a.isArrayPointer()) {
                        VMValue result;
                        result.setArrayPointer(a.getPointerSlot() - b.asInt32(), a.getArrayPointerData());
                        push(result);
                    } else {
                        push(VMValue(a.getPointerSlot() - b.asInt32()));
                    }
                } else if (a.isPointer() && b.isPointer())
                    push(VMValue(a.getPointerSlot() - b.getPointerSlot()));
                else if (a.isHeapPointer() && b.isInt32()) {
                    VMValue result;
                    result.setHeapPointerSlot(a.getHeapPointerSlot() - b.asInt32());
                    push(result);
                } else if (a.isHeapPointer() && b.isHeapPointer())
                    push(VMValue(a.getHeapPointerSlot() - b.getHeapPointerSlot()));
                break;
            }
            case OpCode::Mul: {
                auto b = pop();
                auto a = pop();
                ensureInitialized(a, "Use of an uninitialized value in '*'");
                ensureInitialized(b, "Use of an uninitialized value in '*'");
                if (a.isInt64() && b.isInt64())
                    push(VMValue(a.asInt64() * b.asInt64()));
                else if (a.isInt32() && b.isInt32())
                    push(VMValue(a.asInt32() * b.asInt32()));
                break;
            }
            case OpCode::Div: {
                auto b = pop();
                auto a = pop();
                ensureInitialized(a, "Use of an uninitialized value in '/'");
                ensureInitialized(b, "Use of an uninitialized value in '/'");
                if (a.isInt64() && b.isInt64() && b.asInt64() != 0)
                    push(VMValue(a.asInt64() / b.asInt64()));
                else if (a.isInt32() && b.isInt32() && b.asInt32() != 0)
                    push(VMValue(a.asInt32() / b.asInt32()));
                break;
            }
            case OpCode::Mod: {
                auto b = pop();
                auto a = pop();
                ensureInitialized(a, "Use of an uninitialized value in '%'");
                ensureInitialized(b, "Use of an uninitialized value in '%'");
                if (a.isInt64() && b.isInt64() && b.asInt64() != 0)
                    push(VMValue(a.asInt64() % b.asInt64()));
                else if (a.isInt32() && b.isInt32() && b.asInt32() != 0)
                    push(VMValue(a.asInt32() % b.asInt32()));
                break;
            }

            case OpCode::BitNot: {
                auto a = pop();
                ensureInitialized(a, "Use of an uninitialized value in '~'");
                if (a.isInt64())
                    push(VMValue(~a.asInt64()));
                else if (a.isInt32())
                    push(VMValue(~a.asInt32()));
                break;
            }
            case OpCode::LogicalNot: {
                auto a = pop();
                ensureInitialized(a, "Use of an uninitialized value in '!'");
                if (a.isInt32())
                    push(VMValue(a.asInt32() == 0 ? 1 : 0));
                else if (a.isInt64())
                    push(VMValue(a.asInt64() == 0 ? 1 : 0));
                break;
            }
            case OpCode::BitAnd: {
                auto b = pop();
                auto a = pop();
                if (a.isInt64() && b.isInt64())
                    push(VMValue(a.asInt64() & b.asInt64()));
                else if (a.isInt32() && b.isInt32())
                    push(VMValue(a.asInt32() & b.asInt32()));
                break;
            }
            case OpCode::BitOr: {
                auto b = pop();
                auto a = pop();
                if (a.isInt64() && b.isInt64())
                    push(VMValue(a.asInt64() | b.asInt64()));
                else if (a.isInt32() && b.isInt32())
                    push(VMValue(a.asInt32() | b.asInt32()));
                break;
            }
            case OpCode::BitXor: {
                auto b = pop();
                auto a = pop();
                if (a.isInt64() && b.isInt64())
                    push(VMValue(a.asInt64() ^ b.asInt64()));
                else if (a.isInt32() && b.isInt32())
                    push(VMValue(a.asInt32() ^ b.asInt32()));
                break;
            }
            case OpCode::Shl: {
                auto b = pop();
                auto a = pop();
                if (a.isInt64() && b.isInt64())
                    push(VMValue(a.asInt64() << (b.asInt64() & 63)));
                else if (a.isInt32() && b.isInt32())
                    push(VMValue(a.asInt32() << (b.asInt32() & 31)));
                break;
            }
            case OpCode::Shr: {
                auto b = pop();
                auto a = pop();
                if (a.isInt64() && b.isInt64())
                    push(VMValue(a.asInt64() >> (b.asInt64() & 63)));
                else if (a.isInt32() && b.isInt32())
                    push(VMValue(a.asInt32() >> (b.asInt32() & 31)));
                break;
            }

            case OpCode::SExt: {
                auto a = pop();
                if (a.isInt32()) {
                    push(VMValue(static_cast<int64_t>(a.asInt32())));
                } else {
                    push(a);
                }
                break;
            }

            case OpCode::Trunc: {
                auto a = pop();
                if (a.isInt64()) {
                    push(VMValue(static_cast<int32_t>(a.asInt64())));
                } else {
                    push(a);
                }
                break;
            }

            case OpCode::Eq: {
                auto b = pop();
                auto a = pop();
                if (a.isInt64() && b.isInt64())
                    push(VMValue(static_cast<int32_t>(a.asInt64() == b.asInt64())));
                else if (a.isInt32() && b.isInt32())
                    push(VMValue(static_cast<int32_t>(a.asInt32() == b.asInt32())));
                else if (a.isPointer() && b.isInt32())
                    push(VMValue(static_cast<int32_t>(a.getPointerSlot() == b.asInt32())));
                else if (a.isInt32() && b.isPointer())
                    push(VMValue(static_cast<int32_t>(a.asInt32() == b.getPointerSlot())));
                else if (a.isPointer() && b.isPointer()) {
                    if (a.isArrayPointer() && b.isArrayPointer()) {
                        bool eq = (a.getArrayPointerData() == b.getArrayPointerData()) &&
                                  (a.getPointerSlot() == b.getPointerSlot());
                        push(VMValue(static_cast<int32_t>(eq)));
                    } else if (!a.isArrayPointer() && !b.isArrayPointer()) {
                        push(VMValue(static_cast<int32_t>(a.getPointerSlot() == b.getPointerSlot())));
                    } else {
                        push(VMValue(static_cast<int32_t>(0)));
                    }
                } else if (a.isHeapPointer() && b.isInt32())
                    push(VMValue(static_cast<int32_t>(a.getHeapPointerSlot() == b.asInt32())));
                else if (a.isInt32() && b.isHeapPointer())
                    push(VMValue(static_cast<int32_t>(a.asInt32() == b.getHeapPointerSlot())));
                else if (a.isHeapPointer() && b.isHeapPointer())
                    push(VMValue(static_cast<int32_t>(a.getHeapPointerSlot() == b.getHeapPointerSlot())));
                break;
            }
            case OpCode::Ne: {
                auto b = pop();
                auto a = pop();
                if (a.isInt64() && b.isInt64())
                    push(VMValue(static_cast<int32_t>(a.asInt64() != b.asInt64())));
                else if (a.isInt32() && b.isInt32())
                    push(VMValue(static_cast<int32_t>(a.asInt32() != b.asInt32())));
                else if (a.isPointer() && b.isInt32())
                    push(VMValue(static_cast<int32_t>(a.getPointerSlot() != b.asInt32())));
                else if (a.isInt32() && b.isPointer())
                    push(VMValue(static_cast<int32_t>(a.asInt32() != b.getPointerSlot())));
                else if (a.isPointer() && b.isPointer()) {
                    if (a.isArrayPointer() && b.isArrayPointer()) {
                        bool ne = (a.getArrayPointerData() != b.getArrayPointerData()) ||
                                  (a.getPointerSlot() != b.getPointerSlot());
                        push(VMValue(static_cast<int32_t>(ne)));
                    } else if (!a.isArrayPointer() && !b.isArrayPointer()) {
                        push(VMValue(static_cast<int32_t>(a.getPointerSlot() != b.getPointerSlot())));
                    } else {
                        push(VMValue(static_cast<int32_t>(1)));
                    }
                } else if (a.isHeapPointer() && b.isInt32())
                    push(VMValue(static_cast<int32_t>(a.getHeapPointerSlot() != b.asInt32())));
                else if (a.isInt32() && b.isHeapPointer())
                    push(VMValue(static_cast<int32_t>(a.asInt32() != b.getHeapPointerSlot())));
                else if (a.isHeapPointer() && b.isHeapPointer())
                    push(VMValue(static_cast<int32_t>(a.getHeapPointerSlot() != b.getHeapPointerSlot())));
                break;
            }
            case OpCode::Lt: {
                auto b = pop();
                auto a = pop();
                if (a.isInt64() && b.isInt64())
                    push(VMValue(static_cast<int32_t>(a.asInt64() < b.asInt64())));
                else if (a.isInt32() && b.isInt32())
                    push(VMValue(static_cast<int32_t>(a.asInt32() < b.asInt32())));
                break;
            }
            case OpCode::Gt: {
                auto b = pop();
                auto a = pop();
                if (a.isInt64() && b.isInt64())
                    push(VMValue(static_cast<int32_t>(a.asInt64() > b.asInt64())));
                else if (a.isInt32() && b.isInt32())
                    push(VMValue(static_cast<int32_t>(a.asInt32() > b.asInt32())));
                break;
            }
            case OpCode::Le: {
                auto b = pop();
                auto a = pop();
                if (a.isInt64() && b.isInt64())
                    push(VMValue(static_cast<int32_t>(a.asInt64() <= b.asInt64())));
                else if (a.isInt32() && b.isInt32())
                    push(VMValue(static_cast<int32_t>(a.asInt32() <= b.asInt32())));
                break;
            }
            case OpCode::Ge: {
                auto b = pop();
                auto a = pop();
                if (a.isInt64() && b.isInt64())
                    push(VMValue(static_cast<int32_t>(a.asInt64() >= b.asInt64())));
                else if (a.isInt32() && b.isInt32())
                    push(VMValue(static_cast<int32_t>(a.asInt32() >= b.asInt32())));
                break;
            }
            case OpCode::Dup: {
                if (!stack_.empty()) {
                    push(stack_.back());
                }
                break;
            }

            case OpCode::Pop:
                if (!stack_.empty())
                    pop();
                break;

            case OpCode::StoreLocal: {
                auto val = pop();
                int32_t idx = inst.operand;
                if (idx >= static_cast<int32_t>(frame.locals.size()))
                    frame.locals.resize(idx + 1);
                frame.locals[idx] = val;
                break;
            }

            case OpCode::LoadLocal: {
                int32_t idx = inst.operand;
                if (idx < 0) {
                    trap(RuntimeErrorKind::InvalidOperation,
                         "LoadLocal: negative slot index " + std::to_string(idx));
                }
                if (idx >= static_cast<int32_t>(frame.locals.size())) {
                    frame.locals.resize(static_cast<size_t>(idx) + 1, VMValue::uninitialized());
                }
                ensureInitialized(frame.locals[idx],
                                  "Use of an uninitialized local variable");
                push(frame.locals[idx]);
                break;
            }

            case OpCode::Jmp:
                frame.ip = static_cast<size_t>(inst.operand);
                continue;

            case OpCode::Jz: {
                auto val = pop();
                if (val.isInt32() && val.asInt32() == 0) {
                    frame.ip = static_cast<size_t>(inst.operand);
                    continue;
                }
                if (val.isInt64() && val.asInt64() == 0) {
                    frame.ip = static_cast<size_t>(inst.operand);
                    continue;
                }
                break;
            }

            case OpCode::NewArray: {
                auto sizeVal = pop();
                int32_t size = 0;
                if (sizeVal.isInt32())
                    size = sizeVal.asInt32();
                else if (sizeVal.isInt64())
                    size = static_cast<int32_t>(sizeVal.asInt64());
                auto arrData = std::make_shared<ArrayData>();
                arrData->elements.resize(size, VMValue(static_cast<int32_t>(0)));
                push(VMValue(arrData));
                break;
            }

            case OpCode::ArrGet: {
                auto idxVal = pop();
                auto arrVal = pop();
                if (!arrVal.isArray()) {
                    trap(RuntimeErrorKind::TypeMismatch, "ArrGet on non-array value");
                }
                auto arrData = arrVal.asArray();
                int32_t idx = 0;
                if (idxVal.isInt32())
                    idx = idxVal.asInt32();
                else if (idxVal.isInt64())
                    idx = static_cast<int32_t>(idxVal.asInt64());
                if (idx < 0 || static_cast<size_t>(idx) >= arrData->elements.size())
                    trap(RuntimeErrorKind::InvalidIndex,
                         "array index out of bounds: " + std::to_string(idx));
                ensureInitialized(arrData->elements[idx],
                                  "Use of an uninitialized array element");
                push(arrData->elements[idx]);
                break;
            }

            case OpCode::ArrSet: {
                auto val = pop();
                auto idxVal = pop();
                auto arrVal = pop();
                if (!arrVal.isArray()) {
                    trap(RuntimeErrorKind::TypeMismatch, "ArrSet on non-array value");
                }
                auto arrData = arrVal.asArray();
                int32_t idx = 0;
                if (idxVal.isInt32())
                    idx = idxVal.asInt32();
                else if (idxVal.isInt64())
                    idx = static_cast<int32_t>(idxVal.asInt64());
                if (idx < 0 || static_cast<size_t>(idx) >= arrData->elements.size())
                    trap(RuntimeErrorKind::InvalidIndex,
                         "array index out of bounds: " + std::to_string(idx));
                arrData->elements[idx] = val;
                break;
            }

            case OpCode::Halt:
                callStack_.clear();
                continue;

            case OpCode::RefCreate: {
                auto slotVal = pop();
                if (!slotVal.isInt32()) {
                    trap(RuntimeErrorKind::InvalidArgument,
                         "RefCreate requires an int32 slot index");
                }
                VMValue refVal;
                refVal.setReferenceSlot(slotVal.asInt32());
                push(refVal);
                break;
            }

            case OpCode::RefLoad: {
                auto refVal = pop();
                if (refVal.isArrayElementRef()) {
                    auto elemRef = refVal.asArrayElementRef();
                    if (elemRef.index >= 0 && static_cast<size_t>(elemRef.index) < elemRef.array->elements.size()) {
                        ensureInitialized(elemRef.array->elements[elemRef.index],
                                          "Use of an uninitialized array element");
                        push(elemRef.array->elements[elemRef.index]);
                    } else {
                        trap(RuntimeErrorKind::InvalidReference,
                             "RefLoad: invalid array element ref index");
                    }
                } else if (refVal.isReference()) {
                    int32_t slot = refVal.getReferenceSlot();
                    if (slot >= 0) {
                        if (slot >= static_cast<int32_t>(frame.locals.size()))
                            frame.locals.resize(static_cast<size_t>(slot) + 1, VMValue::uninitialized());
                        ensureInitialized(frame.locals[slot],
                                          "Use of an uninitialized referenced variable");
                        push(frame.locals[slot]);
                    } else {
                        trap(RuntimeErrorKind::InvalidReference,
                             "RefLoad: invalid reference slot " + std::to_string(slot));
                    }
                } else {
                    trap(RuntimeErrorKind::TypeMismatch, "RefLoad on non-reference value");
                }
                break;
            }

            case OpCode::RefStore: {
                auto val = pop();
                auto refVal = pop();
                if (refVal.isArrayElementRef()) {
                    auto elemRef = refVal.asArrayElementRef();
                    if (elemRef.index >= 0 && static_cast<size_t>(elemRef.index) < elemRef.array->elements.size()) {
                        elemRef.array->elements[elemRef.index] = val;
                    } else {
                        trap(RuntimeErrorKind::InvalidReference,
                             "RefStore: invalid array element ref index");
                    }
                } else if (refVal.isReference()) {
                    int32_t slot = refVal.getReferenceSlot();
                    if (slot >= 0) {
                        if (slot >= static_cast<int32_t>(frame.locals.size()))
                            frame.locals.resize(static_cast<size_t>(slot) + 1, VMValue::uninitialized());
                        frame.locals[slot] = val;
                    } else {
                        trap(RuntimeErrorKind::InvalidReference,
                             "RefStore: invalid reference slot " + std::to_string(slot));
                    }
                } else {
                    trap(RuntimeErrorKind::TypeMismatch, "RefStore on non-reference value");
                }
                break;
            }

            case OpCode::PtrCreate: {
                auto slotVal = pop();
                if (!slotVal.isInt32()) {
                    trap(RuntimeErrorKind::InvalidArgument,
                         "PtrCreate requires an int32 slot index");
                }
                VMValue ptrVal;
                ptrVal.setPointerSlot(slotVal.asInt32());
                push(ptrVal);
                break;
            }

            case OpCode::PtrLoad: {
                auto ptrVal = pop();
                if (ptrVal.isStructFieldRef()) {
                    auto ref = ptrVal.asStructFieldRef();
                    if (ref.data->inBounds(ref.offset)) {
                        ensureInitialized(ref.data->at(ref.offset),
                                          "Use of an uninitialized struct field");
                        push(ref.data->at(ref.offset));
                    } else {
                        trap(RuntimeErrorKind::InvalidPointer,
                             "PtrLoad: invalid struct field offset " + std::to_string(ref.offset));
                    }
                } else if (ptrVal.isStruct()) {
                    // Loading a whole struct yields the aggregate handle itself.
                    push(ptrVal);
                } else if (ptrVal.isHeapPointer()) {
                    int32_t slot = ptrVal.getHeapPointerSlot();
                    if (slot >= 0 && slot < static_cast<int32_t>(heap_.size())) {
                        ensureInitialized(heap_[slot], "use of uninitialized heap storage");
                        push(heap_[slot]);
                    } else {
                        trap(RuntimeErrorKind::InvalidPointer,
                             "PtrLoad: invalid heap pointer slot " + std::to_string(slot));
                    }
                } else if (ptrVal.isPointer()) {
                    if (ptrVal.isArrayPointer()) {
                        auto arrData = ptrVal.getArrayPointerData();
                        int32_t index = ptrVal.getPointerSlot();
                        if (index >= 0 && static_cast<size_t>(index) < arrData->elements.size()) {
                            ensureInitialized(arrData->elements[index],
                                              "Use of an uninitialized array element");
                            push(arrData->elements[index]);
                        } else {
                            trap(RuntimeErrorKind::InvalidPointer,
                                 "PtrLoad: invalid array element index");
                        }
                    } else {
                        int32_t slot = ptrVal.getPointerSlot();
                        if (slot >= 0) {
                            if (slot >= static_cast<int32_t>(frame.locals.size()))
                                frame.locals.resize(static_cast<size_t>(slot) + 1, VMValue::uninitialized());
                            ensureInitialized(frame.locals[slot],
                                              "Use of an uninitialized variable");
                            push(frame.locals[slot]);
                        } else {
                            trap(RuntimeErrorKind::InvalidPointer,
                                 "PtrLoad: invalid pointer slot " + std::to_string(slot));
                        }
                    }
                } else {
                    trap(RuntimeErrorKind::TypeMismatch, "PtrLoad on non-pointer value");
                }
                break;
            }

            case OpCode::PtrStore: {
                auto val = pop();
                auto ptrVal = pop();
                if (ptrVal.isStructFieldRef()) {
                    auto ref = ptrVal.asStructFieldRef();
                    if (ref.data->inBounds(ref.offset)) {
                        ref.data->at(ref.offset) = val;
                    } else {
                        trap(RuntimeErrorKind::InvalidPointer,
                             "PtrStore: invalid struct field offset " + std::to_string(ref.offset));
                    }
                } else if (ptrVal.isStruct()) {
                    // Assigning to a whole struct copies field values.
                    if (!val.isStruct()) {
                        trap(RuntimeErrorKind::TypeMismatch,
                             "PtrStore: cannot assign non-struct to struct");
                    }
                    ptrVal.asStruct()->assignFrom(*val.asStruct());
                } else if (ptrVal.isHeapPointer()) {
                    int32_t slot = ptrVal.getHeapPointerSlot();
                    if (slot >= 0 && slot < static_cast<int32_t>(heap_.size())) {
                        heap_[slot] = val;
                    } else {
                        trap(RuntimeErrorKind::InvalidPointer,
                             "PtrStore: invalid heap pointer slot " + std::to_string(slot));
                    }
                } else if (ptrVal.isPointer()) {
                    if (ptrVal.isArrayPointer()) {
                        auto arrData = ptrVal.getArrayPointerData();
                        int32_t index = ptrVal.getPointerSlot();
                        if (index >= 0 && static_cast<size_t>(index) < arrData->elements.size()) {
                            arrData->elements[index] = val;
                        } else {
                            trap(RuntimeErrorKind::InvalidPointer,
                                 "PtrStore: invalid array element index");
                        }
                    } else {
                        int32_t slot = ptrVal.getPointerSlot();
                        if (slot >= 0) {
                            if (slot >= static_cast<int32_t>(frame.locals.size()))
                                frame.locals.resize(static_cast<size_t>(slot) + 1, VMValue::uninitialized());
                            frame.locals[slot] = val;
                        } else {
                            trap(RuntimeErrorKind::InvalidPointer,
                                 "PtrStore: invalid pointer slot " + std::to_string(slot));
                        }
                    }
                } else {
                    trap(RuntimeErrorKind::TypeMismatch, "PtrStore on non-pointer value");
                }
                break;
            }

            case OpCode::New: {
                auto initVal = pop();
                heap_.push_back(initVal);
                VMValue heapPtr;
                heapPtr.setHeapPointerSlot(static_cast<int32_t>(heap_.size() - 1));
                push(heapPtr);
                break;
            }

            case OpCode::Delete: {
                auto ptrVal = pop();
                if (ptrVal.isHeapPointer()) {
                    int32_t slot = ptrVal.getHeapPointerSlot();
                    if (slot >= 0 && slot < static_cast<int32_t>(heap_.size())) {
                        heap_[slot] = VMValue(); // mark as freed
                    }
                }
                break;
            }

            case OpCode::ArrRef: {
                auto indexVal = pop();
                auto arrVal = pop();
                if (!arrVal.isArray()) {
                    trap(RuntimeErrorKind::TypeMismatch, "ArrRef on non-array value");
                }
                auto arrData = arrVal.asArray();
                int32_t idx = 0;
                if (indexVal.isInt32())
                    idx = indexVal.asInt32();
                else if (indexVal.isInt64())
                    idx = static_cast<int32_t>(indexVal.asInt64());
                if (idx < 0 || static_cast<size_t>(idx) >= arrData->elements.size())
                    trap(RuntimeErrorKind::InvalidIndex,
                         "ArrRef: array index out of bounds: " + std::to_string(idx));
                VMValue refVal;
                refVal = VMValue(ArrayElementRef{arrData, idx});
                push(refVal);
                break;
            }

            case OpCode::PtrIndexRef: {
                auto indexVal = pop();
                auto ptrVal = pop();
                int32_t idx = 0;
                if (indexVal.isInt32())
                    idx = indexVal.asInt32();
                else if (indexVal.isInt64())
                    idx = static_cast<int32_t>(indexVal.asInt64());

                if (ptrVal.isHeapPointer()) {
                    int32_t slot = ptrVal.getHeapPointerSlot();
                    int32_t targetSlot = slot + idx;
                    if (targetSlot >= 0 && targetSlot < static_cast<int32_t>(heap_.size())) {
                        // TODO: Heap pointer isn't implement
                        trap(RuntimeErrorKind::Internal,
                             "PtrIndexRef for heap pointers not yet implemented");
                    }
                } else if (ptrVal.isPointer()) {
                    if (ptrVal.isArrayPointer()) {
                        int32_t index = ptrVal.getPointerSlot() + idx;
                        VMValue refVal(ArrayElementRef{ptrVal.getArrayPointerData(), index});
                        push(refVal);
                    } else {
                        int32_t slot = ptrVal.getPointerSlot();
                        int32_t targetSlot = slot + idx;
                        VMValue refVal;
                        refVal.setReferenceSlot(targetSlot);
                        push(refVal);
                    }
                } else {
                    trap(RuntimeErrorKind::TypeMismatch, "PtrIndexRef on non-pointer value");
                }
                break;
            }

            case OpCode::PinArray:
            case OpCode::UnpinArray: {
                // TODO: No GC yet
                pop();
                break;
            }

            case OpCode::PtrFromArray: {
                auto arrVal = pop();
                if (!arrVal.isArray()) {
                    trap(RuntimeErrorKind::TypeMismatch, "PtrFromArray on non-array value");
                }
                auto arrData = arrVal.asArray();
                VMValue ptrVal;
                ptrVal.setArrayPointer(0, arrData);
                push(ptrVal);
                break;
            }

            case OpCode::NewStruct: {
                // operand = size in bytes, operand2 = alignment in bytes. The
                // aligned allocation is handled by StructData itself.
                auto data = std::make_shared<StructData>(inst.operand, inst.operand2);
                push(VMValue(data));
                break;
            }

            case OpCode::FieldRef: {
                auto base = pop();
                std::shared_ptr<StructData> data;

                if (base.isStruct()) {
                    data = base.asStruct();
                } else if (base.isStructFieldRef()) {
                    auto ref = base.asStructFieldRef();
                    if (ref.data->inBounds(ref.offset) &&
                        ref.data->at(ref.offset).isStruct()) {
                        data = ref.data->at(ref.offset).asStruct();
                    } else {
                        trap(RuntimeErrorKind::TypeMismatch,
                             "FieldRef: nested field is not a struct");
                    }
                } else if (base.isPointer() && !base.isArrayPointer()) {
                    int32_t slot = base.getPointerSlot();
                    if (slot >= 0 && static_cast<size_t>(slot) < frame.locals.size() &&
                        frame.locals[slot].isStruct()) {
                        data = frame.locals[slot].asStruct();
                    } else {
                        trap(RuntimeErrorKind::TypeMismatch, "FieldRef: base is not a struct");
                    }
                } else if (base.isHeapPointer()) {
                    int32_t slot = base.getHeapPointerSlot();
                    if (slot >= 0 && static_cast<size_t>(slot) < heap_.size() && heap_[slot].isStruct()) {
                        data = heap_[slot].asStruct();
                    } else {
                        trap(RuntimeErrorKind::TypeMismatch, "FieldRef: base is not a struct");
                    }
                } else {
                    trap(RuntimeErrorKind::TypeMismatch, "FieldRef on non-struct value");
                }

                // The operand is the byte offset produced by the compiler's Struct
                // Layout phase; FieldRef never computes an offset itself.
                if (!data->inBounds(inst.operand)) {
                    trap(RuntimeErrorKind::InvalidPointer,
                         "FieldRef: field offset out of range");
                }
                push(VMValue(StructFieldRef{data, inst.operand}));
                break;
            }

            default:
                break;
            }

            ++frame.ip;
        }

        return result;
    }

    static const char *opcodeNames[] = {
        "LoadConst",
        "LoadFunc",
        "Call",
        "ICall",
        "BCall",
        "Return",
        "Add",
        "Sub",
        "Mul",
        "Div",
        "Mod",
        "BitNot",
        "LogicalNot",
        "BitAnd",
        "BitOr",
        "BitXor",
        "Shl",
        "Shr",
        "SExt",
        "Trunc",
        "Eq",
        "Ne",
        "Lt",
        "Gt",
        "Le",
        "Ge",
        "Dup",
        "Pop",
        "StoreLocal",
        "LoadLocal",
        "Jmp",
        "Jz",
        "NewArray",
        "ArrGet",
        "ArrSet",
        "RefCreate",
        "RefLoad",
        "RefStore",
        "PtrCreate",
        "PtrLoad",
        "PtrStore",
        "New",
        "Delete",
        "ArrRef",
        "PtrIndexRef",
        "PinArray",
        "UnpinArray",
        "PtrFromArray",
        "NewStruct",
        "FieldRef",
        "Halt",
    };

    void VirtualMachine::disassemble() const {
        for (const auto &func : functionList_) {
            std::cout << "function " << func->name
                      << " (paramCount=" << func->paramCount
                      << ", external=" << (func->isExternal ? "true" : "false") << "):\n";
            if (func->instructions.empty()) {
                std::cout << "  (no instructions)\n";
            } else {
                for (size_t i = 0; i < func->instructions.size(); ++i) {
                    const auto &inst = func->instructions[i];
                    uint8_t idx = static_cast<uint8_t>(inst.opcode);
                    const char *name = (idx < sizeof(opcodeNames) / sizeof(opcodeNames[0]))
                                           ? opcodeNames[idx]
                                           : "???";
                    std::cout << "  " << i << ": " << name;
                    if (inst.opcode == OpCode::NewStruct) {
                        std::cout << " size=" << inst.operand << " align=" << inst.operand2;
                    } else if (inst.opcode == OpCode::LoadConst ||
                               inst.opcode == OpCode::LoadFunc ||
                               inst.opcode == OpCode::StoreLocal ||
                               inst.opcode == OpCode::LoadLocal ||
                               inst.opcode == OpCode::Jmp ||
                               inst.opcode == OpCode::Jz ||
                               inst.opcode == OpCode::RefCreate ||
                               inst.opcode == OpCode::PtrCreate ||
                               inst.opcode == OpCode::FieldRef) {
                        std::cout << " " << inst.operand;
                    } else if (inst.opcode == OpCode::Call || inst.opcode == OpCode::BCall) {
                        std::cout << " " << inst.operand;
                    }
                    std::cout << "\n";
                }
            }
            std::cout << "\n";
        }
    }

    void VirtualMachine::push(const VMValue &value) {
        stack_.push_back(value);
    }

    VMValue VirtualMachine::pop() {
        if (stack_.empty())
            trap(RuntimeErrorKind::StackUnderflow, "stack underflow while reading an operand");
        VMValue v = stack_.back();
        stack_.pop_back();
        return v;
    }

    std::vector<Compiler::RuntimeStackFrame> VirtualMachine::buildTraceback() const {
        std::vector<Compiler::RuntimeStackFrame> trace;
        // callStack_[0] is the entry function (no call site). Every frame above it
        // records the call that entered it; report those innermost first.
        for (size_t i = callStack_.size(); i-- > 1;) {
            const CallFrame &frame = callStack_[i];
            trace.push_back(Compiler::RuntimeStackFrame{frame.callerName, frame.callSiteRange});
        }
        return trace;
    }

    void VirtualMachine::trap(const RuntimeErrorKind kind, const std::string &description) const {
        throw Compiler::RuntimeErrorException(kind, description, currentRange_, buildTraceback());
    }

    void VirtualMachine::ensureInitialized(const VMValue &value, const char *what) const {
        // A never-assigned slot holds either an explicit Uninitialized value or a
        // default-constructed Void; both must not be read.
        if (value.isUninitialized() || value.isVoid()) {
            trap(RuntimeErrorKind::UninitializedValue, what);
        }
    }
} // namespace Ryntra::VM
