#include "var_validity_checker.hpp"
#include "full_type_checker.hpp"
#include "declaration_collector.hpp"
#include "compute_lock_info.hpp"
#include "debug_printer.cpp"

bool validate_program(Program* root) {

    // 1. Run [var_validity_checker]
    if(!var_validity_check_program(root)) {
        return false;
    }
    // 2. Collect the declaration
    std::shared_ptr<DeclCollection> decl_collection = std::make_shared<DeclCollection>();
    if(!fill_declaration_info(root, decl_collection)) {
        return false;
    }
     
    if(!type_check_program(root, decl_collection)) {
        return false;
    }
    compute_lock_info(root, decl_collection);
    return true;
}


