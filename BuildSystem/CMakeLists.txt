cmake_minimum_required(VERSION 3.15)
project(RyntraBuildSystem DESCRIPTION "The build system of the Ryntra Programming Language")

# Riko is intentionally decoupled from the compiler libraries: it only reads the
# manifest and delegates compilation/execution to the `rync` executable, so it no
# longer links the front-end, IR or VM.
find_package(PkgConfig REQUIRED)
pkg_check_modules(tomlplusplus REQUIRED IMPORTED_TARGET tomlplusplus)

add_executable(RyntraBuildSystem
        main.cpp
        Init.cpp
        Init.h
        Manifest.cpp
        Manifest.h
        Process.cpp
        Process.h
)

target_link_libraries(RyntraBuildSystem PRIVATE PkgConfig::tomlplusplus)

# ArgParsing is a header-only utility; Riko no longer links the compiler libraries
# (and therefore RyntraUtility), so it needs the Utility headers explicitly.
target_include_directories(RyntraBuildSystem PRIVATE ${CMAKE_SOURCE_DIR}/Utility)

# toml++ is consumed through pkg-config, so vcpkg's app-local deployment does not
# pick up its runtime DLL. Copy it next to riko so the executable can be launched
# directly.
get_filename_component(_tomlplusplus_bin_dir "${tomlplusplus_LIBDIR}/../bin" ABSOLUTE)
file(GLOB _tomlplusplus_runtime_dlls "${_tomlplusplus_bin_dir}/tomlplusplus*.dll")
foreach (_tomlplusplus_dll ${_tomlplusplus_runtime_dlls})
    add_custom_command(TARGET RyntraBuildSystem POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_tomlplusplus_dll}"
                    $<TARGET_FILE_DIR:RyntraBuildSystem>
    )
endforeach ()

set_target_properties(RyntraBuildSystem PROPERTIES
        OUTPUT_NAME "riko"
)
