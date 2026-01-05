#pragma once
#include "top_level.hpp"
#include "scoped_store.cpp"

using TypeContext = std::unordered_map<std::string, std::shared_ptr<NameableType>>;

struct ActorFrontend {
    std::string actor_name;
    std::unordered_map<std::string, std::shared_ptr<TopLevelItem::Constructor>>
        constructors;
    std::unordered_map<std::string, std::shared_ptr<TopLevelItem::Behaviour>>
        member_behaviours;
};

struct TypeEnv {
    TypeContext type_context;
    ScopedStore<std::string, std::shared_ptr<TopLevelItem::Func>> func_name_map;
    std::unordered_map<std::string, std::shared_ptr<ActorFrontend>> actor_frontend_map;
    std::shared_ptr<TopLevelItem::Actor> curr_actor;
};

bool type_check_program(Program *root);