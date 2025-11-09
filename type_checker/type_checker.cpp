// For typing a value expression
#include "utils.hpp"
#include <algorithm>


std::optional<FullType> val_expr_type(TypeEnv& env, std::shared_ptr<ValExpr> val_expr) {
    return std::visit(Overload{
        // Literal values
        [](const ValExpr::VUnit&) {
            return std::optional(FullType{ BasicType{ BasicType::TUnit{} } });
        },
        [](const ValExpr::VInt&) {
            return std::optional(FullType{ BasicType{ BasicType::TInt{} } });
        },
        [](const ValExpr::VFloat&) {
            return std::optional(FullType{ BasicType{ BasicType::TFloat{} } });
        },
        [](const ValExpr::VBool&) {
            return std::optional(FullType{ BasicType{ BasicType::TBool{} } });
        },

        // Variable lookup
        [&](const ValExpr::VVar& v) -> std::optional<FullType> {
            std::optional<FullType> type = env.var_context.get_variable_type(v.name);
            if(!type) {
                return std::nullopt;
            }
            return *type;
        },
        // I am not taking in a const as I would want to mutate this
        [&](ValExpr::VStruct& struct_expr) -> std::optional<FullType> {
            std::optional<NameableType::Struct> struct_contents = get_struct_type(env, struct_expr.type_name);
            if(!struct_contents) {
                return std::nullopt;
            }
            if(struct_valid(env, *struct_contents, struct_expr)) {
                std::optional<BasicType> basic_type = standardize_type(env.type_context, struct_expr.type_name);
                assert(basic_type);
                return (FullType { *basic_type });
            }
            else {
                return std::nullopt;
            }
        },
        
        // Allocations
        [&](const ValExpr::NewInstance& new_instance) -> std::optional<FullType> {
            // auto ind_typ
            auto size_type = val_expr_type(env, new_instance.size);
            if(!size_type) {
                return std::nullopt;
            }
            auto default_type = val_expr_type(env, new_instance.default_value);
            if(default_type == std::nullopt) {
                return std::nullopt;
            }
            auto basic_type = std::get_if<BasicType>(&default_type->t);
            if(!basic_type) {
                return std::nullopt;
            }
            if(basic_type_equal(env.type_context, *basic_type, new_instance.type)) {
                return FullType {FullType::Pointer {new_instance.type, new_instance.cap}};
            }
            return std::nullopt;
        },

        [&](const ValExpr::ActorConstruction& actor_construction) -> std::optional<FullType> {
            if(env.actor_name_map.find(actor_construction.actor_name) == env.actor_name_map.end()) {
                return std::nullopt;
            }
            auto actor_def = env.actor_name_map[actor_construction.actor_name];
            for(const auto& constructor: actor_def->constructors) {
                if(constructor->name == actor_construction.constructor_name) {
                    if(passed_in_parameters_valid(env, constructor->params, actor_construction.args)) {
                        return FullType {BasicType {BasicType::TActor {actor_def->name}}};
                    }
                    else {
                        return std::nullopt;
                    }
                }
            }
            return std::nullopt;
        },

        // Accesses

        [&](const ValExpr::PointerAccess& pointer_access) -> std::optional<FullType> {
            // CR: Implement the mutual recursion and split this common code out
            auto index_type = val_expr_type(env, pointer_access.index);
            if(!index_type) {
                return std::nullopt;
            }
            if(!full_type_equal(env.type_context, *index_type, FullType { BasicType {BasicType::TInt {}}})) {
                return std::nullopt;
            }
            auto internal_type = val_expr_type(env, pointer_access.value);
            if(!internal_type) {
                return std::nullopt;
            }
            auto* pointer_type = std::get_if<FullType::Pointer>(&internal_type->t);
            if(pointer_type == nullptr) {
                return std::nullopt;
            }
            if(auto* locked_cap = std::get_if<Cap::Locked>(&pointer_type->cap.t)) {
                if(!env.var_context.in_atomic_section()) {
                    return std::nullopt;
                }
                env.var_context.add_lock(locked_cap->lock_name);
            }
            return FullType {pointer_type->base};
        },

        [&](const ValExpr::Field& field_access) -> std::optional<FullType> {
            std::optional<FullType> struct_type = val_expr_type(env, field_access.base); 
            if(!struct_type) {
                return std::nullopt;
            }
            auto* basic_type = std::get_if<BasicType>(&struct_type->t);
            if(!basic_type) {
                return std::nullopt;
            }
            auto* named_type = std::get_if<BasicType::TNamed>(&basic_type->t);
            auto struct_contents = get_struct_type(env, named_type->name);
            if(!struct_contents) {
                return std::nullopt;
            }
            for(const auto& [field_name, field_basic_type]: struct_contents->members) {
                if(field_name == field_access.field) {
                    return FullType {field_basic_type};
                }
            }
            return std::nullopt;
        },

        // Assignments
        [&](const ValExpr::Assignment& assignment) -> std::optional<FullType> {
            auto lhs_type = val_expr_type(env, assignment.lhs);
            if(!lhs_type) { return std::nullopt; }
            auto rhs_type = val_expr_type(env, assignment.rhs);
            if(!rhs_type) { return std::nullopt; }
            if(!type_assignable(env.type_context, *lhs_type, *rhs_type)) {
                return std::nullopt;
            }
            // Now just need to check whether the thingy is actually assignable scene
            if(can_appear_in_lhs(env, assignment.lhs)) {
                return unaliased_type(env, *lhs_type);
            }
            else {
                return std::nullopt;
            }
        },

        // Callables
        [&](const ValExpr::FuncCall& f) -> std::optional<FullType> {
            // Lookup function definition in the type context
            auto it = env.func_name_map.find(f.func);
            if(it == env.func_name_map.end()) {
                return std::nullopt;
            }
            auto func_def = it->second;
            if(func_def->locks_dereferenced.size() != 0) {
                if(!env.var_context.in_atomic_section()) {
                    return std::nullopt;
                }
                for(const std::string& s: func_def->locks_dereferenced) {
                    env.var_context.add_lock(s);
                }
            }
            if(passed_in_parameters_valid(env, func_def->params, f.args, false)) {
                return func_def->return_type;
            }
            return std::nullopt;
        },

        [&](const ValExpr::BeCall& b) -> std::optional<FullType> {
            // Lookup function definition in the type context
            auto actor_type = val_expr_type(env, b.actor);
            if(!actor_type) { return std::nullopt; }
            auto basic_type = std::get_if<BasicType>(&actor_type->t);
            if(!basic_type) {
                return std::nullopt;
            }
            auto named_actor = std::get_if<BasicType::TNamed>(&basic_type->t);
            if(!named_actor) { return std::nullopt; }

            assert(env.actor_name_map.find(named_actor->name) != env.actor_name_map.end());
            auto actor_def = env.actor_name_map[named_actor->name];
            for(auto behaviour: actor_def->member_behaviours) {
                if(behaviour->name == b.behaviour_name) {
                    if(passed_in_parameters_valid(env, behaviour->params, b.args, true)) {
                        return FullType { BasicType {BasicType::TUnit {}}};
                    }
                    else {
                        return std::nullopt;
                    }
                }
            }
            return std::nullopt;
        },

        // Binary operation
        [&](const ValExpr::BinOpExpr& e) -> std::optional<FullType> {
            std::optional<FullType> lhs_t = val_expr_type(env, e.lhs);
            std::optional<FullType> rhs_t = val_expr_type(env, e.rhs);
            BasicType lhs_base = extract_basic_type(*lhs_t);
            BasicType rhs_base = extract_basic_type(*rhs_t);
            if(!(lhs_t && rhs_t)) {
                return std::nullopt;
            }
            switch(e.op) {
                case BinOp::Eq:
                case BinOp::Neq:
                    if(type_is_numeric(env.type_context, *lhs_t) && type_is_numeric(env.type_context, *rhs_t)) {
                        return FullType {BasicType { BasicType::TBool {}}};
                    }
                    
                    if(!basic_type_equal(env.type_context, lhs_base, rhs_base)) {
                        return std::nullopt;
                    }
                    if(auto* named_type = std::get_if<BasicType::TNamed>(&lhs_base.t)) {
                        auto standard_type = standardize_type(env.type_context, named_type->name);
                        assert(standard_type);
                        if(std::get_if<BasicType::TNamed>(&standard_type->t)) {
                            return std::nullopt;
                        }
                    }
                    return FullType {BasicType {BasicType::TBool {}}};
                case BinOp::Add:
                case BinOp::Sub:
                case BinOp::Mul:
                case BinOp::Div:
                    if(!(type_is_numeric(env.type_context, *lhs_t) && type_is_numeric(env.type_context, *rhs_t))) {
                        return std::nullopt;
                    }
                    if(basic_type_equal(env.type_context, lhs_base, BasicType { BasicType::TFloat {}}) ||
                        basic_type_equal(env.type_context, rhs_base, BasicType { BasicType::TFloat {}})) {
                        return FullType {BasicType { BasicType::TFloat {}}};
                    }
                    else {
                        return FullType {BasicType {BasicType::TInt {}}};
                    }
                case BinOp::Geq:
                case BinOp::Leq:
                case BinOp::Gt:
                case BinOp::Lt:
                    if(type_is_numeric(env.type_context, *lhs_t) && type_is_numeric(env.type_context, *rhs_t)) {
                        return FullType {BasicType {BasicType::TBool {}}};
                    }
                    else {
                        return std::nullopt;
                    }
            }
            return std::nullopt;
        }

    }, val_expr->t);
}

