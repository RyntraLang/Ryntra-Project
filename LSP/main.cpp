#include "Analysis/LanguageProviders.h"
#include "Compiler/CompilerLanguageProvider.h"
#include "JsonRPCTransport/Transport.h"
#include "LSPServer.h"

#include <iostream>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

int main() {
#ifdef _WIN32
    // The LSP framing counts bytes, so stdio must not translate line endings.
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    Ryntra::LSP::JsonRpcTransport transport(std::cin, std::cout);

    Ryntra::LSP::CompilerLanguageProvider languageProvider;
    Ryntra::LSP::LanguageProviders providers;
    providers.diagnostics = &languageProvider;
    providers.hover = &languageProvider;
    providers.definition = &languageProvider;

    Ryntra::LSP::LSPServer server(transport, providers);

    return server.run();
}
