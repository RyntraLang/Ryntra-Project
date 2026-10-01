// ========== RuntimeError.h ========================================== *- C++ -* //
// Copyright (c) 2026 Remimwen Studio (Ryan "NvKopres" Almond).
// Licensed under Apache-2.0 License. See LICENSE for more info.
// ============================================================================== //

#pragma once

#include "SourceLocation/SourceRange.h"
#include <exception>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Ryntra::Compiler {
    /// \brief The category of a runtime error. Used for diagnostics and, when
    /// needed, programmatic recovery.
    enum class RuntimeErrorKind : int {
        UninitializedValue,
        StackUnderflow,
        TypeMismatch,
        InvalidOperation,
        InvalidReference,
        InvalidPointer,
        InvalidIndex,
        InvalidFunction,
        InvalidBuiltin,
        InvalidArgument,
        HeapError,
        Internal
    };

    /// \brief Human-readable name of a \c RuntimeErrorKind .
    constexpr std::string_view runtimeErrorKindName(const RuntimeErrorKind kind) {
        switch (kind) {
        case RuntimeErrorKind::UninitializedValue: return "uninitialized value";
        case RuntimeErrorKind::StackUnderflow:      return "stack underflow";
        case RuntimeErrorKind::TypeMismatch:        return "type mismatch";
        case RuntimeErrorKind::InvalidOperation:    return "invalid operation";
        case RuntimeErrorKind::InvalidReference:    return "invalid reference";
        case RuntimeErrorKind::InvalidPointer:      return "invalid pointer";
        case RuntimeErrorKind::InvalidIndex:        return "invalid index";
        case RuntimeErrorKind::InvalidFunction:     return "invalid function";
        case RuntimeErrorKind::InvalidBuiltin:      return "invalid builtin";
        case RuntimeErrorKind::InvalidArgument:     return "invalid argument";
        case RuntimeErrorKind::HeapError:           return "heap error";
        case RuntimeErrorKind::Internal:            return "internal error";
        }
        return "runtime error";
    }

    /// \brief A plain description of a runtime error. Kept as a value object so it
    /// can be passed around (e.g. into the \c ErrorHandler ) without throwing.
    struct RuntimeError {
        RuntimeErrorKind kind;
        std::string_view description;
        SourceRange range;
    };

    /// \brief One caller in a runtime traceback. \c callerName is the function
    /// that performed the call and \c callSite is the range of the call
    /// expression itself.
    struct RuntimeStackFrame {
        std::string callerName;
        SourceRange callSite;
    };

    /// \brief The exception thrown by \c trap() . It owns the description and the
    /// traceback so the diagnostic survives stack unwinding out of the VM.
    class RuntimeErrorException : public std::exception {
    public:
        RuntimeErrorException(const RuntimeErrorKind kind, std::string description, SourceRange range,
                              std::vector<RuntimeStackFrame> trace = {})
            : kind_(kind), description_(std::move(description)), range_(std::move(range)),
              trace_(std::move(trace)) {
            message_ = "runtime error: ";
            message_ += runtimeErrorKindName(kind_);
            message_ += ": ";
            message_ += description_;
        }

        /// \brief Rebuild a \c RuntimeError view over this exception's storage.
        [[nodiscard]] RuntimeError getError() const {
            return RuntimeError{kind_, description_, range_};
        }

        [[nodiscard]] RuntimeErrorKind getKind() const noexcept { return kind_; }
        [[nodiscard]] const std::string &getDescription() const noexcept { return description_; }
        [[nodiscard]] const SourceRange &getRange() const noexcept { return range_; }

        /// \brief The call chain that led to the error, innermost call first.
        [[nodiscard]] const std::vector<RuntimeStackFrame> &getTrace() const noexcept { return trace_; }

        [[nodiscard]] const char *what() const noexcept override { return message_.c_str(); }

    private:
        RuntimeErrorKind kind_;
        std::string description_;
        SourceRange range_;
        std::vector<RuntimeStackFrame> trace_;
        std::string message_;
    };

    /// \brief Raise a runtime error. Never returns: it always throws
    /// \c RuntimeErrorException . This is the guard that stops the VM before it
    /// can continue in an inconsistent state.
    /// \param kind The runtime error category
    /// \param description The diagnostic description
    /// \param range The source range the error refers to, if known
    /// \param trace The call chain that produced the error, innermost call first
    [[noreturn]] inline void trap(const RuntimeErrorKind kind, std::string description,
                                  const SourceRange &range = {},
                                  std::vector<RuntimeStackFrame> trace = {}) {
        throw RuntimeErrorException(kind, std::move(description), range, std::move(trace));
    }
} // namespace Ryntra::Compiler
