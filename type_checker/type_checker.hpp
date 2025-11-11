#pragma once
#include "env.cpp"

std::optional<FullType> val_expr_type(TypeEnv& env, std::shared_ptr<ValExpr> val_expr);
bool type_check_statement(TypeEnv& env, std::shared_ptr<Stmt> stmt);
bool type_check_program(Program* root);