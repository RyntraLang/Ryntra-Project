#include "../SemanticAnalyzer.h"
#include "ErrorHandler/ErrorHandler.h"

namespace Ryntra::Compiler::Semantic {
    void SemanticAnalyzer::visit(FunctionCallNode &node) {
        auto funcNameNode = node.getFunctionName();
        std::string funcName = funcNameNode->getName();

        auto sym = symbolTable.resolve(funcName);

        std::vector<std::shared_ptr<ExpressionNode>> args;
        if (node.getArgumentList()) {
            args = node.getArgumentList()->getArguments();
        }
        std::vector<std::shared_ptr<TypedExpressionNode>> typedArgs;
        for (size_t i = 0; i < args.size(); ++i) {
            args[i]->accept(*this);
            auto typedArg = std::dynamic_pointer_cast<TypedExpressionNode>(lastNode);
            typedArgs.push_back(typedArg);
        }

        // Case: calling through a function pointer variable, e.g. `p_add(3, 4)`
        if (auto varSym = std::dynamic_pointer_cast<VariableSymbol>(sym)) {
            auto varSTType = varSym->getType();
            if (varSTType->getKind() == STType::TypeKind::Pointer) {
                auto &ptrSTType = static_cast<const STType::PointerType &>(*varSTType);
                if (ptrSTType.getElementType()->getKind() == STType::TypeKind::Function) {
                    auto fnTyped = toTypedType(ptrSTType.getElementType());
                    auto &fnType = static_cast<const FunctionType &>(*fnTyped);
                    const auto &paramTypes = fnType.getParamTypes();
                    auto resultType = fnType.getReturnType();

                    if (args.size() != paramTypes.size()) {
                        ErrorHandler::getInstance().makeError(
                            "[RCE070]: Function pointer '" + funcName + "' expects " +
                                std::to_string(paramTypes.size()) + " arguments, but got " +
                                std::to_string(args.size()) + ".",
                            node.getRange());
                    }

                    for (size_t i = 0; i < typedArgs.size(); ++i) {
                        auto typedArg = typedArgs[i];
                        if (!typedArg || i >= paramTypes.size())
                            continue;
                        auto actualType = typedArg->getType();
                        if (!paramTypes[i]->equals(*actualType) && actualType->toString() != "unknown") {
                            ErrorHandler::getInstance().makeError(
                                "[RCE071]: Argument " + std::to_string(i + 1) +
                                    " expects type '" + paramTypes[i]->toString() +
                                    "', but got '" + actualType->toString() + "'.",
                                args[i]->getRange());
                        }
                    }

                    auto calleeExpr = std::make_shared<TypedVariableNode>(funcName, toTypedType(varSTType));
                    calleeExpr->setRange(funcNameNode->getRange());

                    auto typedCall = std::make_shared<TypedFunctionPointerCallNode>(calleeExpr, typedArgs, resultType);
                    typedCall->setRange(node.getRange());
                    lastNode = typedCall;
                    return;
                }
            }
        }

        // Case: value construction, e.g. `Rectangle(100, 200)`. A struct type name
        // used in call position denotes a constructor call producing a struct value.
        if (auto typeSym = std::dynamic_pointer_cast<TypeSymbol>(sym)) {
            if (auto structSTType = std::dynamic_pointer_cast<STType::StructType>(typeSym->getType())) {
                std::vector<TypePtr> ctorParamSTTypes;
                bool anyDeclared = false;
                auto ctor = resolveConstructor(structSTType, typedArgs, ctorParamSTTypes,
                                               anyDeclared, node.getRange());

                if (!ctor && anyDeclared) {
                    // A constructor was declared but no overload matched; the
                    // concrete error has already been reported.
                    lastNode = nullptr;
                    return;
                }
                if (!ctor && !anyDeclared && !args.empty()) {
                    ErrorHandler::getInstance().makeError(
                        "[RCE106]: Struct '" + funcName + "' has no constructor accepting " +
                            std::to_string(args.size()) + " argument(s).",
                        node.getRange());
                    lastNode = nullptr;
                    return;
                }

                std::vector<std::shared_ptr<Type>> ctorParamTypes;
                for (const auto &pt : ctorParamSTTypes) {
                    ctorParamTypes.push_back(toTypedType(pt));
                }

                auto structType = toTypedType(structSTType);
                auto typedCtor = std::make_shared<TypedConstructorCallNode>(
                    funcName, std::move(typedArgs), std::move(ctorParamTypes),
                    ctor != nullptr, structType);
                typedCtor->setRange(node.getRange());
                lastNode = typedCtor;
                return;
            }
        }

        TypePtr stReturnType;
        std::vector<TypePtr> expectedParamTypes;

        if (!sym) {
            ErrorHandler::getInstance().makeError(
                "[RCE008]: Function '" + funcName + "' is not defined.",
                node.getRange());
            stReturnType = makeSTType("unknown");
        } else if (auto overloadSet = std::dynamic_pointer_cast<OverloadSet>(sym)) {
            bool found = false;

            if (funcName == "__builtin_scan" && expectedReturnType) {
                for (const auto &overload : overloadSet->getFunctions()) {
                    auto overloadRetTyped = toTypedType(overload->getReturnType());
                    if (overloadRetTyped->equals(*expectedReturnType)) {
                        stReturnType = overload->getReturnType();
                        expectedParamTypes = overload->getParamTypes();
                        found = true;
                        break;
                    }
                }
            }

            if (!found)
                for (const auto &overload : overloadSet->getFunctions()) {
                    if (overload->getParamTypes().size() != args.size())
                        continue;

                    bool match = true;
                    for (size_t i = 0; i < args.size(); ++i) {
                        if (!typedArgs[i]) {
                            match = false;
                            break;
                        }
                        auto expectedTyped = toTypedType(overload->getParamTypes()[i]);
                        if (!expectedTyped->equals(*typedArgs[i]->getType()) &&
                            typedArgs[i]->getType()->toString() != "unknown") {
                            match = false;
                            break;
                        }
                    }

                    if (match) {
                        stReturnType = overload->getReturnType();
                        expectedParamTypes = overload->getParamTypes();
                        found = true;
                        break;
                    }
                }

            if (!found) {
                std::string expectedTypes;
                for (const auto &overload : overloadSet->getFunctions()) {
                    if (!expectedTypes.empty())
                        expectedTypes += " or ";
                    expectedTypes += "'" + toTypedType(overload->getParamTypes()[0])->toString() + "'";
                }
                ErrorHandler::getInstance().makeError(
                    "[RCE009]: No matching overload for function '" + funcName +
                        "'. Expected " + expectedTypes + " argument, but got '" +
                        (typedArgs.empty() || !typedArgs[0] ? "unknown" : typedArgs[0]->getType()->toString()) + "'.",
                    node.getRange());
                stReturnType = makeSTType("unknown");
                if (!overloadSet->getFunctions().empty()) {
                    expectedParamTypes = overloadSet->getFunctions()[0]->getParamTypes();
                }
            }
        } else if (auto funcSym = std::dynamic_pointer_cast<FunctionSymbol>(sym)) {
            stReturnType = funcSym->getReturnType();
            expectedParamTypes = funcSym->getParamTypes();
        } else {
            ErrorHandler::getInstance().makeError(
                "[RCE010]: '" + funcName + "' is not a function.",
                node.getRange());
            stReturnType = makeSTType("unknown");
        }

        auto returnType = toTypedType(stReturnType);

        if (sym && args.size() != expectedParamTypes.size() &&
            !std::dynamic_pointer_cast<OverloadSet>(sym)) {
            ErrorHandler::getInstance().makeError(
                "[RCE011]: Function '" + funcName + "' expects " +
                    std::to_string(expectedParamTypes.size()) + " arguments, but got " +
                    std::to_string(args.size()) + ".",
                node.getRange());
        }

        for (size_t i = 0; i < typedArgs.size(); ++i) {
            auto typedArg = typedArgs[i];
            if (!typedArg)
                continue;

            if (i < expectedParamTypes.size()) {
                auto expectedTyped = toTypedType(expectedParamTypes[i]);
                auto actualType = typedArg->getType();
                if (!expectedTyped->equals(*actualType) &&
                    actualType->toString() != "unknown") {
                    ErrorHandler::getInstance().makeError(
                        "[RCE012]: Argument " + std::to_string(i + 1) +
                            " expects type '" + expectedTyped->toString() +
                            "', but got '" + actualType->toString() + "'.",
                        args[i]->getRange());
                }
            }
        }

        std::vector<std::shared_ptr<Type>> paramTypeObjs;
        for (const auto &pt : expectedParamTypes)
            paramTypeObjs.push_back(toTypedType(pt));
        auto funcType = TypeFactory::getFunction(returnType, paramTypeObjs);

        auto typedFuncName = std::make_shared<TypedIdentifierNode>(funcName, funcType);
        typedFuncName->setRange(funcNameNode->getRange());

        auto typedCall = std::make_shared<TypedFunctionCallNode>(typedFuncName, typedArgs, returnType);
        typedCall->setRange(node.getRange());
        lastNode = typedCall;
    }
} // namespace Ryntra::Compiler::Semantic
