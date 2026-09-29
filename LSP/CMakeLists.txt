cmake_minimum_required(VERSION 3.15)
project(RyntraLSP DESCRIPTION "The LSP of The Ryntra Programming Language")

set(nlohmann_json_DIR "C:/vcpkg/vcpkg/vcpkg/installed/x64-windows/share/nlohmann_json")

add_executable(RyntraLSP main.cpp
        LSP/LSPServer.h
        LSP/LSPServer.cpp
        LSP/DocumentManager.h
        LSP/DocumentManager.cpp
        LSP/JsonRPC/JsonRPC.cpp
        LSP/JsonRPC/JsonRPC.h
        LSP/JsonRPCTransport/Transport.h
        LSP/JsonRPCTransport/Transport.cpp
        LSP/Protocol/Protocol.h
        LSP/Protocol/LSPTypes.h
        LSP/Protocol/Initialize.h
        LSP/Protocol/Initialize.cpp
        LSP/Protocol/TextDocument.h
        LSP/Protocol/TextDocument.cpp
)

set_target_properties(RyntraLSP PROPERTIES
        OUTPUT_NAME "ryntls"
)

find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(RyntraLSP PRIVATE nlohmann_json::nlohmann_json)
target_include_directories(RyntraLSP PRIVATE LSP/)