#pragma once
#include "env.hpp"

std::optional<FullType> val_expr_type(TypeEnv& env, std::shared_ptr<ValExpr> val_expr);