#include "initialization_checker.hpp"
#include "general_validator_structs.hpp"
#include "utils.hpp"

struct InitEnv {
    std::shared_ptr<TopLevelItem::Actor> curr_actor;
    std::shared_ptr<ActorFrontend> actor_frontend;
    InitEnv(
        std::shared_ptr<TopLevelItem::Actor> curr_actor,
        std::shared_ptr<ActorFrontend> actor_frontend) {
        this->curr_actor = curr_actor;
        this->actor_frontend = actor_frontend;
    }
};


bool valexpr_accesses_vars(const std::unordered_set<std::string>& vars, std::shared_ptr<ValExpr> val_expr);

bool valexpr_list_accesses_vars(
    const std::unordered_set<std::string>& vars, 
    const std::vector<std::shared_ptr<ValExpr>>& val_expr_list) {
    for(std::shared_ptr<ValExpr> val_expr: val_expr_list) {
        if(valexpr_accesses_vars(vars, val_expr)) {
            return true;
        }
    }
    return false;
}
// CR: This is a general comment. Think carefully about the shadowing of the members of an actor

bool valexpr_accesses_vars(const std::unordered_set<std::string>& vars, std::shared_ptr<ValExpr> val_expr) {
    // Returns whether [valexpr] accesses any variable in [vars]
    // CR: Think carefully about do I want to log error messages here when I get to that
    return std::visit(Overload{
        [&](const ValExpr::VVar& var) {
            return vars.find(var.name) != vars.end();
        },
        [&](const ValExpr::VStruct& struct_instance) {
            // CR: Try using advanced C++ features to do this functionally using [valexpr_list_accesses_vars]
            for(const auto&[k, v]: struct_instance.fields) {
                if(valexpr_accesses_vars(vars, v)) {
                    return true;
                }
            }
            return false;
        },
        [&](const ValExpr::NewInstance& new_instance) {
            return valexpr_accesses_vars(vars, new_instance.default_value) || valexpr_accesses_vars(vars, new_instance.size);
        },
        [&](const ValExpr::ActorConstruction& actor_construction) {
            return valexpr_list_accesses_vars(vars, actor_construction.args);
        },
        [&](const ValExpr::Consume& consume) {
            return vars.find(consume.var_name) != vars.end();
        },
        [&](const ValExpr::PointerAccess& pointer_access) {
            return valexpr_accesses_vars(vars, pointer_access.index) || valexpr_accesses_vars(vars, pointer_access.value);
        },
        [&](const ValExpr::Field& field_access) {
            return valexpr_accesses_vars(vars, field_access.base);
        },
        [&](const ValExpr::Assignment& assignment) {
            return valexpr_accesses_vars(vars, assignment.lhs) || valexpr_accesses_vars(vars, assignment.rhs);
        },
        [&](const ValExpr::FuncCall& func_call) {
            return valexpr_list_accesses_vars(vars, func_call.args);
        },
        [&](const ValExpr::BinOpExpr& bin_op_expr) {
            return valexpr_accesses_vars(vars, bin_op_expr.lhs) || valexpr_accesses_vars(vars, bin_op_expr.rhs);
        },
        [&](const auto&) { return false; }
    }, val_expr->t);
}



bool valexpr_calls_actor_function(InitEnv &env, std::shared_ptr<ValExpr> val_expr) {
    return std::visit(Overload{
        [&](const ValExpr::FuncCall& func_call) {
            assert(env.curr_actor);
            if(env.actor_frontend->member_functions.contains(func_call.func)) {
                // Calling a function of the enclosing actor
                return true;
            }
            for(auto arg: func_call.args) {
                if(valexpr_calls_actor_function(env, arg)) {
                    return true;
                }
            }
            return false;
        },
        [&](const auto&) {
            return predicate_valexpr_walker(
                val_expr, 
                [&](std::shared_ptr<ValExpr> val_expr) {
                    return valexpr_calls_actor_function(env, val_expr);
                });
        }
    }, val_expr->t);
}

bool valexpr_accesses_uninitialized(
    InitEnv &env,
    std::unordered_set<std::string>& unassigned_members, 
    std::shared_ptr<ValExpr> val_expr) {
    // This is a wrapper around [valexpr_accesses_vars] to add the [this] variable when necessary
    assert(unassigned_members.find("this") == unassigned_members.end());
    if(unassigned_members.size() != 0) {
        unassigned_members.insert("this");
    }
    bool result = valexpr_accesses_vars(unassigned_members, val_expr);
    unassigned_members.erase("this");
    if(result) {
        return true;
    }
    if(unassigned_members.size() != 0) {
        if(valexpr_calls_actor_function(env, val_expr)) {
            return true;
        }
    }
    return false;
}


bool valexpr_list_accesses_uninitialized(
    InitEnv &env,
    std::unordered_set<std::string>& vars, 
    const std::vector<std::shared_ptr<ValExpr>>& val_expr_list) {
    for(std::shared_ptr<ValExpr> val_expr: val_expr_list) {
        if (valexpr_accesses_uninitialized(env, vars, val_expr))
        {
            return true;
        }
    }
    return false;
}

std::optional<std::unordered_set<std::string>> new_assigned_var_in_stmt_list(
    InitEnv& env, 
    std::unordered_set<std::string>& unassigned_members, 
    const std::vector<std::shared_ptr<Stmt>>& stmt_list);
