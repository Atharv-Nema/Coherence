#pragma once
#include "top_level.hpp"
#include "pattern_matching_boilerplate.hpp"
#include "scoped_store.cpp"

bool initialization_check(
    std::shared_ptr<TopLevelItem::Actor> curr_actor,
    ScopedStore<std::string, std::shared_ptr<TopLevelItem::Func>>& func_name_map,
    std::shared_ptr<TopLevelItem::Constructor> constructor_def);