bool type_check_statement(TypeEnv& env, std::shared_ptr<Stmt> stmt) {
    return std::visit(Overload{
        [&](const Stmt::VarDeclWithInit& var_decl_with_init) {
            if(env.var_context.variable_already_defined_in_scope(var_decl_with_init.name)) {
                return false;
            }
            env.var_context.insert_variable(var_decl_with_init.name, var_decl_with_init.type);
            return true;
        },
        [&](const Stmt::MemberInitialize& member_init) {
            if(env.var_context.get_local_member(member_init.member_name)) {
                return false;
            }
            auto expected_type = env.var_context.get_actor_member(member_init.member_name);
            if(!expected_type) {
                return false;
            }
            auto present_type = val_expr_type(env, member_init.init);
            if(!present_type) {
                return false;
            }
            return full_type_equal(env.type_context, *expected_type, *present_type);
        },
        [&](const Stmt::Expr& val_expr) {
            auto type = val_expr_type(env, val_expr.expr);
            return type != std::nullopt;
        },
        [&](const Stmt::If& if_expr) {
            auto cond_type = val_expr_type(env, if_expr.cond);
            if(!cond_type) {
                return false;
            }
            if(!full_type_equal(env.type_context, FullType {BasicType {BasicType::TBool{}}}, *cond_type)) {
                return false;
            }
            // Creating a scope for the if block
            env.var_context.create_new_scope();
            bool if_block_type_check = type_check_stmt_list(env, if_expr.then_body);
            env.var_context.pop_scope();
            env.var_context.create_new_scope();
            bool else_block_type_check = if_expr.else_body ? type_check_stmt_list(env, *if_expr.else_body) : true;
            env.var_context.pop_scope();
            return if_block_type_check && else_block_type_check;
        },
        [&](const Stmt::While& while_expr) {
            // creating scope for the cond
            auto cond_type = val_expr_type(env, while_expr.cond);
            if(!cond_type) {
                return false;
            }
            if(!full_type_equal(env.type_context, FullType {BasicType {BasicType::TBool{}}}, *cond_type)) {
                return false;
            }
            // Creating a scope for the body
            env.var_context.create_new_scope();
            bool body_valid = type_check_stmt_list(env, while_expr.body);
            env.var_context.pop_scope();
            return body_valid;
        },
        [&](std::shared_ptr<Stmt::Atomic> atomic_block) {
            if(!env.var_context.in_atomic_section()) {
                env.var_context.create_new_scope(atomic_block);
            }
            else {
                env.var_context.create_new_scope();
            }
            bool body_valid = type_check_stmt_list(env, atomic_block->body);
            env.var_context.pop_scope();
            return body_valid;
        },
        [&](const Stmt::Return& return_stmt) {
            auto return_expr_type = val_expr_type(env, return_stmt.expr);
            if(!return_expr_type) {
                return false;
            }
            auto expected_return_type = env.var_context.get_expected_function_return_type();
            if(!expected_return_type) {
                return false;
            }
            if(!full_type_equal(env.type_context, *return_expr_type, *expected_return_type)) {
                return false;
            }
            return true;
        }
    }, stmt->t);
}


