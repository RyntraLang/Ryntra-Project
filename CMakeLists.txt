cmake_minimum_required(VERSION 3.15)
project(RyntraProject)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/vcpkg/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "vcpkg toolchain")

add_subdirectory(Compiler)
add_subdirectory(CodeEditor)
