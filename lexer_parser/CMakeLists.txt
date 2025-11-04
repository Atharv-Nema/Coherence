cmake_minimum_required(VERSION 3.16)
project(LexerParser LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(FLEX REQUIRED)
find_package(BISON REQUIRED)

set(LEXER_FILE ${CMAKE_CURRENT_SOURCE_DIR}/lexer.l)
set(PARSER_FILE ${CMAKE_CURRENT_SOURCE_DIR}/parser.y)

BISON_TARGET(Parser
    ${PARSER_FILE}
    ${CMAKE_CURRENT_BINARY_DIR}/parser.tab.cc
    DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/parser.tab.hpp
    COMPILE_FLAGS "--defines=parser.tab.hpp"
)

FLEX_TARGET(Lexer
    ${LEXER_FILE}
    ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.cc
)

ADD_FLEX_BISON_DEPENDENCY(Lexer Parser)

add_library(lexer_parser
    ${BISON_Parser_OUTPUTS}
    ${FLEX_Lexer_OUTPUTS}
)

target_include_directories(lexer_parser PUBLIC
    ${CMAKE_SOURCE_DIR}/ast
    ${CMAKE_CURRENT_BINARY_DIR}
)

message(STATUS "LexerParser configured successfully.")
