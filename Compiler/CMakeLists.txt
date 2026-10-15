cmake_minimum_required(VERSION 3.15)
project(Rync DESCRIPTION "The compiler of The Ryntra Language")

set(antlr4-runtime_DIR "C:/vcpkg/vcpkg/vcpkg/installed/x64-windows/share/antlr4-runtime")

find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_package(antlr4-runtime REQUIRED)

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

set(SEMANTIC_SOURCE
        Semantic/SymbolTable.cpp
        Semantic/SemanticAnalyzer.cpp
        Semantic/Sema/Assignments.cpp
        Semantic/Sema/ControlFlow.cpp
        Semantic/Sema/Declarations.cpp
        Semantic/Sema/FunctionCall.cpp
        Semantic/Sema/Literals.cpp
        Semantic/Sema/Operators.cpp
        Semantic/Sema/Packages.cpp
        Semantic/Sema/RefAndPtr.cpp
)

# The import-graph resolver lives with the front-end: it only needs the parser and
# the semantic program model, and lets `rync` compile a multi-file program alone.
set(DRIVER_FRONTEND_SOURCE
        Driver/ImportGraph.cpp
        Driver/ImportGraph.h
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
)

# The reusable front-end (lexer, parser, AST and semantic analysis). It is shared
# by the command line compiler and the language server, so neither has to
# recompile the front-end sources and the language server can depend on a stable
# library boundary instead of the compiler executable.
add_library(RyntraFrontend STATIC
        ${ANTLR_SOURCE}
        ${AST_SOURCE}
        ${SEMANTIC_SOURCE}
        ${DRIVER_FRONTEND_SOURCE}
)

add_dependencies(RyntraFrontend GenerateAllNodesVisitor)

# Utility provides the shared ErrorHandler/SourceLocation code and, transitively,
# the ANTLR runtime used by the generated front-end sources.
target_link_libraries(RyntraFrontend PUBLIC RyntraUtility antlr4_shared)

target_include_directories(RyntraFrontend PUBLIC
        ${CMAKE_SOURCE_DIR}/ANTLR/antlr-generated
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/
        ${CMAKE_BINARY_DIR} #< Avoid pollute the source tree
)

add_library(RyntraBackend STATIC
        ${IR_SOURCE}
        ${VM_SOURCE}
        Driver/CompilePipeline.cpp
        Driver/CompilePipeline.h
)

target_link_libraries(RyntraBackend PUBLIC RyntraFrontend RyntraUtility)

add_executable(RyntraCompiler
        main.cpp
)

target_link_libraries(RyntraCompiler PRIVATE RyntraBackend)

# rync links the ANTLR shared runtime; place it next to the executable so rync can
# be launched directly (e.g. as a subprocess from riko).
add_custom_command(TARGET RyntraCompiler POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:antlr4_shared>
                $<TARGET_FILE_DIR:RyntraCompiler>
)

set_target_properties(RyntraCompiler PROPERTIES
        OUTPUT_NAME "rync"
)
