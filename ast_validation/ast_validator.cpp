#include "var_validity_checker.hpp"
#include "full_type_checker.hpp"

bool validate_program(Program* root) {
    // 1. Run [var_validity_checker]
    if(!var_validity_check_program(root)) {
        return false;
    }
    if(!type_check_program(root)) {
        return false;
    }
    // TODO: Implement the atomic section stage
    return true;
}


