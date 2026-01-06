#pragma once
#include "top_level.hpp"
#include <functional>
#include "general_validator_structs.hpp"

void report_error_location(const SourceSpan& span);
std::string orig_name(const std::string& s);
std::shared_ptr<TopLevelItem::Func> get_func_def(
    const std::string& func_name, 
    std::shared_ptr<TopLevelItem::Actor> curr_actor,
    std::shared_ptr<DeclCollection> decl_collection);
bool predicate_valexpr_walker(
    std::shared_ptr<ValExpr> val_expr, 
    std::function<bool(std::shared_ptr<ValExpr>)> predicate);