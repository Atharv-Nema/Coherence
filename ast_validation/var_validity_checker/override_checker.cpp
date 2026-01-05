#include <memory>
#include "scoped_store.cpp"
#include "pattern_matching_boilerplate.hpp"
#include "utils.hpp"
#include <variant>
#include "override_checker.hpp"

struct OverrideEnv {
    std::shared_ptr<TopLevelItem::Actor> curr_actor;
    ScopedStore<std::string, std::monostate> var_scope_info;
};

bool variable_overridable(OverrideEnv& env, const std::string& var_name) {
    // Need this for parameters stuff
    if(var_name == "this") {
        return false;
    }
    if(env.var_scope_info.key_in_curr_scope(var_name)) {
        return false;
    }
    if(env.curr_actor && 
       env.curr_actor->member_vars.contains(var_name)) {
        return false;
    }
    return true;
}

bool override_check_stmt_list(
    OverrideEnv& env,
    std::vector<std::shared_ptr<Stmt>>& stmt_list);

bool override_check_stmt(
    OverrideEnv& env,
    std::shared_ptr<Stmt> stmt) {
    return std::visit(Overload{
        [&](Stmt::VarDeclWithInit& var_decl_with_init) {
            if(!variable_overridable(env, var_decl_with_init.name)) {
                report_error_location(stmt->source_span);
                std::cerr << "Variable " << orig_name(var_decl_with_init.name) << " is not overridable" << std::endl;
                return false;
            }
            return true;
        },
        [&](Stmt::If& if_stmt) {
            {
                ScopeGuard if_scope(env.var_scope_info);
                return override_check_stmt_list(env, if_stmt.then_body);
            }
            if(if_stmt.else_body) {
                ScopeGuard else_scope(env.var_scope_info);
                return override_check_stmt_list(env, *if_stmt.else_body);
            }
            return true;
        },
        [&](Stmt::While& while_stmt) {
            ScopeGuard while_scope(env.var_scope_info);
            return override_check_stmt_list(env, while_stmt.body);
        },
        [&](Stmt::Atomic& atomic_stmt) {
            ScopeGuard atomic_scope(env.var_scope_info);
            return override_check_stmt_list(env, atomic_stmt.body);
        },
        [&](const auto&) {return true;}
    }, stmt->t);
}

bool override_check_stmt_list(
    OverrideEnv& env,
    std::vector<std::shared_ptr<Stmt>>& stmt_list) {
    for(auto stmt: stmt_list) {
        if(!var_validity_check_stmt(env, stmt)) {
            return false;
        }
    }
    return true;
}

bool override_check_callable(
    std::shared_ptr<TopLevelItem::Actor> curr_actor,
    std::vector<TopLevelItem::VarDecl>& params,
    std::vector<std::shared_ptr<Stmt>>& callable_body) {
    OverrideEnv env;
    env.curr_actor = curr_actor;
    env.var_scope_info.create_new_scope();
    // Add arguments to scope
    for(TopLevelItem::VarDecl var_decl: params) {
        // CR: Figure out how to add error location here
        if(!variable_overridable(env, var_decl.name)) {
            std::cerr << "Parameter " << orig_name(var_decl.name) << " not valid" << std::endl;
            return false;
        }
        env.var_scope_info.insert(var_decl.name, std::monostate{});
    }

    // Now, simply iterate through statements
    for(auto stmt: callable_body) {
        if(!var_validity_check_stmt(env, stmt)) {
            return false;
        }
    }
    return true;
}