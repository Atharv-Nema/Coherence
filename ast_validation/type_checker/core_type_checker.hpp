#pragma once
#include "full_type_checker.hpp"

bool type_check_callable_body(
    TypeEnv& env,
    std::shared_ptr<TopLevelItem::Func> curr_func,
    const std::vector<TopLevelItem::VarDecl>& args,
    const std::vector<std::shared_ptr<Stmt>>& callable_body);

