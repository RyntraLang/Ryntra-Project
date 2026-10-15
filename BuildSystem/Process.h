// ========== Process.h =============================================== *- C++ -* //
// Copyright (c) 2026 Remimwen Studio. All rights reserved.
// Part of the Ryntra Project. Licensed under Apache-2.0 License.
//
// Minimal child-process helper used to invoke the compiler (`rync`).
// ================================================================================ //

#pragma once

#include <string>
#include <vector>

namespace Ryntra::BuildSystem {
    /**
     * @brief Run @p program with @p args, inheriting this process' standard streams
     * so the child's output is relayed to the user.
     * @return The child's exit code, or -1 if it could not be launched.
     */
    int runProcess(const std::string &program, const std::vector<std::string> &args);
} // namespace Ryntra::BuildSystem
