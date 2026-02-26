#include "stage_utils.hpp"
#include "pattern_matching_boilerplate.hpp"
#include "utils.hpp"
#include <cassert>
#include "ast_walkers.hpp"

std::shared_ptr<std::unordered_set<std::string>> get_callable_locks(SyncCallable sync_callable) {
    return std::visit(
        [](const auto& callable) {
            return callable->locks_dereferenced;
        }, sync_callable.callable);
}

void set_callable_locks(
    SyncCallable sync_callable,
    std::shared_ptr<std::unordered_set<std::string>> locks_dereferenced) {
    std::visit(
        [&](const auto& callable) {
            callable->locks_dereferenced = locks_dereferenced;
        }, sync_callable.callable);
}

void add_valexpr_lock_info(std::shared_ptr<ValExpr> val_expr, LockInfoEnv& env) {
    auto curried = [&](std::shared_ptr<ValExpr> val_expr) {
        add_valexpr_lock_info(val_expr, env);
    };
    visitor_valexpr_walker(val_expr, curried);
    std::visit(Overload{
        [&](const ValExpr::FuncCall& func_call) {
            std::shared_ptr<TopLevelItem::Func> called_func 
                = get_func_def(func_call.func, env.curr_actor, env.decl_collection);
            if(env.locks_dereferenced == called_func->locks_dereferenced) {
                // May be the case when a function is running for eg
                return;
            }
            if(env.locks_dereferenced == nullptr) {
                // May be the case when filling atomic sections
                return;
            }
            for(const std::string& func_lock: *called_func->locks_dereferenced) {
                env.locks_dereferenced->insert(func_lock);
            }
        },
        [&](const ValExpr::PointerAccess& pointer_access) {
            auto* pointer_type = std::get_if<Type::Pointer>(&pointer_access.value->expr_type->t);
            assert(pointer_type != nullptr);
            auto* locked_cap = std::get_if<Cap::Locked>(&pointer_type->cap.t);
            if(locked_cap != nullptr) {
                assert(env.locks_dereferenced != nullptr);
                env.locks_dereferenced->insert(locked_cap->lock_name);
            }
        },
        [&](const auto&){}
    }, val_expr->t);
}