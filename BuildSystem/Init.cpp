// ========== Init.cpp ================================================ *- C++ -* //
// Copyright (c) 2026 Remimwen Studio. All rights reserved.
// Part of the Ryntra Project. Licensed under Apache-2.0 License.
// ================================================================================ //

#include "Init.h"

#include <cctype>
#include <fstream>
#include <system_error>

namespace Ryntra::BuildSystem {
    namespace {
        /// \brief Turn an arbitrary directory name into a TOML-bare-key friendly
        /// target name (letters, digits and underscores).
        std::string sanitizeIdentifier(const std::string &name) {
            std::string result;
            result.reserve(name.size());
            for (const char c : name) {
                result += (std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_') ? c : '_';
            }
            if (result.empty() ||
                result.find_first_not_of("0123456789") == std::string::npos) {
                result = "App" + result;
            }
            return result;
        }

        std::string escapeTomlString(const std::string &value) {
            std::string result;
            result.reserve(value.size());
            for (const char c : value) {
                if (c == '\\' || c == '"') {
                    result += '\\';
                }
                result += c;
            }
            return result;
        }

        bool writeFile(const std::filesystem::path &file, const std::string &content) {
            std::ofstream out(file, std::ios::binary | std::ios::trunc);
            if (!out.is_open()) {
                return false;
            }
            out << content;
            return out.good();
        }
    } // namespace

    InitResult initializeProject(const std::filesystem::path &path) {
        InitResult result;

        std::error_code ec;
        auto resolved = std::filesystem::absolute(path, ec);
        if (ec) {
            result.message = "Invalid path '" + path.string() + "'.";
            return result;
        }
        resolved = resolved.lexically_normal();
        result.projectDir = resolved;

        std::string folderName = resolved.filename().string();
        if (folderName.empty() || folderName == "." || folderName == "..") {
            folderName = "RyntraProject";
        }
        result.projectName = folderName;
        result.targetName = sanitizeIdentifier(folderName);

        const auto manifestFile = resolved / "Riko.toml";
        const auto sourceFile = resolved / "src" / "main.rynt";

        if (std::filesystem::exists(manifestFile)) {
            result.message = "Refusing to overwrite existing '" + manifestFile.string() + "'.";
            return result;
        }
        if (std::filesystem::exists(sourceFile)) {
            result.message = "Refusing to overwrite existing '" + sourceFile.string() + "'.";
            return result;
        }

        std::filesystem::create_directories(resolved / "src", ec);
        if (ec) {
            result.message = "Cannot create directory '" + (resolved / "src").string() + "': " + ec.message();
            return result;
        }

        const std::string manifestContent =
            "[Project]\n"
            "name = \"" + escapeTomlString(result.projectName) + "\"\n"
            "version = \"0.1.0\"\n"
            "\n"
            "[Targets." + result.targetName + "]\n"
            "entry = \"src/main.rynt\"\n"
            "package_root = \"src\"\n"
            "\n"
            "[CompileFlags]\n"
            "strict = false\n";

        // A file at src/main.rynt has the derived package name "main".
        const std::string sourceContent =
            "package main;\n"
            "\n"
            "public void main() {\n"
            "    __builtin_print(\"Hello, Ryntra!\\n\");\n"
            "}\n";

        if (!writeFile(manifestFile, manifestContent)) {
            result.message = "Failed to write '" + manifestFile.string() + "'.";
            return result;
        }
        if (!writeFile(sourceFile, sourceContent)) {
            result.message = "Failed to write '" + sourceFile.string() + "'.";
            return result;
        }

        result.ok = true;
        result.message = "Initialized Ryntra project '" + result.projectName + "' in " + resolved.string() + ".";
        return result;
    }
} // namespace Ryntra::BuildSystem
