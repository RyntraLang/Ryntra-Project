cmake_minimum_required(VERSION 3.15)
project(Rync DESCRIPTION "The compiler of The Ryntra Language")

set(antlr4-runtime_DIR "C:/vcpkg/vcpkg/vcpkg/installed/x64-windows/share/antlr4-runtime")

find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_package(antlr4-runtime REQUIRED)

# Make the imported runtime visible to sibling projects (e.g. the language
# server) so they can deploy the shared library next to their executable.
if (TARGET antlr4_shared)
    set_target_properties(antlr4_shared PROPERTIES IMPORTED_GLOBAL TRUE)
endif ()

set(GEN_SCRIPT_DIR "${CMAKE_SOURCE_DIR}/Scripts/GenAllNodesVisitor")
set(GEN_SCRIPT "${GEN_SCRIPT_DIR}/GenAllNodesVisitor.py")
set(GEN_NODES_LIST "${GEN_SCRIPT_DIR}/NodesList.txt")
set(GENERATED_HEADER "${CMAKE_BINARY_DIR}/Compiler/GeneratedHeader/AllNodesVisitor.h")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/Compiler/GeneratedHeader")

add_custom_command(
        OUTPUT ${GENERATED_HEADER}
        COMMAND ${Python3_EXECUTABLE} ${GEN_SCRIPT} ${GENERATED_HEADER}
        WORKING_DIRECTORY ${GEN_SCRIPT_DIR}
        DEPENDS ${GEN_SCRIPT} ${GEN_NODES_LIST}
        COMMENT "Generating AllNodesVisitor.h from GenAllNodesVisitor.py"
        VERBATIM
)

add_custom_target(GenerateAllNodesVisitor ALL DEPENDS ${GENERATED_HEADER})

set(ANTLR_SOURCE
        ${CMAKE_SOURCE_DIR}/ANTLR/antlr-generated/antlr/RyntraBaseListener.cpp
        ${CMAKE_SOURCE_DIR}/ANTLR/antlr-generated/antlr/RyntraBaseVisitor.cpp
        ${CMAKE_SOURCE_DIR}/ANTLR/antlr-generated/antlr/RyntraLexer.cpp
        ${CMAKE_SOURCE_DIR}/ANTLR/antlr-generated/antlr/RyntraParser.cpp
        ${CMAKE_SOURCE_DIR}/ANTLR/antlr-generated/antlr/RyntraListener.cpp
        ${CMAKE_SOURCE_DIR}/ANTLR/antlr-generated/antlr/RyntraVisitor.cpp
)

set(AST_SOURCE
        AST/ASTNodes.cpp
        AST/ASTBuilder.cpp
        ${GENERATED_HEADER}
)

set(UTILITY_SOURCE
        ${CMAKE_SOURCE_DIR}/Utility/ErrorHandler/ErrorHandler.cpp
        ${CMAKE_SOURCE_DIR}/Utility/ErrorHandler/LexParseErrorHandler.h
        ${CMAKE_SOURCE_DIR}/Utility/ErrorHandler/LexParseErrorHandler.cpp
        ${CMAKE_SOURCE_DIR}/Utility/SourceLocation/SourceRange.h
        ${CMAKE_SOURCE_DIR}/Utility/SourceLocation/SourceRangeBuilder.h
        ${CMAKE_SOURCE_DIR}/Utility/ErrorHandler/InternalCompilerError.h
        ${CMAKE_SOURCE_DIR}/Utility/ErrorHandler/RuntimeError.h
)

set(SEMANTIC_SOURCE
        Semantic/SymbolTable.cpp
        Semantic/SemanticAnalyzer.cpp
        Semantic/Sema/Assignments.cpp
        Semantic/Sema/ControlFlow.cpp
        Semantic/Sema/Declarations.cpp
        Semantic/Sema/FunctionCall.cpp
        Semantic/Sema/Literals.cpp
        Semantic/Sema/Operators.cpp
        Semantic/Sema/RefAndPtr.cpp
)

set(IR_SOURCE
        IR/IRBuilder.h
        IR/IRBuilder.cpp
        IR/IRGenerator.h
        IR/IRGenerator.cpp
        IR/Generator/ControlFlow.cpp
        IR/Generator/Expressions.cpp
        IR/Generator/Memory.cpp
        IR/Generator/Operations.cpp
        IR/Generator/ShortCircuit.cpp
        IR/Generator/Variables.cpp
        IR/Generator/Structs.cpp
        IR/Type.h
        IR/Value.h
        IR/Instruction.h
        IR/Constant.h
        IR/BasicBlock.h
        IR/Function.h
        IR/Module.h
        IR/ImmediateValue.h
)

set(VM_SOURCE
        VM/VMValue.h
        VM/Bytecode.h
        VM/BytecodeGenerator.h
        VM/BytecodeGenerator.cpp
        VM/VirtualMachine.h
        VM/VirtualMachine.cpp
        ${CMAKE_SOURCE_DIR}/Utility/ErrorHandler/InternalCompilerError.h
)

# The reusable front-end (lexer, parser, AST and semantic analysis). It is shared
# by the command line compiler and the language server, so neither has to
# recompile the front-end sources and the language server can depend on a stable
# library boundary instead of the compiler executable.
add_library(RyntraFrontend STATIC
        ${ANTLR_SOURCE}
        ${AST_SOURCE}
        ${UTILITY_SOURCE}
        ${SEMANTIC_SOURCE}
)

add_dependencies(RyntraFrontend GenerateAllNodesVisitor)

target_link_libraries(RyntraFrontend PUBLIC antlr4_shared)

# Remove this line to silence the deprecated warning of getLocation()/setLocation()
target_compile_definitions(RyntraFrontend PUBLIC RYNTRA_ENABLE_LOCATION_DEPRECATION)

target_include_directories(RyntraFrontend PUBLIC
        ${CMAKE_SOURCE_DIR}/ANTLR/antlr-generated
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/Utility
        ${CMAKE_SOURCE_DIR}/
        ${CMAKE_BINARY_DIR} #< Avoid pollute the source tree
)

add_executable(RyntraCompiler
        main.cpp
        ${IR_SOURCE}
        ${VM_SOURCE}
)

target_link_libraries(RyntraCompiler PRIVATE RyntraFrontend)

set_target_properties(RyntraCompiler PROPERTIES
        OUTPUT_NAME "rync"
)
