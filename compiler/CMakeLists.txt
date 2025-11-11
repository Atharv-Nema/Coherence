cmake_minimum_required(VERSION 3.16)
project(CoherenceCompiler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(coherence
    main.cpp
)

# Link in lexer + type checker logic
target_link_libraries(coherence PRIVATE
    lexer_parser
    type_checker
)

# Include headers from these modules
target_include_directories(coherence PRIVATE
    ${CMAKE_SOURCE_DIR}/ast
    ${CMAKE_SOURCE_DIR}/type_checker
    ${CMAKE_SOURCE_DIR}/lexer_parser
)

message(STATUS "Coherence compiler executable configured successfully.")