std::optional<std::unordered_set<std::string>> new_assigned_variable_in_stmt(
    InitEnv& env, 
    std::unordered_set<std::string>& unassigned_members, 
    std::shared_ptr<Stmt> stmt) {
    // returns the newly assigned variables in the statement. If it encounters a violation, it logs
    // and returns std::nullopt
    std::unordered_set<std::string> new_assigned_var;
    return std::visit(Overload{
        [&](const Stmt::VarDeclWithInit& var_decl_init) -> std::optional<std::unordered_set<std::string>> {
            if(valexpr_accesses_uninitialized(env, unassigned_members, var_decl_init.init)) {
                return std::nullopt;
            }
            return new_assigned_var;
        },
        [&](const Stmt::MemberInitialize& mem_init) -> std::optional<std::unordered_set<std::string>> {
            if(valexpr_accesses_uninitialized(env, unassigned_members, mem_init.init)) {
                return std::nullopt;
            }
            if(unassigned_members.find(mem_init.member_name) != unassigned_members.end()) {
                new_assigned_var.emplace(mem_init.member_name);
            }
            return new_assigned_var;
        },
        [&](const Stmt::BehaviourCall& b) -> std::optional<std::unordered_set<std::string>> {
            // return valexpr_list_accesses_vars(vars, be_call.args);
            if(valexpr_accesses_uninitialized(env, unassigned_members, b.actor) 
            || valexpr_list_accesses_uninitialized(env, unassigned_members, b.args)) {
                return std::nullopt;
            }
            return new_assigned_var;
        },
        [&](const Stmt::Print& print_stmt) -> std::optional<std::unordered_set<std::string>> {
            if(valexpr_accesses_uninitialized(env, unassigned_members, print_stmt.print_expr)) {
                return std::nullopt;
            }
            return new_assigned_var;
        },
        [&](const Stmt::Expr& expr) -> std::optional<std::unordered_set<std::string>> {
            if(valexpr_accesses_uninitialized(env, unassigned_members, expr.expr)) {
                return std::nullopt;
            }
            return new_assigned_var;
        },
        [&](const Stmt::If& if_expr) -> std::optional<std::unordered_set<std::string>> {
            if(valexpr_accesses_uninitialized(env, unassigned_members, if_expr.cond)) {
                return std::nullopt;
            }
            auto then_block_assigned_vars = new_assigned_var_in_stmt_list(env, unassigned_members, if_expr.then_body);
            if(!then_block_assigned_vars) {
                return std::nullopt;
            }
            if(!if_expr.else_body) {
                return new_assigned_var;
            }
            auto else_block_assigned_vars = new_assigned_var_in_stmt_list(env, unassigned_members, *if_expr.else_body);
            if(!else_block_assigned_vars) {
                return std::nullopt;
            }
            // Now need to take the intersection to be conservative
            for(const auto& var: *then_block_assigned_vars) {
                if(else_block_assigned_vars->find(var) != else_block_assigned_vars->end()) {
                    new_assigned_var.emplace(var);
                }
            }
            return new_assigned_var;
        },
        [&](const Stmt::While& while_expr) -> std::optional<std::unordered_set<std::string>> {
            // TODO hard thing need to implement
            if(valexpr_accesses_uninitialized(env, unassigned_members, while_expr.cond)) {
                return std::nullopt;
            }
            return new_assigned_var_in_stmt_list(env, unassigned_members, while_expr.body);
        },
        [&](std::shared_ptr<Stmt::Atomic> atomic_expr) -> std::optional<std::unordered_set<std::string>> {
            return new_assigned_var_in_stmt_list(env, unassigned_members, atomic_expr->body);
        },
        [&](const Stmt::Return& return_expr) -> std::optional<std::unordered_set<std::string>> {
            if(valexpr_accesses_uninitialized(env, unassigned_members, return_expr.expr)) {
                return std::nullopt;
            }
            return new_assigned_var;
        }
    }, stmt->t);
}

std::optional<std::unordered_set<std::string>> new_assigned_var_in_stmt_list(
    InitEnv& env, 
    std::unordered_set<std::string>& unassigned_members, 
    const std::vector<std::shared_ptr<Stmt>>& stmt_list) {
    std::unordered_set<std::string> curr_unassigned_members = unassigned_members;
    std::unordered_set<std::string> newly_assigned_members;
    for(auto stmt: stmt_list) {
        auto new_assigned_var = new_assigned_variable_in_stmt(env, curr_unassigned_members, stmt);
        if(!new_assigned_var) {
            return std::nullopt;
        }
        for(const std::string& var: *new_assigned_var) {
            assert(curr_unassigned_members.find(var) != curr_unassigned_members.end());
            curr_unassigned_members.erase(var);
            newly_assigned_members.emplace(var);
        }
    }
    return newly_assigned_members;
}

bool initialization_check(
    std::shared_ptr<TopLevelItem::Actor> curr_actor,
    std::shared_ptr<ActorFrontend> actor_frontend,
    std::shared_ptr<TopLevelItem::Constructor> constructor_def) {
    assert(curr_actor != nullptr);
    InitEnv init_env(curr_actor, actor_frontend);
    std::unordered_set<std::string> unassigned_members;
    for(const auto& [k, v]: curr_actor->member_vars) {
        assert(unassigned_members.find(k) == unassigned_members.end());
        unassigned_members.insert(k);
    }
    auto assigned_vars = new_assigned_var_in_stmt_list(init_env, unassigned_members, constructor_def->body);
    if(!assigned_vars) {
        return false;
    }
    bool all_assigned = true;
    for(const auto& k: unassigned_members) {
        if(assigned_vars->find(k) == assigned_vars->end()) {
            all_assigned = false;
        }
    }
    return all_assigned;
}