cmake_minimum_required(VERSION 3.16)
project(TypeChecker LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define the type_checker library
add_library(type_checker
    type_checker.cpp
    utils.cpp
)

message(STATUS "TypeChecker configured successfully.")
