#include "parser.tab.hpp"
#include "lex.yy.h"

extern Program* program_root;

Program* parse_file(FILE* input) {
    yyscan_t scanner;
    if (yylex_init(&scanner)) {
        std::cerr << "Error: Could not initialize scanner.\n";
        return nullptr;
    }
    yyset_in(input, scanner);

    if (yyparse(scanner) != 0) {
        std::cerr << "Parse failed.\n";
        yylex_destroy(scanner);
        return nullptr;
    }
    yylex_destroy(scanner);
    return program_root;
}