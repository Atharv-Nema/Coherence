#include "var_validity_checker.hpp"
#include "full_type_checker.hpp"
#include "declaration_collector.hpp"

bool validate_program(Program* root) {
    // 1. Run [var_validity_checker]
    if(!var_validity_check_program(root)) {
        return false;
    }
    // 2. Collect the declaration
    std::unordered_map<std::string, std::shared_ptr<TopLevelItem::Func>> func_name_map;
    std::unordered_map<std::string, std::shared_ptr<ActorFrontend>> actor_frontend_map;
    if(!fill_declaration_info(func_name_map, actor_frontend_map, root)) {
        return false;
    }
     
    if(!type_check_program(root, func_name_map, actor_frontend_map)) {
        return false;
    }
    // TODO: Implement the atomic section stage
    return true;
}


