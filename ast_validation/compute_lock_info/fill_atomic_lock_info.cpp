#include "fill_atomic_lock_info.hpp"
#include "stage_structs.hpp"
#include "pattern_matching_boilerplate.hpp"
#include <cassert>
#include "defer.cpp"
#include "utils.hpp"

void fill_stmt_info(
    std::shared_ptr<Stmt> stmt,
    LockInfoEnv& env) {
    std::function<void(std::shared_ptr<ValExpr>)> valexpr_visitor;
    valexpr_visitor = [&](std::shared_ptr<ValExpr> val_expr) {
        visitor_valexpr_walker(val_expr, valexpr_visitor);
        std::visit(Overload{
            [&](const ValExpr::PointerAccess& pointer_access) {
                auto* pointer_type = std::get_if<FullType::Pointer>(&pointer_access.value->expr_type.t);
                assert(pointer_type != nullptr);
                auto* locked_cap = std::get_if<Cap::Locked>(&pointer_type->cap.t);
                if(locked_cap != nullptr) {
                    assert(env.locks_dereferenced != nullptr);
                    env.locks_dereferenced->insert(locked_cap->lock_name);
                }
            },
            [&](const auto&) {}
        }, val_expr->t);
    };
    // Can do this directly with val_expr
    auto stmt_visitor = [&](std::shared_ptr<Stmt> stmt) {
        fill_stmt_info(stmt, env);
    };
    std::visit(Overload{
        [&](std::shared_ptr<Stmt::Atomic> atomic_stmt) {
            assert(env.locks_dereferenced == nullptr);
            env.locks_dereferenced = atomic_stmt->locks_dereferenced;
            Defer d([&](){env.locks_dereferenced = nullptr;});
            valexpr_visitor_stmt_walker(stmt, valexpr_visitor);
        },
        [&](const auto&) {
            valexpr_and_stmt_visitors_stmt_walker(stmt, valexpr_visitor, stmt_visitor);
        }
    }, stmt->t);
}

void fill_stmt_list_info(
    std::vector<std::shared_ptr<Stmt>>& stmt_list,
    LockInfoEnv& env) {
    for(std::shared_ptr<Stmt> stmt: stmt_list) {
        fill_stmt_info(stmt, env);
    }
}

void fill_atomic_lock_info(
    Program* root,
    std::shared_ptr<DeclCollection> decl_collection) {
    LockInfoEnv env;
    env.curr_actor = nullptr;
    env.decl_collection = decl_collection;
    env.locks_dereferenced = nullptr;
    for(TopLevelItem& toplevel_item: root->top_level_items) {
        std::visit(Overload{
            [&](const TopLevelItem::TypeDef&){},
            [&](std::shared_ptr<TopLevelItem::Func> func_def) {
                assert(env.curr_actor == nullptr);
                fill_stmt_list_info(func_def->body, env);
            },
            [&](std::shared_ptr<TopLevelItem::Actor> actor_def) {
                env.curr_actor = actor_def;
                Defer d([&]{env.curr_actor = nullptr;});
                for(auto actor_mem: actor_def->actor_members) {
                    std::visit(
                        [&](const auto& mem) {
                            fill_stmt_list_info(mem->body, env);
                        }, actor_mem);
                }
            }
        }, toplevel_item.t);
    }
}