#include "var_validity_checker.hpp"
#include "scoped_store.cpp"
#include "utils.hpp"
#include <iostream>
#include "pattern_matching_boilerplate.hpp"
#include "alpha_renaming.hpp"
#include "override_checker.hpp"
#include "defer.cpp"
#include "consume_checker.hpp"

struct ValidityEnv {
    std::unordered_set<std::string> types;
    std::shared_ptr<TopLevelItem::Actor> curr_actor;
};

bool var_validity_check_callable(
    ValidityEnv& env,
    std::vector<TopLevelItem::VarDecl>& params,
    std::vector<std::shared_ptr<Stmt>>& callable_body) {
    // Override check the callable
    if(!override_check_callable(env.curr_actor, params, callable_body)) {
        return false;
    }
    // Alpha rename the callable body
    alpha_rename_callable_body(callable_body);

    // Now run the consume checker
    if(!consume_check(callable_body)) {
        return false;
    }
    return true;
}

bool var_validity_check_toplevel_item(ValidityEnv& env, TopLevelItem toplevel_item) {
    return std::visit(Overload{
        [&](const TopLevelItem::TypeDef& type_def) {
            if(env.types.contains(type_def.type_name)) {
                report_error_location(toplevel_item.source_span);
                std::cerr << "Type " << type_def.type_name << " already defined" << std::endl;
                return false;
            }
            return true;
        },
        [&](std::shared_ptr<TopLevelItem::Func> func_def) {
            return var_validity_check_callable(env, func_def->params, func_def->body);
        },
        [&](std::shared_ptr<TopLevelItem::Actor> actor_def) {
            env.curr_actor = actor_def;
            Defer d([&](){env.curr_actor = nullptr;});
            bool validity_checks = true;
            for(auto actor_mem: actor_def->actor_members) {
                bool mem_checks = std::visit(Overload{
                    [&](auto const& def) {
                        // I am using the fact that the given expression works for all the different definitions
                        return var_validity_check_callable(env, def->params, def->body);
                    }
                }, actor_mem);
                validity_checks = validity_checks && mem_checks;
            }
            return validity_checks;
        }
    }, toplevel_item.t);
}

bool var_validity_check_program(Program* root) {
    ValidityEnv validity_env;
    validity_env.curr_actor = nullptr;
    bool validity_checks = true;
    for(auto top_level_item: root->top_level_items) {
        validity_checks = var_validity_check_toplevel_item(validity_env, top_level_item);
    }
    return validity_checks;
}