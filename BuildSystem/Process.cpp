// ========== Process.cpp ============================================= *- C++ -* //
// Copyright (c) 2026 Remimwen Studio. All rights reserved.
// Part of the Ryntra Project. Licensed under Apache-2.0 License.
// ================================================================================ //

#include "Process.h"

#include <cstdint>
#include <cstdlib>

#ifdef _WIN32
#include <process.h>
#endif

namespace Ryntra::BuildSystem {
    int runProcess(const std::string &program, const std::vector<std::string> &args) {
#ifdef _WIN32
        // _spawnvp runs the program directly (no shell), so paths containing spaces
        // need no quoting, and waits for it to finish. It also searches PATH.
        std::vector<const char *> argv;
        argv.reserve(args.size() + 2);
        argv.push_back(program.c_str());
        for (const auto &arg : args) {
            argv.push_back(arg.c_str());
        }
        argv.push_back(nullptr);

        const intptr_t status = _spawnvp(_P_WAIT, program.c_str(), argv.data());
        if (status == -1) {
            return -1;
        }
        return static_cast<int>(status);
#else
        std::string command = "\"" + program + "\"";
        for (const auto &arg : args) {
            command += " \"" + arg + "\"";
        }
        return std::system(command.c_str());
#endif
    }
} // namespace Ryntra::BuildSystem
