// ========== ASTVisitor.h =================================== *- C++ -* ========== //
// Copyright (c) 2026 Remimwen Studio. All rights reserved.
// Part of the Ryntra Project. Licensed under Apache-2.0 License.
// ================================================================================ //

#pragma once

namespace Ryntra::Compiler {
    /**
     * @brief The base class of the AST Visitor. Allow all Visitors to be
     * held or passed uniformly via an @c IVisitor* pointer, thereby implementing type erasure.
     */
    class IVisitor {
    public:
        virtual ~IVisitor() = default;
    };

    /**
     * @brief A templated access interface. It declares a pure virtual function
     * <code>visit(T)</code>, which means that any class inheriting from @c Visitor<NodeType>
     * must implement the access logic for that node type.
     * @tparam T The type that match the AST Node. (e.g. IfNode)
     */
    template <typename T>
    class Visitor {
    public:
        virtual ~Visitor() = default;
        virtual void visit(T &node) = 0;
    };
} // namespace Ryntra::Compiler
