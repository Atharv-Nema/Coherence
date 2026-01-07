#include "callable_graph_builder.hpp"
#include "pattern_matching_boilerplate.hpp"
#include "utils.hpp"
#include <assert.h>
#include "defer.cpp"

struct GraphBuilderEnv {
    std::shared_ptr<DeclCollection> decl_collection;
    std::shared_ptr<TopLevelItem::Actor> curr_actor;
    std::shared_ptr<CallableGraph> graph;
};

void add_valexpr_callable_neighbours(
    SyncCallable sync_callable, 
    GraphBuilderEnv &env,
    std::shared_ptr<ValExpr> val_expr) {
    auto curried = [&](std::shared_ptr<ValExpr> val_expr) {
        add_valexpr_callable_neighbours(sync_callable, env, val_expr);
    };
    std::visit(Overload{
        [&](ValExpr::ActorConstruction& actor_construction) {
            std::shared_ptr<ActorFrontend> actor_frontend = 
                env.decl_collection->actor_frontend_map.at(actor_construction.actor_name);
            std::shared_ptr<TopLevelItem::Constructor> constructor_def =
                actor_frontend->constructors.at(actor_construction.constructor_name);
            // Finding the actor definition pointer. This is pretty bad code. However, it is the simplest way
            // to find the actor definition
            std::shared_ptr<TopLevelItem::Actor> actor_def = 
                env.graph->find(SyncCallable{constructor_def, nullptr})->first.curr_actor;
            assert(actor_def != nullptr);
            env.graph->at(sync_callable).insert(SyncCallable{constructor_def, actor_def});
            visitor_valexpr_walker(val_expr, curried);
        },
        [&](ValExpr::FuncCall& func_call) {
            std::shared_ptr<TopLevelItem::Func> func_def = 
                get_func_def(func_call.func, env.curr_actor, env.decl_collection);
            assert(func_def != nullptr);
            std::shared_ptr<TopLevelItem::Actor> actor_def = 
                env.graph->find(SyncCallable{func_def, nullptr})->first.curr_actor;
            env.graph->at(sync_callable).insert(SyncCallable{func_def, actor_def});
            visitor_valexpr_walker(val_expr, curried);
        },
        [&](const auto&) {
            visitor_valexpr_walker(val_expr, curried);
        }
    }, val_expr->t);
}

void add_callable_neighbours(CallableVariant callable, GraphBuilderEnv &env) {
    SyncCallable sync_callable{callable, env.curr_actor};
    env.graph->try_emplace(sync_callable);
    std::vector<std::shared_ptr<Stmt>>& callable_body = std::visit(
        [](const auto& callable)->std::vector<std::shared_ptr<Stmt>>& {
            return callable->body;
        }, sync_callable.callable);
    auto curried_add_valexpr = [&](std::shared_ptr<ValExpr> val_expr) {
        add_valexpr_callable_neighbours(sync_callable, env, val_expr);
    };
    for(std::shared_ptr<Stmt> stmt: callable_body) {
        valexpr_visitor_stmt_walker(stmt, curried_add_valexpr);
    }
}

static void prepopulate_callable_graph(
    Program* root,
    std::shared_ptr<CallableGraph> graph) {
    for (TopLevelItem& item : root->top_level_items) {
        std::visit(Overload{
            [&](std::shared_ptr<TopLevelItem::Func> func_def) {
                graph->try_emplace(SyncCallable{func_def, nullptr});
            },
            [&](std::shared_ptr<TopLevelItem::Actor> actor_def) {
                for (auto& member : actor_def->actor_members) {
                    std::visit(Overload{
                        [&](std::shared_ptr<TopLevelItem::Behaviour>) {},
                        [&](auto callable_def) {
                            graph->try_emplace(SyncCallable{callable_def, actor_def});
                        }
                    }, member);
                }
            },
            [&](TopLevelItem::TypeDef&) {}
        }, item.t);
    }
}

std::shared_ptr<CallableGraph> build_graph(
    Program* root, 
    std::shared_ptr<DeclCollection> decl_collection) {
    GraphBuilderEnv env{decl_collection, nullptr, std::make_shared<CallableGraph>()};
    prepopulate_callable_graph(root, env.graph);
    for(TopLevelItem& item: root->top_level_items) {
        std::visit(Overload{
            [&](std::shared_ptr<TopLevelItem::Func> func_def) {
                add_callable_neighbours(func_def, env);
            },
            [&](std::shared_ptr<TopLevelItem::Actor> actor_def) {
                env.curr_actor = actor_def;
                Defer d([&](){env.curr_actor = nullptr;});
                for(auto actor_mem: actor_def->actor_members) {
                    std::visit(Overload{
                        [&](std::shared_ptr<TopLevelItem::Behaviour> be) {},
                        [&](auto callable) {
                            add_callable_neighbours(callable, env);
                        }
                    }, actor_mem);
                }
            },
            [&](TopLevelItem::TypeDef&) {}
        }, item.t);
    }
    return env.graph;
}