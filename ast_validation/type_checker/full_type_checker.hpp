#pragma once
#include "top_level.hpp"
#include "scoped_store.cpp"
#include "general_validator_structs.hpp"

struct TypeEnv {
    TypeContext type_context;
    std::unordered_map<std::string, std::shared_ptr<TopLevelItem::Func>> func_name_map;
    std::unordered_map<std::string, std::shared_ptr<ActorFrontend>> actor_frontend_map;
    std::shared_ptr<TopLevelItem::Actor> curr_actor;
};

bool type_check_program(
    Program *root,
    std::unordered_map<std::string, std::shared_ptr<TopLevelItem::Func>> func_name_map,
    std::unordered_map<std::string, std::shared_ptr<ActorFrontend>> actor_frontend_map);