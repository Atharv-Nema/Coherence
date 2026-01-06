#pragma once
#include "top_level.hpp"
#include "pattern_matching_boilerplate.hpp"
#include "scoped_store.cpp"
#include "general_validator_structs.hpp"

bool initialization_check(
    std::shared_ptr<TopLevelItem::Actor> curr_actor,
    std::shared_ptr<ActorFrontend> actor_frontend,
    std::shared_ptr<TopLevelItem::Constructor> constructor_def);