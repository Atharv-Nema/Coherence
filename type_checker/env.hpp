#pragma once
#include "../ast/top_level.hpp"
#include <unordered_map>
// map from type variables to the concrete types
using TypeContext = std::unordered_map<std::string, std::shared_ptr<NameableType>>;

// map from variables to their type
using VarContext = std::unordered_map<std::string, FullType>;

// map from function names to their declaration
using FuncNameMap = std::unordered_map<std::string, std::shared_ptr<TopLevelItem::Func>>;

// For now, I will not allow for variable name shadowing. Note that there are no top level variables
using ActorNameMap = std::unordered_map<std::string, std::shared_ptr<TopLevelItem::Actor>>;

struct TypeEnv {
    TypeContext type_context;
    VarContext var_context;
    FuncNameMap func_name_map;
    ActorNameMap actor_name_map;
};