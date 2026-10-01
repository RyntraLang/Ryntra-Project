cmake_minimum_required(VERSION 3.15)
project(RyntraProject)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/vcpkg/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "vcpkg toolchain")

set(ALL_PROJECTS
        Compiler
        LSP
        CodeEditor
)

set(ENABLE_PROJECTS "all" CACHE STRING "List of projects to build")

if (ENABLE_PROJECTS STREQUAL "all")
    set(ENABLED_PROJECT_LIST ${ALL_PROJECTS})
else ()
    set(ENABLED_PROJECT_LIST ${ENABLE_PROJECTS})
endif ()

foreach (proj ${ENABLED_PROJECT_LIST})
    if (NOT proj IN_LIST ALL_PROJECTS)
        message(FATAL_ERROR "Unknown project: ${proj}. Available: ${ALL_PROJECTS}")
    endif ()
endforeach ()

message(STATUS "Enabled projects: ${ENABLED_PROJECT_LIST}")

# This should be uncommented when the project is ready for deploying
# But for now, it will left commented.

#foreach (proj ${ALL_PROJECTS})
#    if (proj IN_LIST ENABLED_PROJECT_LIST)
#        add_subdirectory(${proj})
#    else ()
#        message(STATUS "Skipping: ${proj}")
#    endif ()
#endforeach ()

add_subdirectory(Compiler)
add_subdirectory(LSP)