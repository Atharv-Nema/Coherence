#include "fill_callable_lock_info.hpp"
#include "stage_utils.hpp"
#include <functional>
#include <cassert>
#include "pattern_matching_boilerplate.hpp"
#include "utils.hpp"



void add_stmt_lock_info(
    LockInfoEnv& env,
    std::shared_ptr<Stmt> stmt) {
    std::function<void(std::shared_ptr<ValExpr>)> valexpr_visitor;
    valexpr_visitor = [&](std::shared_ptr<ValExpr> val_expr) {
        visitor_valexpr_walker(val_expr, valexpr_visitor);
        std::visit(Overload{
            [&](const ValExpr::FuncCall& func_call) {
                std::shared_ptr<TopLevelItem::Func> called_func 
                    = get_func_def(func_call.func, env.curr_actor, env.decl_collection);
                if(env.locks_dereferenced == called_func->locks_dereferenced) {
                    return;
                }
                for(const std::string& func_lock: *called_func->locks_dereferenced) {
                    env.locks_dereferenced->insert(func_lock);
                }
            },
            [&](const auto&){}
        }, val_expr->t);
    };
    valexpr_visitor_stmt_walker(stmt, valexpr_visitor);
}


void fill_callable_lock_info(
    SyncCallable sync_callable,
    std::shared_ptr<DeclCollection> decl_collection) {
    LockInfoEnv env;
    auto locks_deref = get_callable_locks(sync_callable);
    env.locks_dereferenced = locks_deref;
    env.curr_actor = sync_callable.curr_actor;
    env.decl_collection = decl_collection;
    std::vector<std::shared_ptr<Stmt>>& callable_body = std::visit(
        [](const auto& callable)->std::vector<std::shared_ptr<Stmt>>& {
            return callable->body;
        }, sync_callable.callable);
    for(std::shared_ptr<Stmt> stmt: callable_body) {
        add_stmt_lock_info(env, stmt);
    }
}

void fill_all_callable_lock_info(
    std::shared_ptr<CallableGraph> callable_graph,
    std::shared_ptr<DeclCollection> decl_collection) {
    std::unordered_set<SyncCallable> visited;
    std::function<void(SyncCallable)> dfs;
    dfs = [&](SyncCallable sync_callable) {
        assert(!visited.contains(sync_callable));
        visited.insert(sync_callable);
        for(auto neighbour: callable_graph->at(sync_callable)) {
            if(!visited.contains(neighbour)) {
                dfs(neighbour);
            }
        }
        fill_callable_lock_info(sync_callable, decl_collection);
    };
    for(auto& [sync_callable, _]: *callable_graph) {
        if(!visited.contains(sync_callable)) {
            dfs(sync_callable);
        }
    }
}