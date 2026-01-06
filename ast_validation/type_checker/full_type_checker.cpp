#include "full_type_checker.hpp"
#include "core_type_checker.hpp"
#include "return_checker.hpp"
#include "initialization_checker.hpp"
#include "pattern_matching_boilerplate.hpp"
#include "utils.hpp"
#include "defer.cpp"
#include <assert.h>
#include <iostream>

bool type_check_function(TypeEnv& env, std::shared_ptr<TopLevelItem::Func> func_def) {
    if(env.func_name_map.key_in_curr_scope(func_def->name)) {
        std::cerr << "Function already defined" << std::endl;
        return false;
    }
    env.func_name_map.insert(func_def->name, func_def);
    if(!type_check_callable_body(env, func_def, func_def->params, func_def->body)) {
        return false;
    }
    // Now checking that the last statement actually returns something
    if(func_def->body.size() == 0) {
        std::cerr << "Function does not return" << std::endl;
        return false;
    }
    if(!statement_returns(func_def->body.back())) {
        std::cerr << "Last statement of the function does not return" << std::endl;
        return false;
    }
    return true;
}

bool type_check_behaviour(TypeEnv& env, std::shared_ptr<TopLevelItem::Behaviour> behaviour_def) {
    return type_check_callable_body(env, nullptr, behaviour_def->params, behaviour_def->body);
}

bool type_check_constructor(TypeEnv& env, std::shared_ptr<TopLevelItem::Constructor> constructor_def) {
    if(!type_check_callable_body(env, nullptr, constructor_def->params, constructor_def->body)) {
        return false;
    }
    // After this is done, we do a recursive check of the initialization
    return initialization_check(env.curr_actor, env.func_name_map, constructor_def);
}



bool type_check_toplevel_item(TypeEnv& env, TopLevelItem toplevel_item) {
    return std::visit(Overload{
        [&](const TopLevelItem::TypeDef& type_def) {
            assert(!env.type_context.contains(type_def.type_name));
            env.type_context.emplace(type_def.type_name, type_def.nameable_type);
            return true;
        },
        [&](std::shared_ptr<TopLevelItem::Func> func_def) {
            return type_check_function(env, func_def);
        },
        [&](std::shared_ptr<TopLevelItem::Actor> actor_def) {
            // Checking that the fields are unique
            if(env.actor_frontend_map.find(actor_def->name) != env.actor_frontend_map.end()) {
                report_error_location(toplevel_item.source_span);
                std::cerr << "Actor " << actor_def->name << " already defined." << std::endl;
                return false;
            }
            // Create an actor frontend
            std::shared_ptr<ActorFrontend> actor_frontend = std::make_shared<ActorFrontend>();
            actor_frontend->actor_name = actor_def->name;
            env.actor_frontend_map.emplace(actor_def->name, actor_frontend);
            
            // Creating a scope for the functions within the actor
            ScopeGuard actor_func_scope(env.func_name_map);
            env.curr_actor = actor_def;
            Defer d([&](void){env.curr_actor = nullptr;});
            bool type_checked_successfully = true;
            // Typecheck members as they come
            for(auto actor_mem: actor_def->actor_members) {
                type_checked_successfully =  std::visit(Overload{
                    [&](std::shared_ptr<TopLevelItem::Func> mem_func) {
                        return type_check_function(env, mem_func);
                    },
                    [&](std::shared_ptr<TopLevelItem::Behaviour> mem_behaviour) {
                        actor_frontend->member_behaviours[mem_behaviour->name] = mem_behaviour;
                        return type_check_behaviour(env, mem_behaviour);
                    },
                    [&](std::shared_ptr<TopLevelItem::Constructor> mem_constructor) {
                        actor_frontend->constructors[mem_constructor->name] = mem_constructor;
                        return type_check_constructor(env, mem_constructor);
                    }
                }, actor_mem) && type_checked_successfully;
            }
            return type_checked_successfully;
        }
    }, toplevel_item.t);
}

bool type_check_program(Program* root) {
    bool program_typechecks = true;
    TypeEnv env;
    env.func_name_map.create_new_scope();
    for(auto top_level_item: root->top_level_items) {
        program_typechecks = type_check_toplevel_item(env, top_level_item) && program_typechecks;
    }
    if(!program_typechecks) {
        // CR: This error message is probably redundant
        std::cerr << "Program does not typecheck" << std::endl;
        return false;
    }
    // Checking whether there is an actor with name [Main]
    if(env.actor_frontend_map.find("Main") == env.actor_frontend_map.end()) {
        std::cerr << "Error: Actor Main not found" << std::endl;
        return false;
    }
    return true;
}