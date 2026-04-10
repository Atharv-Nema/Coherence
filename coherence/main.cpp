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
#include "parse_file.hpp"

#include <boost/program_options.hpp>
namespace po = boost::program_options;

extern Program* program_root;

int main(int argc, char* argv[]) {
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help", "produce help message")
        ("input-file", po::value<std::string>()->required(), "coherence program to compile")
        ("only-typecheck", po::value<bool>(), "whether to only typecheck the program")
        ("optimize", po::value<bool>(), "whether to optimize the program") // Added here
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

    bool optimize = false;
    if(vm.count("optimize")) {
        optimize = vm["optimize"].as<bool>();
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

    Program* program_root = parse_file(input);
    fclose(input);
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
        delete program_root;
        return 0;
    }
    
    if(!vm.count("output-dir")) {
        std::cerr << "Error: Output directory not provided" << std::endl;
        delete program_root;
        return 1;
    }
    std::filesystem::path output_dir(vm["output-dir"].as<std::string>());
    if(!std::filesystem::exists(output_dir)) {
        if(!std::filesystem::create_directories(output_dir)) {
            std::cerr << "Error: could not create output directory" << std::endl;
        }
    }

    // 3. LLVM code generation
    std::filesystem::path out_raw_ll_path = output_dir / "out_raw.ll";
    ast_codegen(program_root, out_raw_ll_path.string());
    std::cout << "Compilation successful\n";
    delete program_root;

    std::string final_ll_path = out_raw_ll_path.string();
    std::string llc_opt_flag = "-O0";

    // Conditionally run the optimizer
    if (optimize) {
        std::filesystem::path out_opt_ll_path = output_dir / "out_opt.ll";
        std::string opt_cmd = std::format("opt -O3 {} -S -o {}", out_raw_ll_path.string(), out_opt_ll_path.string());
        
        std::cout << "Running LLVM optimizer\n";
        if(std::system(opt_cmd.c_str()) != 0) {
            std::cerr << "Error: optimization failed\n";
            return 1;
        }
        final_ll_path = out_opt_ll_path.string();
        llc_opt_flag = "-O3";
    }

    // 4. Compiling to assembly
    std::filesystem::path out_s_path = output_dir / "out.s";
    std::string obj_compile_cmd = std::format("llc {} {} -o {}", llc_opt_flag, final_ll_path, out_s_path.string());
    if (std::system(obj_compile_cmd.c_str()) != 0) {
        std::cerr << "Error: llc failed\n";
        return 1;
    }

    // 5. Link assembly + runtime -> executable
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