#pragma once

#include <string_view>

namespace Ryntra::LSP::Protocol {
    inline constexpr std::string_view kInitialize = "initialize";
    inline constexpr std::string_view kInitialized = "initialized";
    inline constexpr std::string_view kShutdown = "shutdown";
    inline constexpr std::string_view kExit = "exit";

    inline constexpr std::string_view kTextDocumentDidOpen = "textDocument/didOpen";
    inline constexpr std::string_view kTextDocumentDidChange = "textDocument/didChange";
    inline constexpr std::string_view kTextDocumentDidClose = "textDocument/didClose";
    inline constexpr std::string_view kTextDocumentPublishDiagnostics = "textDocument/publishDiagnostics";
    inline constexpr std::string_view kTextDocumentHover = "textDocument/hover";
    inline constexpr std::string_view kTextDocumentDefinition = "textDocument/definition";

    inline constexpr std::string_view kServerName = "Ryntra Language Server";
    inline constexpr std::string_view kServerVersion = "0.1.0";
} // namespace Ryntra::LSP::Protocol
