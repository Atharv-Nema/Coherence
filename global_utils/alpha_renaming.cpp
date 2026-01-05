#include "alpha_renaming.hpp"
#include "pattern_matching_boilerplate.hpp"
#include "scoped_store.cpp"
#include "assert.h"
#include <variant>

struct RenameInfo {
    // The renaming is done by appending .<scope_identifier> at the
    // end of the variable name. Every scope has a different scope_identifier
    // and a scope has only one scope_identifier
    size_t scope_identifier = 0;
    ScopedStore<std::string, std::string> rename_map;
    void create_new_scope() {
        scope_identifier++;
        rename_map.create_new_scope();
    }
    void pop_scope() {
        rename_map.pop_scope();
    }
    void rename_variable(std::string& variable) {
        auto renamed_var = rename_map.get_value(variable);
        // Do not rename if the variable is not registered
        if(renamed_var != std::nullopt) {
            variable = *renamed_var;
        }
    }
    std::string register_fresh_name(const std::string& var, FullType type) {
        std::string new_name = var + "." + std::to_string(scope_identifier);
        assert(rename_map.num_scopes() > 0);
        rename_map.insert(var, new_name);
        return new_name;
    }
};


void alpha_rename_val_expr(
    RenameInfo &rename_info,
    std::shared_ptr<ValExpr> val_expr) {
    return std::visit(Overload{
        [&](ValExpr::VVar& var) {
            rename_info.rename_variable(var.name);
        },
        [&](ValExpr::VStruct& vstruct) {
            for(auto [name, val_expr]: vstruct.fields) {
                alpha_rename_val_expr(rename_info, val_expr);
            }
        },
        [&](ValExpr::NewInstance& new_instance) {
            alpha_rename_val_expr(rename_info, new_instance.default_value);
            alpha_rename_val_expr(rename_info, new_instance.size);
        },
        [&](ValExpr::ActorConstruction& actor_construction) {
            for(auto val_expr: actor_construction.args) {
                alpha_rename_val_expr(rename_info, val_expr);
            }
        },
        [&](ValExpr::Consume& consume) {
            rename_info.rename_variable(consume.var_name);
        },
        [&](ValExpr::PointerAccess& pointer_access) {
            alpha_rename_val_expr(rename_info, pointer_access.index);
            alpha_rename_val_expr(rename_info, pointer_access.value);
        },
        [&](ValExpr::Field& field_access) {
            alpha_rename_val_expr(rename_info, field_access.base);
        },
        [&](ValExpr::Assignment& assignemt){
            alpha_rename_val_expr(rename_info, assignemt.lhs);
            alpha_rename_val_expr(rename_info, assignemt.rhs);
        },
        [&](ValExpr::FuncCall& func_call) {
            for(auto val_expr: func_call.args) {
                alpha_rename_val_expr(rename_info, val_expr);
            }
        },
        [&](ValExpr::BinOpExpr& bin_op_expr) {
            alpha_rename_val_expr(rename_info, bin_op_expr.lhs);
            alpha_rename_val_expr(rename_info, bin_op_expr.rhs);
        },
        [&](auto&) {}
    }, val_expr->t);
}

void alpha_rename_stmt_list(
    RenameInfo &rename_info,
    std::vector<std::shared_ptr<Stmt>>& stmt_list);

// The actor variables, [this] and the function parameters are not renamed 
void alpha_rename_statement(
    RenameInfo &rename_info,
    std::shared_ptr<Stmt> stmt) {
    return std::visit(Overload{
        [&](Stmt::VarDeclWithInit &var_decl_init) {
            alpha_rename_val_expr(rename_info, var_decl_init.init);
            var_decl_init.name = 
                rename_info.register_fresh_name(var_decl_init.name, var_decl_init.type);
        },
        [&](Stmt::MemberInitialize &member_initialize) {
            alpha_rename_val_expr(rename_info, member_initialize.init);
        },
        [&](Stmt::BehaviourCall &be_call) {
            alpha_rename_val_expr(rename_info, be_call.actor);
            for(auto val_expr: be_call.args) {
                alpha_rename_val_expr(rename_info, val_expr);
            }
        },
        [&](Stmt::Print &print_stmt) {
            alpha_rename_val_expr(rename_info, print_stmt.print_expr);
        },
        [&](Stmt::Expr &expr) {
            alpha_rename_val_expr(rename_info, expr.expr);
        },
        [&](Stmt::If &if_stmt) {
            alpha_rename_val_expr(rename_info, if_stmt.cond);
            rename_info.create_new_scope();
            alpha_rename_stmt_list(rename_info, if_stmt.then_body);
            rename_info.pop_scope();
            if(if_stmt.else_body) {
                rename_info.create_new_scope();
                alpha_rename_stmt_list(rename_info, *if_stmt.else_body);
                rename_info.pop_scope();
            }
        },
        [&](Stmt::While &while_stmt) {
            alpha_rename_val_expr(rename_info, while_stmt.cond);
            rename_info.create_new_scope();
            alpha_rename_stmt_list(rename_info, while_stmt.body);
            rename_info.pop_scope();
        },
        [&](std::shared_ptr<Stmt::Atomic> atomic) {
            rename_info.create_new_scope();
            alpha_rename_stmt_list(rename_info, atomic->body);
            rename_info.pop_scope();
        },
        [&](Stmt::Return &return_stmt) {
            alpha_rename_val_expr(rename_info, return_stmt.expr);
        }
    }, stmt->t);
}

void alpha_rename_stmt_list(
    RenameInfo &rename_info,
    std::vector<std::shared_ptr<Stmt>>& stmt_list) {
    for(std::shared_ptr<Stmt> stmt: stmt_list) {
        alpha_rename_statement(rename_info, stmt);
    }
}

void alpha_rename_callable_body(
    std::vector<std::shared_ptr<Stmt>> body) {
    RenameInfo rename_info;
    rename_info.create_new_scope();
    alpha_rename_stmt_list(rename_info, body);
}