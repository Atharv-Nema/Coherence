#pragma once
#include "func_actors.hpp"

struct TopLevelItem {
    struct TypeDefI { TypeDef def; };
    struct FuncDef { Func fn; };
    struct ActorDef { Actor actor; };
    std::variant<TypeDefI, FuncDef, ActorDef> value;
};

struct Program {
    std::vector<std::string> lock_names;
    std::vector<TopLevelItem> top_level_items;
};
