#pragma once
#include "top_level.hpp"

// Sometimes, different stages need to use the same structure. These structures are put here.
// Ideally, we should try to separate out the different stages as much as possible, but sometimes
// it cannot be helped. 

using TypeContext = std::unordered_map<std::string, std::shared_ptr<NameableType>>;

struct ActorFrontend {
    std::string actor_name;

    std::unordered_map<std::string, std::shared_ptr<TopLevelItem::Constructor>>
        constructors;
    std::unordered_map<std::string, std::shared_ptr<TopLevelItem::Behaviour>>
        member_behaviours;
    std::unordered_map<std::string, std::shared_ptr<TopLevelItem::Func>>
        member_functions;
};

struct DeclCollection {
    std::unordered_map<std::string, std::shared_ptr<TopLevelItem::Func>> func_name_map;
    std::unordered_map<std::string, std::shared_ptr<ActorFrontend>> actor_frontend_map;
};