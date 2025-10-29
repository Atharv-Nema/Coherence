cmake_minimum_required(VERSION 3.16)
project(Coherence LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories for headers
include_directories(
    ${PROJECT_SOURCE_DIR}/ast
    ${PROJECT_SOURCE_DIR}/type_checker
)

# Explicitly list your source files
add_library(type_checker
    type_checker/type_checker.cpp
    type_checker/utils.cpp
)
