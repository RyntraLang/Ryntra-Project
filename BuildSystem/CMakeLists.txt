cmake_minimum_required(VERSION 3.15)
project(RyntraBuildSystem DESCRIPTION "The build system of the Ryntra Programming Language")

find_package(PkgConfig REQUIRED)
pkg_check_modules(tomlplusplus REQUIRED IMPORTED_TARGET tomlplusplus)

add_executable(RyntraBuildSystem
        main.cpp
)

target_link_libraries(RyntraBuildSystem PRIVATE RyntraBackend PkgConfig::tomlplusplus)

add_custom_command(TARGET RyntraBuildSystem POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:antlr4_shared>
                $<TARGET_FILE_DIR:RyntraBuildSystem>
)

set_target_properties(RyntraBuildSystem PROPERTIES
        OUTPUT_NAME "riko"
)