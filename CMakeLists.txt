cmake_minimum_required(VERSION 3.16)
project(Coherence LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(ast)
add_subdirectory(global_utils)
add_subdirectory(lexer_parser)
add_subdirectory(type_checker)
add_subdirectory(runtime)
add_subdirectory(codegen)
add_subdirectory(coherence)