#pragma once
#include "top_level.hpp"

std::unordered_map<std::string, FullType> alpha_rename_callable_body(
    std::vector<std::shared_ptr<Stmt>> body);