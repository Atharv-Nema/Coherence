#include "consume_checker.hpp"
#include "utils.hpp"
#include "scoped_store.cpp"
#include "pattern_matching_boilerplate.hpp"
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <iostream>

// CR: Think about a way to reuse the common part of the traversing logic
bool update_valexpr_validity_info(
    ScopedStore<std::string, bool>& var_valid,
    std::shared_ptr<ValExpr> val_expr) {
    return std::visit(Overload{
        // Cases where the actual checking occurs
        [&](ValExpr::VVar& var) {
            auto var_status = var_valid.get_value(var.name);
            if(var_status == std::nullopt) {
                // There will be variables that are not "local". Using them is assumed to be fine (errors here
                // are not the responsibility of this stage)
                return true;
            }
            return *var_status;
        },
        [&](ValExpr::Consume& consume) {
            auto var_status = var_valid.get_value(consume.var_name);
            if(var_status == std::nullopt) {
                report_error_location(val_expr->source_span);
                std::cerr << "Consuming a variable that either has not been defined or is not local" << std::endl;
                return false;
            }
            if(*var_status == false) {
                std::cerr << "Consuming a variable twice" << std::endl;
            }
            var_valid.insert(consume.var_name, false);
            return true;
        },
        [&](ValExpr::Assignment& assignment) {
            // The lhs is compiled before the rhs. But cases like [a = consume a] make this a bit more subtle
            // That is why you see a slightly odd repetition of code here
            ValExpr::VVar* lhs_var;
            if((lhs_var = std::get_if<ValExpr::VVar>(&assignment.lhs->t)) != nullptr) {
                // We just compile the rhs as the lhs cannot influence the consume status and then refresh the lhs
                if(!update_valexpr_validity_info(var_valid, assignment.rhs)) {
                    return false;
                }
                if(var_valid.get_value(lhs_var->name) != std::nullopt) {
                    var_valid.insert(lhs_var->name, true);
                }
                return true;
            }
            else {
                // Compile the lhs and then the rhs normally
                if(!update_valexpr_validity_info(var_valid, assignment.lhs)) {
                    return false;
                }
                if(!update_valexpr_validity_info(var_valid, assignment.rhs)) {
                    return false;
                }
                return true;
            }
        },
        // Cases where need to propogate recursively downwards
        [&](ValExpr::VStruct& struct_val) {
            for(const auto& [_, field_expr]: struct_val.fields) {
                if(!update_valexpr_validity_info(var_valid, field_expr)) {
                    return false;
                }
            }
            return true;
        },
        [&](ValExpr::NewInstance& new_instance) {
            // Order of evaluation is default value, size
            if(!update_valexpr_validity_info(var_valid, new_instance.default_value)) {
                return false;
            }
            if(!update_valexpr_validity_info(var_valid, new_instance.size)) {
                return false;
            }
            return true;
        },
        [&](ValExpr::ActorConstruction& actor_construction) {
            for(auto arg: actor_construction.args) {
                if(!update_valexpr_validity_info(var_valid, arg)) {
                    return false;
                }
            }
            return true;
        },
        [&](ValExpr::PointerAccess& pointer_access) {
            // First the pointer then the index
            if(!update_valexpr_validity_info(var_valid, pointer_access.value)) {
                return false;
            }
            if(!update_valexpr_validity_info(var_valid, pointer_access.index)) {
                return false;
            }
            return true;
        },
        [&](const ValExpr::Field& field_access) {
            return update_valexpr_validity_info(var_valid, field_access.base);
        },
        [&](const ValExpr::FuncCall& func_call) {
            for(auto arg: func_call.args) {
                if(!update_valexpr_validity_info(var_valid, arg)) {
                    return false;
                }
            }
            return true;
        },
        [&](const ValExpr::BinOpExpr& bin_op_expr) {
            if(!update_valexpr_validity_info(var_valid, bin_op_expr.lhs)) {
                return false;
            }
            if(!update_valexpr_validity_info(var_valid, bin_op_expr.rhs)) {
                return false;
            }
            return true;
        },
        // All other cases do not need to be considered
        [&](const auto&) {return true;}
    }, val_expr->t);
}

