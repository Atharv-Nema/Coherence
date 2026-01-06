#pragma once
#include "expr.hpp"
#include <functional>

void report_error_location(const SourceSpan& span);
std::string orig_name(const std::string& s);
bool predicate_valexpr_walker(
    std::shared_ptr<ValExpr> val_expr, 
    std::function<bool(std::shared_ptr<ValExpr>)> predicate);