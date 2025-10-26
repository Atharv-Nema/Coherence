#pragma once
#include "../ast/expr.hpp"
#include "env.hpp"
#include "overload.hpp"
#include <assert.h>
#include "type_checker.hpp"

template<typename ... Ts>
struct Overload : Ts ... { 
    using Ts::operator() ...;
};
template<class... Ts> Overload(Ts...) -> Overload<Ts...>;

BasicType extract_basic_type(const FullType& full_type);
std::optional<BasicType> standardize_type(TypeContext& type_context, const std::string& type_name);
bool can_appear_in_lhs(TypeEnv& env, std::shared_ptr<ValExpr> expr);
bool type_is_numeric(TypeContext& type_context, const FullType& type);
bool ref_cap_equal(Cap c1, Cap c2);
bool capabilities_assignable(Cap c1, Cap c2);
bool basic_type_equal(TypeContext& type_context, const BasicType& type_1, const BasicType& type_2);
bool full_type_equal(TypeContext& type_context, const FullType& type_1, const FullType& type_2);
bool type_assignable(TypeContext& type_context, const FullType& lhs, const FullType& rhs);
bool passed_in_parameters_valid(TypeEnv& env, const std::vector<TopLevelItem::VarDecl>& signature, 
    const std::vector<std::shared_ptr<ValExpr>>& arguments);

std::optional<BasicType> dereferenced_type(TypeEnv& env, FullType type);