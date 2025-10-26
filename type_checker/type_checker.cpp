// For typing a value expression
#pragma once
#include "utils.hpp"



std::optional<FullType> val_expr_type(TypeEnv& env, std::shared_ptr<ValExpr> val_expr) {
    return std::visit(Overload{
        // ---------- Literal values ----------
        [](const ValExpr::VUnit&) {
            return FullType{ BasicType{ BasicType::TUnit{} } };
        },
        [](const ValExpr::VInt&) {
            return FullType{ BasicType{ BasicType::TInt{} } };
        },
        [](const ValExpr::VFloat&) {
            return FullType{ BasicType{ BasicType::TFloat{} } };
        },
        [](const ValExpr::VBool&) {
            return FullType{ BasicType{ BasicType::TBool{} } };
        },

        // ---------- Variable lookup ----------
        [&](const ValExpr::VVar& v) -> std::optional<FullType> {
            auto it = env.var_context.find(v.name);
            if (it == env.var_context.end())
                return std::nullopt;
            return it->second;
        },
        [&](const ValExpr::VStruct& struct) {
            
        },
        
        // Allocations
        [&](const ValExpr::NewInstance& new_instance) -> std::optional<FullType> {
            auto default_type = val_expr_type(env, new_instance.default_value);
            if(default_type == std::nullopt) {
                return std::nullopt;
            }
            auto raw_type = std::get_if<BasicType>(&default_type->t);
            if(raw_type == nullptr) {
                return std::nullopt;
            }
            if(basic_type_equal(env.type_context, raw_type->base, new_instance.type)) {
                return FullType {FullType::Pointer {new_instance.type, new_instance.cap}};
            }
            return std::nullopt;
        },

        [&](const ValExpr::Array& array) -> std::optional<FullType> {
            if(array.size < 0) {
                return std::nullopt;
            }
            return val_expr_type(env, ValExpr {ValExpr::NewInstance {array.element_info}});
        },
        [&](const ValExpr::ActorConstruction& actor_construction) -> std::optional<FullType> {
            if(env.actor_name_map.find(actor_construction.actor_name) == env.actor_name_map.end()) {
                return std::nullopt;
            }
            auto actor_def = env.actor_name_map[actor_construction.actor_name];
            for(int i = 0; i < actor_def->constructors.size(); i++) {
                // do stuff

            }
            for(const auto& constructors: actor_def->constructors) {
                if(constructors.name == actor_def->name) {
                    if(passed_in_parameters_valid(env, constructors.params, actor_construction.args)) {
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

        [&](const ValExpr::ArrayAccess& array_access) -> std::optional<FullType> {
            if(array_access.index < 0) {
                return std::nullopt;
            }
            // CR: Implement the mutual recursion and split this common code out
            auto internal_type = val_expr_type(env, array_access.value);
            if(!internal_type) {
                return std::nullopt;
            }
            auto deref_type = dereferenced_type(env, *internal_type);
            if(!deref_type) {
                return std::nullopt;
            }
            return FullType{ *deref_type };
        },

        [&](const ValExpr::Deref& dereference) -> std::optional<FullType> {
            auto internal_type = val_expr_type(env, dereference.inner);
            if(!internal_type) {
                return std::nullopt;
            }
            auto deref_type = dereferenced_type(env, *internal_type);
            if(!deref_type) {
                return std::nullopt;
            }
            return FullType{ *deref_type };
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
                return rhs_type;
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
            
            if(passed_in_parameters_valid(env, func_def->params, f.args)) {
                return func_def->return_type;
            }
            return std::nullopt;
        },

        [&](const ValExpr::BeCall& b) -> std::optional<FullType> {
            // Lookup function definition in the type context
            auto actor_type = val_expr_type(env, b.actor);
            if(!actor_type) { return std::nullopt; }
            auto named_actor = std::get_if<BasicType::TNamed>(&actor_type->t);
            if(!named_actor) { return std::nullopt; }

            assert(env.actor_name_map.find(named_actor->name) != env.actor_name_map.end());
            auto actor_def = env.actor_name_map[named_actor->name];
            for(const auto& behaviour: actor_def->member_behaviours) {
                if(behaviour.name == b.behaviour_name) {
                    if(passed_in_parameters_valid(env, behaviour.params, b.args)) {
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
        },

        [&](const ValExpr::ConsumeE& consume) -> std::optional<FullType> {
            // Need to check that it is a consumable expression
        }

    }, *val_expr);
}




