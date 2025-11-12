#include <iostream>
#include <string>
#include <filesystem>
#include <cstdio>
#include "../ast/top_level.hpp"
#include "../type_checker/type_checker.hpp"
#include "../lexer_parser/parser.tab.hpp"
#include "../lexer_parser/lex.yy.h"


extern Program* program_root;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: coherence [--only-typecheck] <source_file>\n";
        return 1;
    }

    bool only_typecheck = false;
    std::string filename;

    if(argc == 3) {
        only_typecheck = true;
    }
    filename = argv[argc - 1];

    if (filename.empty()) {
        std::cerr << "Error: No input file specified.\n";
        return 1;
    }

    if (!std::filesystem::exists(filename)) {
        std::cerr << "Error: File not found: " << filename << "\n";
        return 1;
    }

    FILE* input = fopen(filename.c_str(), "r");
    if (!input) {
        std::cerr << "Error: Cannot open file " << filename << "\n";
        return 1;
    }

    // --- Initialize scanner ---
    yyscan_t scanner;
    if (yylex_init(&scanner)) {
        std::cerr << "Error: Could not initialize scanner.\n";
        fclose(input);
        return 1;
    }
    yyset_in(input, scanner);

    std::cout << "Parsing " << filename << "...\n";
    if (yyparse(scanner) != 0) {
        std::cerr << "Parse failed.\n";
        yylex_destroy(scanner);
        fclose(input);
        return 1;
    }

    std::cout << "Parsing successful.\n";
    fclose(input);
    yylex_destroy(scanner);

    if (!program_root) {
        std::cerr << "Error: No program produced by parser.\n";
        return 1;
    }

    std::cout << "Running type checker...\n";
    TypeEnv env;
    bool ok = type_check_program(program_root);

    if (!ok) {
        std::cerr << "Type checking failed.\n";
        delete program_root;
        return 1;
    }

    std::cout << "Type checking succeeded.\n";

    if (only_typecheck) {
        return 0;
    }

    // --- 3. Future steps ---
    std::cout << "Compiler not ready yet\n";
    delete program_root;
    return 0;
}
