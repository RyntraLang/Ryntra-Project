cmake_minimum_required(VERSION 3.15)
project(RyntraUtility DESCRIPTION "Shared utility code of the Ryntra Programming Language")

set(antlr4-runtime_DIR "C:/vcpkg/vcpkg/vcpkg/installed/x64-windows/share/antlr4-runtime")
find_package(antlr4-runtime REQUIRED)

if (TARGET antlr4_shared)
    set_target_properties(antlr4_shared PROPERTIES IMPORTED_GLOBAL TRUE)
endif ()

add_library(RyntraUtility STATIC
        ArgParsing/ArgParsing.h
        ColoredTextManager/ColoredTextManager.h
        ErrorHandler/ErrorHandler.cpp
        ErrorHandler/ErrorHandler.h
        ErrorHandler/InternalCompilerError.h
        ErrorHandler/LexParseErrorHandler.cpp
        ErrorHandler/LexParseErrorHandler.h
        ErrorHandler/RuntimeError.h
        SourceLocation/SourceLocation.h
        SourceLocation/SourceRange.h
        SourceLocation/SourceRangeBuilder.h
)

target_include_directories(RyntraUtility PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(RyntraUtility PUBLIC antlr4_shared)

# Remove this line to silence the deprecated warning of getLocation()/setLocation()
target_compile_definitions(RyntraUtility PUBLIC RYNTRA_ENABLE_LOCATION_DEPRECATION)
