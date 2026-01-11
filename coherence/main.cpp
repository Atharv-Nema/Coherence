#include <iostream>
#include <string>
#include <filesystem>
#include <cstdio>
#include <optional>
#include <cstdlib>
#include <format>
#include "top_level.hpp"
#include "ast_validator.hpp"
#include "codegen.hpp"

#include "parser.tab.hpp"
#include "lex.yy.h"

#include <boost/program_options.hpp>
namespace po = boost::program_options;

extern Program* program_root;

int main(int argc, char* argv[]) {
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help", "produce help message")
        ("input-file", po::value<std::string>()->required(), "coherence program to compile")
        ("only-typecheck", po::value<bool>(), "whether to only typecheck the program")
        ("output-dir", po::value<std::string>(), "directory where the generated files will be stored");
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    if(vm.count("help")) {
        std::cout << desc << std::endl;
        return 0;
    }

    po::notify(vm);
    
    bool only_typecheck = false;
    if(vm.count("only-typecheck")) {
        only_typecheck = vm["only-typecheck"].as<bool>();
    }
    std::filesystem::path input_file(vm["input-file"].as<std::string>());

    if (!std::filesystem::exists(input_file)) {
        std::cerr << "Error: Input file not found: " << input_file << "\n";
        return 1;
    }

    FILE* input = fopen(input_file.c_str(), "r");
    if (!input) {
        std::cerr << "Error: Cannot open input file " << input_file << "\n";
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

    if (yyparse(scanner) != 0) {
        std::cerr << "Parse failed.\n";
        yylex_destroy(scanner);
        fclose(input);
        return 1;
    }

    fclose(input);
    yylex_destroy(scanner);

    if (!program_root) {
        std::cerr << "Error: No program produced by parser.\n";
        return 1;
    }

    bool ok = validate_program(program_root);

    if (!ok) {
        std::cerr << "Ast validation failed.\n";
        delete program_root;
        return 1;
    }


    if (only_typecheck) {
        return 0;
    }
    
    if(!vm.count("output-dir")) {
        std::cerr << "Error: Output directory not provided" << std::endl;
        return 1;
    }
    std::filesystem::path output_dir(vm["output-dir"].as<std::string>());
    if(!std::filesystem::exists(output_dir)) {
        if(!std::filesystem::create_directories(output_dir)) {
            std::cerr << "Error: could not create output directory" << std::endl;
        }
    }

    // 3. LLVM code generation
    std::filesystem::path out_ll_path = output_dir / "out.ll";
    ast_codegen(program_root, out_ll_path.string());
    std::cout << "Compilation successful\n";
    delete program_root;

    // 4. Compiling to an executable
    std::filesystem::path out_s_path = output_dir / "out.s";
    std::string obj_compile_cmd = std::format("llc {} -o {}", out_ll_path.string(), out_s_path.string());
    if (std::system(obj_compile_cmd.c_str()) != 0) {
        std::cerr << "Error: llc failed\n";
        return 1;
    }

    // 5) Link assembly + runtime -> executable
    // NOTE: adjust the path to libruntime.a if needed
    std::filesystem::path out_path = output_dir / "out";
    std::string link_cmd = std::format(
        "clang++ {} " COH_RUNTIME_LIB_PATH " -lboost_context -pthread -o {}", 
        out_s_path.string(), out_path.string());
    if (std::system(link_cmd.c_str()) != 0) {
        std::cerr << "Error: link failed\n";
        return 1;
    }

    std::cout << "Built executable: ./out\n";
    return 0;
}
