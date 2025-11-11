#include <iostream>
#include <string>
#include <filesystem>
#include <cstdio>
#include "../ast/top_level.hpp"
#include "../type_checker/type_checker.hpp"

// Declared by the generated parser (parser.tab.hh / parser.tab.hpp)
extern int yyparse();
extern FILE* yyin;

// The parser fills this in
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

    // --- 1. Open file for parsing ---
    yyin = fopen(filename.c_str(), "r");
    if (!yyin) {
        std::cerr << "Error: Cannot open file " << filename << "\n";
        return 1;
    }

    std::cout << "Parsing " << filename << "...\n";
    if (yyparse() != 0) {
        std::cerr << "Parse failed.\n";
        fclose(yyin);
        return 1;
    }
    fclose(yyin);

    if (!program_root) {
        std::cerr << "Parse produced no program.\n";
        return 1;
    }

    std::cout << "Parsing successful.\n";

    // --- 2. Type checking ---
    std::cout << "Running type checker...\n";

    TypeEnv env;
    bool ok = type_check_program(program_root);

    if (!ok) {
        std::cerr << "Type checking failed.\n";
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
