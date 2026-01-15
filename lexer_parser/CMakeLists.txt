find_package(FLEX REQUIRED)
find_package(BISON REQUIRED)

set(LEXER_FILE ${CMAKE_CURRENT_SOURCE_DIR}/lexer.l)
set(PARSER_FILE ${CMAKE_CURRENT_SOURCE_DIR}/parser.y)

set(BISON_WARNING_FLAGS
    "-Wall -Wcounterexamples -Wconflicts-sr -Wconflicts-rr"
)

set(BISON_FULL_FLAGS
    "${BISON_WARNING_FLAGS} --defines=parser.tab.hpp"
)

BISON_TARGET(Parser
    ${PARSER_FILE}
    ${CMAKE_CURRENT_BINARY_DIR}/parser.tab.cc
    DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/parser.tab.hpp
    COMPILE_FLAGS "${BISON_FULL_FLAGS}"
)

FLEX_TARGET(Lexer
    ${LEXER_FILE}
    ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.cc
    DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.h
)

# Ensure dependency
ADD_FLEX_BISON_DEPENDENCY(Lexer Parser)

add_library(lexer_parser
    ${BISON_Parser_OUTPUTS}
    ${FLEX_Lexer_OUTPUTS}
    parse_file.cpp
)

# CR: Probably make [ast] a library
target_include_directories(lexer_parser PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR} 
)

target_link_libraries(lexer_parser PUBLIC ast)

message(STATUS "LexerParser configured successfully.")
