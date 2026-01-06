#pragma once
#include "top_level.hpp"
#include "scoped_store.cpp"
#include "general_validator_structs.hpp"

struct TypeEnv {
    TypeContext type_context;
    std::shared_ptr<DeclCollection> decl_collection;
    std::shared_ptr<TopLevelItem::Actor> curr_actor;
};

bool type_check_program(
    Program *root,
    std::shared_ptr<DeclCollection> decl_collection);