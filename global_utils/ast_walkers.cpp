#include "ast_walkers.hpp"
#include "pattern_matching_boilerplate.hpp"

// CR: Better name
// Unwraps a valexpr once (looked at through valexpr's) and applies the function to the subcases. Ignores
// the default cases. Follows predicate logic and stops early.
bool predicate_valexpr_walker(
    std::shared_ptr<ValExpr> val_expr, 
    std::function<bool(std::shared_ptr<ValExpr>)> predicate) {
    return std::visit(Overload{
        [&](ValExpr::VStruct& struct_val) {
            for(auto& [field_name, field_expr]: struct_val.fields) {
                if(!predicate(field_expr)) {
                    return false;
                }
            }
            return true;
        },
        [&](ValExpr::NewInstance& new_instance) {
            return predicate(new_instance.init_expr) && predicate(new_instance.size);
        },
        [&](ValExpr::ActorConstruction& actor_construction) {
            for(auto arg: actor_construction.args) {
                if(!predicate(arg)) {
                    return false;
                }
            }
            return true;
        },
        [&](ValExpr::PointerAccess& pointer_access) {
            // First the pointer then the index
            return predicate(pointer_access.value) && predicate(pointer_access.index);
        },
        [&](const ValExpr::Field& field_access) {
            return predicate(field_access.base);
        },
        [&](const ValExpr::Assignment& assignment) {
            return predicate(assignment.lhs) && predicate(assignment.rhs);
        },
        [&](const ValExpr::FuncCall& func_call) {
            for(auto arg: func_call.args) {
                if(!predicate(arg)) {
                    return false;
                }
            }
            return true;
        },
        [&](const ValExpr::BinOpExpr& bin_op_expr) {
            return predicate(bin_op_expr.lhs) && predicate(bin_op_expr.rhs);
        },
        [&](const auto&) {return true;}
    }, val_expr->t);
}

// Same as above but applies it fully
void visitor_valexpr_walker(
    std::shared_ptr<ValExpr> val_expr, 
    std::function<void(std::shared_ptr<ValExpr>)> visitor) {
    predicate_valexpr_walker(
        val_expr, 
        [&](std::shared_ptr<ValExpr> val_expr) {
            visitor(val_expr); 
            return true;
        });
}

// CR: THE NAMES ARE SO BAD HELP
void valexpr_and_stmt_visitors_stmt_walker(
    std::shared_ptr<Stmt> stmt,
    std::function<void(std::shared_ptr<ValExpr>)> val_expr_visitor,
    std::function<void(std::shared_ptr<Stmt>)> stmt_visitor) {
    std::visit(Overload{
        [&](Stmt::VarDeclWithInit& var_decl_init) {
            val_expr_visitor(var_decl_init.init);
        },
        [&](Stmt::MemberInitialize& mem_init) {
            val_expr_visitor(mem_init.init);
        },
        [&](Stmt::BehaviourCall& be_call) {
            val_expr_visitor(be_call.actor);
            for(std::shared_ptr<ValExpr> arg: be_call.args) {
                val_expr_visitor(arg);
            }
        },
        [&](Stmt::Print& print_expr) {
            val_expr_visitor(print_expr.print_expr);
        },
        [&](Stmt::Expr& expr) {
            val_expr_visitor(expr.expr);
        },
        [&](Stmt::If& if_stmt) {
            val_expr_visitor(if_stmt.cond);
            for(std::shared_ptr<Stmt> then_stmt: if_stmt.then_body) {
                stmt_visitor(then_stmt);
            }
            if(if_stmt.else_body != std::nullopt) {
                for(std::shared_ptr<Stmt> else_stmt: *if_stmt.else_body) {
                    stmt_visitor(else_stmt);
                }
            }
        },
        [&](Stmt::While& while_stmt) {
            val_expr_visitor(while_stmt.cond);
            for(std::shared_ptr<Stmt> body_stmt: while_stmt.body) {
                stmt_visitor(body_stmt);
            }
        },
        [&](std::shared_ptr<Stmt::Atomic> atomic_stmt) {
            for(std::shared_ptr<Stmt> body_stmt: atomic_stmt->body) {
                stmt_visitor(body_stmt);
            }
        },
        [&](Stmt::Return& return_stmt) {
            val_expr_visitor(return_stmt.expr);
        }
    }, stmt->t);
}

// Unpacks non-base case statements recursively using visitor_stmt_walker and applies the valexpr function
// to the 
void valexpr_visitor_stmt_walker(
    std::shared_ptr<Stmt> stmt,
    std::function<void(std::shared_ptr<ValExpr>)> visitor) {
    std::function<void(std::shared_ptr<Stmt>)> stmt_walker;
    stmt_walker = [&](std::shared_ptr<Stmt> stmt) {
        return valexpr_and_stmt_visitors_stmt_walker(stmt, visitor, stmt_walker);
    };
    valexpr_and_stmt_visitors_stmt_walker(stmt, visitor, stmt_walker);
}