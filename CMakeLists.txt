cmake_minimum_required(VERSION 3.16)
project(minicuda_ptx_compiler CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -O2)
endif()

# Source files
set(SOURCES
    src/lexer.cpp
    src/parser.cpp
    src/irgen.cpp
    src/ptx_emitter.cpp
    src/main.cpp
)

add_executable(ptxc ${SOURCES})
target_include_directories(ptxc PRIVATE include)

# Output directory for .ptx files
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/output)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
