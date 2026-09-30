cmake_minimum_required(VERSION 3.15)
project(RyntraLSP DESCRIPTION "The LSP of The Ryntra Programming Language")

set(nlohmann_json_DIR "C:/vcpkg/vcpkg/vcpkg/installed/x64-windows/share/nlohmann_json")

add_executable(RyntraLSP main.cpp
        LSP/LSPServer.h
        LSP/LSPServer.cpp
        LSP/DocumentManager.h
        LSP/DocumentManager.cpp
        LSP/Analysis/Completion.h
        LSP/Analysis/Completion.cpp
        LSP/Analysis/CompletionProvider.h
        LSP/Analysis/DefinitionProvider.h
        LSP/Analysis/DocumentSymbol.h
        LSP/Analysis/DocumentSymbol.cpp
        LSP/Analysis/DocumentSymbolProvider.h
        LSP/Analysis/Hover.h
        LSP/Analysis/Hover.cpp
        LSP/Analysis/HoverProvider.h
        LSP/Analysis/LanguageProviders.h
        LSP/Analysis/Location.h
        LSP/Analysis/Location.cpp
        LSP/Analysis/SemanticTokens.h
        LSP/Analysis/SemanticTokens.cpp
        LSP/Analysis/SemanticTokensProvider.h
        LSP/Compiler/CompilerLanguageProvider.h
        LSP/Compiler/CompilerLanguageProvider.cpp
        LSP/JsonRPC/JsonRPC.cpp
        LSP/JsonRPC/JsonRPC.h
        LSP/JsonRPCTransport/Transport.h
        LSP/JsonRPCTransport/Transport.cpp
        LSP/Protocol/Protocol.h
        LSP/Protocol/LSPTypes.h
        LSP/Protocol/LSPTypes.cpp
        LSP/Protocol/Initialize.h
        LSP/Protocol/Initialize.cpp
        LSP/Protocol/TextDocument.h
        LSP/Protocol/TextDocument.cpp
        LSP/Diagnostics/Diagnostic.h
        LSP/Diagnostics/Diagnostic.cpp
        LSP/Diagnostics/DiagnosticsProvider.h
        LSP/Text/TextOffset.h
        LSP/Text/TextOffset.cpp
)

set_target_properties(RyntraLSP PROPERTIES
        OUTPUT_NAME "ryntra-lsp"
)

find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(RyntraLSP PRIVATE nlohmann_json::nlohmann_json RyntraFrontend)
target_include_directories(RyntraLSP PRIVATE LSP/)

# The compiler front-end is linked against the ANTLR shared runtime; place it
# next to the executable so the language server can be launched directly.
add_custom_command(TARGET RyntraLSP POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:antlr4_shared>
                $<TARGET_FILE_DIR:RyntraLSP>
)
