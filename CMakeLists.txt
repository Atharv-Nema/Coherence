cmake_minimum_required(VERSION 3.16)
project(Coherence LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    ${PROJECT_SOURCE_DIR}/ast
)

add_subdirectory(lexer_parser)
add_subdirectory(type_checker)
