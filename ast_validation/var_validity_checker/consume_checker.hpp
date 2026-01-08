#pragma once
#include "top_level.hpp"

bool consume_check(
    std::vector<TopLevelItem::VarDecl>& params,
    std::vector<std::shared_ptr<Stmt>>& stmt_list);