#include "fill_lock_info.hpp"
#include "utils.hpp"
#include "stage_utils.hpp"
#include "pattern_matching_boilerplate.hpp"
#include "defer.cpp"
#include <assert.h>

struct LockInfoEnv {
    std::shared_ptr<std::unordered_set<std::string>> callable_locks_deref;
    std::shared_ptr<std::unordered_set<std::string>> atomic_section_locks_deref;
    std::shared_ptr<DeclCollection> decl_collection;
    std::shared_ptr<TopLevelItem::Actor> curr_actor;
};

void insert_lock_info(LockInfoEnv& env, std::string lock_name) {
    assert(env.callable_locks_deref != nullptr);
    env.callable_locks_deref->insert(lock_name);
    if(env.atomic_section_locks_deref != nullptr) {
        env.atomic_section_locks_deref->insert(lock_name);
    }
}

void add_valexpr_lock_info(std::shared_ptr<ValExpr> val_expr, LockInfoEnv& env) {
    auto curried = [&](std::shared_ptr<ValExpr> val_expr) {
        add_valexpr_lock_info(val_expr, env);
    };
    // Recursively checking all the subcases
    visitor_valexpr_walker(val_expr, curried);
    assert(env.callable_locks_deref != nullptr);
    std::visit(Overload{
        [&](ValExpr::PointerAccess& pointer_access) {
            auto* pointer_type = std::get_if<FullType::Pointer>(&val_expr->expr_type.t);
            assert(pointer_type != nullptr);
            auto* locked_cap = std::get_if<Cap::Locked>(&pointer_type->cap.t);
            if(locked_cap != nullptr) {
                insert_lock_info(env, locked_cap->lock_name);
            }
        },
        [&](ValExpr::FuncCall& func_call) {
            // Do the recursive check
            std::shared_ptr<TopLevelItem::Func> called_func 
                = get_func_def(func_call.func, env.curr_actor, env.decl_collection);
            if(env.callable_locks_deref == called_func->locks_dereferenced) {
                return;
            }
            for(const std::string& func_lock: *called_func->locks_dereferenced) {
                insert_lock_info(env, func_lock);
            }
        },
        [&](ValExpr::ActorConstruction& actor_construction) {
            std::shared_ptr<ActorFrontend> actor_frontend = 
                env.decl_collection->actor_frontend_map.at(actor_construction.actor_name);
            std::shared_ptr<TopLevelItem::Constructor> constructor_def = 
                actor_frontend->constructors.at(actor_construction.constructor_name);
            if(env.callable_locks_deref == constructor_def->locks_dereferenced) {
                return;
            }
            for(const std::string& constructor_lock: *constructor_def->locks_dereferenced) {
                insert_lock_info(env, constructor_lock);
            }
        },
        [&](const auto&) {}
    }, val_expr->t);
}

void add_stmt_lock_info(std::shared_ptr<Stmt> stmt, LockInfoEnv& env) {
    auto valexpr_visitor = [&](std::shared_ptr<ValExpr> val_expr) {
        return add_valexpr_lock_info(val_expr, env);
    };
    std::function<void(std::shared_ptr<Stmt>)> stmt_visitor;
    stmt_visitor = [&](std::shared_ptr<Stmt> stmt) {
        std::visit(Overload{
            [&](Stmt::Atomic& atomic_stmt) {
                assert(env.atomic_section_locks_deref == nullptr);
                env.atomic_section_locks_deref = atomic_stmt.locks_dereferenced;
                Defer d([&](){env.atomic_section_locks_deref = nullptr;});
                valexpr_visitor_stmt_walker(stmt, valexpr_visitor);
            },
            [&](const auto&) {
                valexpr_and_stmt_visitors_stmt_walker(stmt, valexpr_visitor, stmt_visitor);
            }
        }, stmt->t);
    };
}

void fill_lock_info(
    SyncCallable sync_callable, 
    std::shared_ptr<DeclCollection> decl_collection) {
    LockInfoEnv env{nullptr, nullptr, decl_collection, sync_callable.curr_actor};
    env.callable_locks_deref = get_callable_locks(sync_callable);
    std::vector<std::shared_ptr<Stmt>>& callable_body = std::visit(
        [](const auto& callable)->std::vector<std::shared_ptr<Stmt>>& {
            return callable->body;
        }, sync_callable.callable);
    for(std::shared_ptr<Stmt> stmt: callable_body) {
        add_stmt_lock_info(stmt, env);
    }
}