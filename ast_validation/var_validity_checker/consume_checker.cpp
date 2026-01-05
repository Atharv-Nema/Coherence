#include "consume_checker.hpp"
#include "utils.hpp"
#include <unordered_set>
#include <iostream>

using ValidityInfo = ScopedStore<std::string, bool>;

// Returns whether [val_expr] has any consume violations. It also modifies the outermost scope of
// [var_valid] with the newly consumed variables
bool update_valexpr_validity_info(
    ValidityInfo& var_valid,
    std::shared_ptr<ValExpr> val_expr) {
}

std::optional<std::unordered_set<std::string>> get_scope_consume_variable(
    ValidityInfo& var_valid,
    const std::vector<std::shared_ptr<Stmt>>& stmt_list);

// This returns whether [stmt] has any consume violations. It also modifies the outermost scope of
// [var_valid] with the newly consumed variables
bool update_stmt_validity_info(
    ValidityInfo& var_valid,
    std::shared_ptr<Stmt> stmt) {
    return std::visit(Overload{
        [&](const Stmt::VarDeclWithInit& var_decl_init) {
            var_valid.insert(var_decl_init.name, true);
            return true;
        },
        [&](const Stmt::BehaviourCall& be_call) {
            if(!update_valexpr_validity_info(var_valid, be_call.actor)) {
                return false;
            }
            for(auto arg_expr: be_call.args) {
                if(!update_valexpr_validity_info(var_valid, arg_expr)) {
                    return false;
                }
            }
            return true;
        },
        [&](const Stmt::Print& print_expr) {
            return update_valexpr_validity_info(var_valid, print_expr.print_expr);
        },
        [&](const Stmt::Expr& expr) {
            return update_valexpr_validity_info(var_valid, expr.expr);
        },
        [&](const Stmt::If& if_stmt) {
            if(!update_valexpr_validity_info(var_valid, if_stmt.cond)) {
                return false;
            }
            auto then_block_consumed = get_scope_consume_variable(var_valid, if_stmt.then_body);
            if(!then_block_consumed) {
                return false;
            }
            // Note that as [then_block_consumed] is not std::nullopt, we know that no consume violations
            // occured in that branch.
            if(if_stmt.else_body != std::nullopt) {
                auto else_block_consumed = get_scope_consume_variable(var_valid, *if_stmt.else_body);
                if(!else_block_consumed) {
                    return false;
                }
                for(const std::string& else_consumed: *else_block_consumed) {
                    var_valid.insert(else_consumed, false);
                }
            }
            // Adding the 'then block' variables now to prevent interference with the else block
            for(const std::string& then_consumed: *then_block_consumed) {
                var_valid.insert(then_consumed, false);
            }
            return true;
        },
        [&](const Stmt::While& while_stmt) {
            // This is the most tricky case. The condition is always executed atleast once.
            // If something is consumed in the condition but replinished in the body, this is fine
            // (the compiler will still assume it is consumed). But if it is not replinished in the body,
            // this is bad (double consume). Easy way to implement is to wrap the condition in a statement
            // and then pass it along with the body. Then call the val_expr function on the condition
            std::vector<std::shared_ptr<Stmt>> loop_stmts;
            loop_stmts.reserve(while_stmt.body.size() + 1);
            loop_stmts.push_back(
                std::make_shared<Stmt>(Stmt{while_stmt.cond->source_span, Stmt::Expr {while_stmt.cond}}));
            loop_stmts.insert(loop_stmts.end(), while_stmt.body.begin(), while_stmt.body.end());
            auto body_consumed = get_scope_consume_variable(var_valid, loop_stmts);
            if(body_consumed == std::nullopt) {
                return false;
            }
            if(body_consumed->size() > 0) {
                report_error_location(stmt->source_span);
                std::cerr << "There is a possibility of double-consume due to the repeated execution of the loop" 
                << std::endl;
                return false;
            }
            bool cond_consume_check = update_valexpr_validity_info(var_valid, consumed_vars, while_stmt.cond);
            assert(cond_consume_check);
            return true;
        },
        [&](const Stmt::Atomic& atomic_stmt) {
            auto atomic_consumed = get_scope_consume_variable(var_valid, atomic_stmt.body);
            if(atomic_consumed == std::nullopt) {
                return false;
            }
            consumed_vars.insert(atomic_consumed->begin(), atomic_consumed->end());
            return true;
        }
    }, stmt->t);
}

// Returns variables whose status was changed from the outer scope scene. It does this by adding consumed variables to [var_valid]
// that were not already in the latest scope. Also, it refreshes variables only in the latest scope. At the end, it
// pops the outer scope and returns the variables in it. If it ever encounters a consume violation, it reports an
// error
std::optional<std::unordered_map<std::string, bool>> get_scope_consume_variable(
    ValidityInfo& var_valid,
    const std::vector<std::shared_ptr<Stmt>>& stmt_list) {
    // Returns the variables that will be consumed from the upper scopes after control exits this scope
    var_valid.create_new_scope();
    std::unordered_set<std::string> consumed_vars;
    for(std::shared_ptr<Stmt> stmt: stmt_list) {
        
    }
    
    
}


bool consume_check(std::vector<std::shared_ptr<Stmt>>& stmt_list) {

}