#include <functional>
#include "top_level.hpp"

bool predicate_valexpr_walker(
    std::shared_ptr<ValExpr> val_expr, 
    std::function<bool(std::shared_ptr<ValExpr>)> predicate);
void visitor_valexpr_walker(
    std::shared_ptr<ValExpr> val_expr, 
    std::function<void(std::shared_ptr<ValExpr>)> visitor);
void valexpr_and_stmt_visitors_stmt_walker(
    std::shared_ptr<Stmt> stmt,
    std::function<void(std::shared_ptr<ValExpr>)> val_expr_visitor,
    std::function<void(std::shared_ptr<Stmt>)> stmt_visitor);
void valexpr_visitor_stmt_walker(
    std::shared_ptr<Stmt> stmt,
    std::function<void(std::shared_ptr<ValExpr>)> visitor);