// Takes in all the validity updates on going through a variety of control flows and modifies [var_valid]
void apply_control_flow_validity_updates(
    ScopedStore<std::string, bool>& var_valid,
    std::vector<std::unordered_map<std::string, bool>>& control_path_updates) {
    assert(control_path_updates.size() > 0);
    std::unordered_set<std::string> consumed_vars;
    for(auto& update: control_path_updates) {
        for(const auto& [var_name, var_available]: update) {
            if(!var_available) {
                consumed_vars.insert(var_name);
            }
        }
    }
    std::unordered_set<std::string> refreshed_vars;
    for(const auto&[var_name, var_available]: control_path_updates[0]) {
        if(!var_available) {
            continue;
        }
        refreshed_vars.insert(var_name);
        for(auto& update: control_path_updates) {
            if(!(update.contains(var_name) && update.at(var_name))) {
                refreshed_vars.erase(var_name);
                break;
            }
        }
    }
    for(const std::string& consumed_var: consumed_vars) {
        var_valid.insert(consumed_var, false);
    }
    for(const std::string& refreshed_var: refreshed_vars) {
        var_valid.insert(refreshed_var, true);
    }
}


std::optional<std::unordered_map<std::string, bool>> get_scope_validity_change(
    ScopedStore<std::string, bool>& var_valid,
    const std::vector<std::shared_ptr<Stmt>>& stmt_list);

bool update_stmt_validity_info(
    ScopedStore<std::string, bool>& var_valid,
    std::shared_ptr<Stmt> stmt) {
    return std::visit(Overload{
        [&](const Stmt::VarDeclWithInit& var_decl_init) {
            if(!update_valexpr_validity_info(var_valid, var_decl_init.init)) {
                return false;
            }
            assert(var_valid.get_value(var_decl_init.name) == std::nullopt);
            var_valid.insert(var_decl_init.name, true);
            return true;
        },
        [&](const Stmt::MemberInitialize& mem_init) {
            return update_valexpr_validity_info(var_valid, mem_init.init);
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
            std::vector<std::unordered_map<std::string, bool>> control_paths(2);
            auto then_block_valchange = get_scope_validity_change(var_valid, if_stmt.then_body);
            if(!then_block_valchange) {
                return false;
            }
            control_paths[0] = *then_block_valchange;
            if(if_stmt.else_body != std::nullopt) {
                auto else_block_valchange = get_scope_validity_change(var_valid, *if_stmt.else_body);
                if(!else_block_valchange) {
                    return false;
                }
                control_paths[1] = *else_block_valchange;
            }
            apply_control_flow_validity_updates(var_valid, control_paths);
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
            auto body_valchange = get_scope_validity_change(var_valid, loop_stmts);
            if(!body_valchange) {
                return false;
            }
            for(auto& [var_name, var_available]: *body_valchange) {
                if(!var_available) {
                    std::cerr << "Variable " << orig_name(var_name) << " may be consumed twice" << std::endl;
                    return false;
                }
            }
            bool cond_consume_check = update_valexpr_validity_info(var_valid, while_stmt.cond);
            assert(cond_consume_check);
            return true;
        },
        [&](std::shared_ptr<Stmt::Atomic> atomic_stmt) {
            auto atomic_consumed = get_scope_validity_change(var_valid, atomic_stmt->body);
            if(atomic_consumed == std::nullopt) {
                return false;
            }
            std::vector<std::unordered_map<std::string, bool>> v{std::move(atomic_consumed.value())};
            apply_control_flow_validity_updates(var_valid, v);
            return true;
        },
        [&](const Stmt::Return& return_stmt) {
            return update_valexpr_validity_info(var_valid, return_stmt.expr);
        }
    }, stmt->t);
}

// Returns variables whose status was changed from the outer scope scene. It does this by adding consumed variables to 
// [var_valid] that were not already in the latest scope. Also, it refreshes variables only in the latest scope. At the 
// end, it pops the outer scope and returns the variables in it. If it ever encounters a consume violation, it reports an
// error and returns std::nullopt
std::optional<std::unordered_map<std::string, bool>> get_scope_validity_change(
    ScopedStore<std::string, bool>& var_valid,
    const std::vector<std::shared_ptr<Stmt>>& stmt_list) {
    // Returns the variables that will be consumed from the upper scopes after control exits this scope
    std::unordered_set<std::string> consumed_vars;
    for(std::shared_ptr<Stmt> stmt: stmt_list) {
        if(!update_stmt_validity_info(var_valid, stmt)) {
            return std::nullopt;
        }
    }
    std::unordered_map<std::string, bool> scope_var_availabilities = var_valid.get_latest_scope_updates();
    var_valid.pop_scope();
    for(auto it = scope_var_availabilities.begin(); it != scope_var_availabilities.end();) {
        if(var_valid.get_value(it->first) == std::nullopt)
            it = scope_var_availabilities.erase(it); // erase returns next iterator 
        else it++;
    }
    return scope_var_availabilities;
}

// Returns true if passes
bool consume_check(std::vector<std::shared_ptr<Stmt>>& stmt_list) {
    ScopedStore<std::string, bool> var_valid;
    return get_scope_validity_change(var_valid, stmt_list) != std::nullopt;
}