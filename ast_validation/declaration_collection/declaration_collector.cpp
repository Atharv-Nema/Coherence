#include "declaration_collector.hpp"
#include "pattern_matching_boilerplate.hpp"
#include <iostream>
#include "utils.hpp"

bool fill_actor_info(
    std::unordered_map<std::string, std::shared_ptr<ActorFrontend>>& actor_frontend_map,
    std::shared_ptr<TopLevelItem::Actor> actor_def) {
    if(actor_frontend_map.contains(actor_def->name)) {
        std::cerr << "Actor " << actor_def->name << " already defined" << std::endl;
        return false;
    }
    std::shared_ptr<ActorFrontend> actor_frontend = std::make_shared<ActorFrontend>();
    actor_frontend->actor_name = actor_def->name;
    for(auto& actor_mem: actor_def->actor_members) {
        bool result = std::visit(Overload{
            [&](std::shared_ptr<TopLevelItem::Func> mem_func) {
                if(actor_frontend->member_functions.contains(mem_func->name)) {
                    std::cerr << "Function " << mem_func->name << " already defined" << std::endl;
                    return false;
                }
                actor_frontend->member_functions.emplace(mem_func->name, mem_func);
                return true;
            },
            [&](std::shared_ptr<TopLevelItem::Constructor> mem_constructor) {
                if(actor_frontend->constructors.contains(mem_constructor->name)) {
                    std::cerr << "Constructor " << mem_constructor->name << " already defined" << std::endl;
                    return false;
                }
                actor_frontend->constructors.emplace(mem_constructor->name, mem_constructor);
                return true;
            },
            [&](std::shared_ptr<TopLevelItem::Behaviour> mem_behaviour) {
                if(actor_frontend->member_behaviours.contains(mem_behaviour->name)) {
                    std::cerr << "Behaviour " << mem_behaviour->name << " already defined" << std::endl;
                    return false;
                }
                actor_frontend->member_behaviours.emplace(mem_behaviour->name, mem_behaviour);
                return true;
            }
        }, actor_mem);
        if(!result) {
            return false;
        }
    }
    actor_frontend_map.emplace(actor_def->name, actor_frontend);
    return true;
}

bool fill_declaration_info(
    std::unordered_map<std::string, std::shared_ptr<TopLevelItem::Func>>& func_name_map,
    std::unordered_map<std::string, std::shared_ptr<ActorFrontend>>& actor_frontend_map,
    Program *root) {
    for(auto toplevel_item: root->top_level_items) {
        bool result = std::visit(Overload{
            [&](TopLevelItem::TypeDef&){return true;},
            [&](std::shared_ptr<TopLevelItem::Func> func_def) {
                if(func_name_map.contains(func_def->name)) {
                    report_error_location(toplevel_item.source_span);
                    std::cerr << "Function " << func_def->name << " already defined" << std::endl;
                    return false;
                }
                return true;
            },
            [&](std::shared_ptr<TopLevelItem::Actor> actor_def) {
                return fill_actor_info(actor_frontend_map, actor_def);
            }
        }, toplevel_item.t);
        if(!result) {
            return false;
        }
    }
    return true;
}