bool type_check_toplevel_item(TypeEnv& env, std::shared_ptr<TopLevelItem> toplevel_item) {
    return std::visit(Overload{
        [&](const TopLevelItem::TypeDef& type_def) {
            if(env.type_context.find(type_def.type_name) != env.type_context.end()) {
                return false;
            }
            env.type_context.emplace(type_def.type_name, type_def.nameable_type);
            return true;
        },
        [&](std::shared_ptr<TopLevelItem::Func> func_def) {
            return type_check_function(env, func_def);
        },
        [&](std::shared_ptr<TopLevelItem::Actor> actor_def) {
            // Checking that the fields are unique
            if(env.actor_name_map.find(actor_def->name) != env.actor_name_map.end()) {
                return false;
            }
            // Add the current actor to the scope
            env.actor_name_map[actor_def->name] = actor_def;
            
            // First type-check the functions (simple)
            // Creating a scope for the functions within the actor
            ScopeGuard actor_func_scope(env.func_name_map);
            for(auto mem_func: actor_def->member_funcs) {
                if(!type_check_function(env, mem_func)) {
                    return false;
                }
            }

            // Now type checking the behaviours
            for(auto mem_behaviour: actor_def->member_behaviours) {
                if(!type_check_behaviour(env, mem_behaviour)) {
                    return false;
                }
            }

            // Type checking constructors
            for(auto constructor: actor_def->constructors) {
                if(!type_check_constructor(env, constructor)) {
                    return false;
                }
            }
            return true;
        }
    }, toplevel_item->t);
}

bool type_check_program(std::shared_ptr<Program> root) {

}


