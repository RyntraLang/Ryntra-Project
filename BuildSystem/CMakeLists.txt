cmake_minimum_required(VERSION 3.15)
project(RyntraBuildSystem DESCRIPTION "The build system of the Ryntra Programming Language")

add_executable(RyntraBuildSystem
        main.cpp
)

target_link_libraries(RyntraBuildSystem PRIVATE RyntraUtility)

set_target_properties(RyntraBuildSystem PROPERTIES
        OUTPUT_NAME "riko"
)