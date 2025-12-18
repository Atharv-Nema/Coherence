#pragma once
#include "expr.hpp"
#include "type_checker.hpp"
#include "env.cpp"
#include "pattern_matching_boilerplate.hpp"

void report_error_location(const SourceSpan& span);
BasicType extract_basic_type(const FullType& full_type);
std::optional<BasicType> standardize_type(TypeContext& type_context, const std::string& type_name);
bool can_appear_in_lhs(TypeEnv& env, std::shared_ptr<ValExpr> expr);
bool type_is_numeric(TypeContext& type_context, const FullType& type);
bool ref_cap_equal(Cap c1, Cap c2);
bool capabilities_assignable(Cap c1, Cap c2);
bool basic_type_equal(TypeContext& type_context, const BasicType& type_1, const BasicType& type_2);
bool full_type_equal(TypeContext& type_context, const FullType& type_1, const FullType& type_2);
bool type_assignable(TypeContext& type_context, const FullType& lhs, const FullType& rhs);
std::optional<FullType> get_actor_member_type(TypeEnv& env, const std::string& var_name);
std::optional<FullType> get_variable_type(TypeEnv& env, const std::string& var_name);
bool variable_overridable_in_scope(TypeEnv& env, const std::string& var_name);
bool passed_in_parameters_valid(TypeEnv &env, const std::vector<TopLevelItem::VarDecl> &signature,
                                const std::vector<std::shared_ptr<ValExpr>> &arguments, bool parameters_being_sent);
bool struct_valid(TypeEnv &env, NameableType::Struct& struct_contents, ValExpr::VStruct& struct_);
bool type_check_stmt_list(TypeEnv &env, const std::vector<std::shared_ptr<Stmt>>& stmt_list);
std::optional<NameableType::Struct> get_struct_type(TypeEnv& env, const std::string& struct_type_name);
FullType unaliased_type(TypeEnv &env, const FullType& full_type);
bool statement_returns(TypeEnv &env, std::shared_ptr<Stmt> last_statement);
bool type_check_function(TypeEnv& env, std::shared_ptr<TopLevelItem::Func> func_def, SourceSpan& source_span);
bool type_check_behaviour(TypeEnv& env, std::shared_ptr<TopLevelItem::Behaviour> behaviour_def);
bool type_check_constructor(TypeEnv& env, std::shared_ptr<TopLevelItem::Constructor> constructor_def);