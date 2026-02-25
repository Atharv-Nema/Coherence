#include <assert.h>
#include <iostream>
#include <functional>
#include "core_type_checker.hpp"
#include <iostream>
#include "pattern_matching_boilerplate.hpp"
#include "alpha_renaming.hpp"
#include "defer.cpp"
#include "utils.hpp"
#include "type_checker_utils.hpp"


struct CoreEnv {
    TypeEnv& type_env;
    std::shared_ptr<TopLevelItem::Func> curr_func;
    std::unordered_map<std::string, std::shared_ptr<const Type>> local_var_context;
    int atomic_nest_level = 0;
    CoreEnv(
        TypeEnv& type_env,
        std::shared_ptr<TopLevelItem::Func> curr_func): type_env(type_env) {
        this->curr_func = curr_func;
    }
};




std::shared_ptr<const Type> val_expr_type(CoreEnv& env, std::shared_ptr<ValExpr> val_expr);

// CR: Think carefully about including mutability here
bool can_appear_in_lhs(CoreEnv& env, std::shared_ptr<ValExpr> expr) {
    // This assumes that expr is well-typed
    return std::visit(Overload{
        [](const ValExpr::VVar&) {return true;},
        [&](const ValExpr::PointerAccess& pointer_access) {
            // First, need to check whether the internal pointer of array_access is of an assignable type
            auto arr_type = val_expr_type(env, pointer_access.value);
            assert(arr_type);
            auto* ptr_type = std::get_if<Type::Pointer>(&arr_type->t);
            assert(ptr_type != nullptr);
            Cap effective_ptr_cap = viewpoint_adaptation_op(arr_type->viewpoint, ptr_type->cap).value();
            if(!capability_mutable(effective_ptr_cap)) {
                return false;
            }
            return can_appear_in_lhs(env, pointer_access.value);
        },
        [&](const ValExpr::Field& field) {
            return can_appear_in_lhs(env, field.base);
        },
        [](const auto&) {return false;}
    }, expr->t);
}








std::shared_ptr<const Type> get_actor_member_type(CoreEnv& env, const std::string& var_name) {
    if(!env.type_env.curr_actor) {
        return nullptr;
    }
    if(var_name == "this") {
        return std::make_shared<Type>(Type{Type::TActor{env.type_env.curr_actor->name}, std::nullopt});
    }
    if(env.type_env.curr_actor->member_vars.find(var_name) ==
        env.type_env.curr_actor->member_vars.end()) {
        return nullptr;
    }
    return env.type_env.curr_actor->member_vars[var_name];
}

std::shared_ptr<const Type> get_variable_type(CoreEnv& env, const std::string& var_name) {
    if(env.local_var_context.find(var_name) != env.local_var_context.end()) {
        return env.local_var_context.at(var_name);
    }
    return get_actor_member_type(env, var_name);
}


bool check_type_expr_list_assignable(
    CoreEnv& env,
    const std::vector<std::shared_ptr<const Type>>& expected_types,
    const std::vector<std::shared_ptr<ValExpr>>& val_expr_list) {
    if (expected_types.size() != val_expr_list.size()) {
        return false;
    }
    for(size_t i = 0; i < expected_types.size(); i++) {
        std::shared_ptr<const Type> arg_type = val_expr_type(env, val_expr_list[i]);
        if(!arg_type) {
            return false;
        }
        const std::shared_ptr<const Type>& expected_type = expected_types[i];

        if (!type_assignable(env.type_env.type_context, expected_type, arg_type)) {
            return false;
        }
    }
    return true;
}

// CR: Need to improve the API here. A boolean flag is pretty bad code
bool passed_in_parameters_valid(
    CoreEnv& env,
    const std::vector<TopLevelItem::VarDecl>& signature,
    const std::vector<std::shared_ptr<ValExpr>>& arguments,
    bool parameters_being_sent) {
    std::vector<std::shared_ptr<const Type>> expected_types;
    expected_types.reserve(signature.size());
    for(const auto& var_decl: signature) {
        expected_types.push_back(var_decl.type);
    }
    return check_type_expr_list_assignable(env, expected_types, arguments);
}



bool struct_valid(
    CoreEnv &env, 
    std::optional<Cap> viewpoint, 
    NameableType::Struct& struct_contents, 
    ValExpr::VStruct& struct_expr) {
    // Does not perform logging
    std::sort(struct_contents.members.begin(), struct_contents.members.end(),
    [](const auto& a, const auto& b) { return a.first < b.first; });
    std::sort(struct_expr.fields.begin(), struct_expr.fields.end(),
    [](const auto& a, const auto& b) { return a.first < b.first; });
    if (struct_contents.members.size() != struct_expr.fields.size()) {
        return false;
    }
    std::vector<std::shared_ptr<const Type>> expected_types;
    std::vector<std::shared_ptr<ValExpr>> struct_members;
    size_t size = struct_contents.members.size();
    expected_types.reserve(size);
    struct_members.reserve(size);
    for(int i = 0; i < size; i++) {
        if(struct_contents.members[i].first != struct_expr.fields[i].first) {
            return false;
        }
        // Viewpoint adapt the members of the defined struct
        expected_types.emplace_back(apply_viewpoint_to_type(viewpoint, struct_contents.members[i].second));
        struct_members.emplace_back(struct_expr.fields[i].second);
    }
    return check_type_expr_list_assignable(env, expected_types, struct_members);
}

std::shared_ptr<const Type> val_expr_type(CoreEnv& env, std::shared_ptr<ValExpr> val_expr) {
    std::shared_ptr<const Type> full_type = std::visit(Overload{
        // Literal values
        [&](const ValExpr::VUnit&) {
            return std::make_shared<const Type>(Type{Type::TUnit{}, std::nullopt});
        },
        [](const ValExpr::VNullptr) {
            return std::make_shared<const Type>(Type{Type::TNullptr{}, std::nullopt});
        },
        [](const ValExpr::VInt&) {
            return std::make_shared<const Type>(Type{Type::TInt{}, std::nullopt});
        },
        [](const ValExpr::VBool&) {
            return std::make_shared<const Type>(Type{Type::TBool{}, std::nullopt});
        },

        // Variable lookup
        [&](const ValExpr::VVar& v) -> std::shared_ptr<const Type> {
            std::shared_ptr<const Type> type = get_variable_type(env, v.name);
            if(!type) {
                report_error_location(val_expr->source_span);
                std::cerr << "Variable " << orig_name(v.name) << " not defined" << std::endl;
                return nullptr;
            }
            return type;
        },
        // I am not taking in a const as I would want to mutate this
        [&](ValExpr::VStruct& struct_expr) -> std::shared_ptr<const Type> {
            std::shared_ptr<const Type> standard_struct_type = 
                get_standardized_type(env.type_env.type_context, struct_expr.struct_type);
            if(standard_struct_type == nullptr) {
                report_error_location(val_expr->source_span);
                std::cerr << "Type of struct does not exist " << std::endl;
                return nullptr;
            }
            if(!std::holds_alternative<Type::TNamed>(standard_struct_type->t)) {
                report_error_location(val_expr->source_span);
                std::cerr << "Type declared is not that of a struct " << std::endl;
                return nullptr;
            }
            std::string struct_name = std::get<Type::TNamed>(standard_struct_type->t).name;
            NameableType::Struct struct_type = get_struct_type(env.type_env.type_context, struct_name).value();
            if(struct_valid(env, standard_struct_type->viewpoint, struct_type, struct_expr)) {
                return standard_struct_type;
            }
            else {
                report_error_location(val_expr->source_span);
                std::cerr << "Field types do not match the type of the struct" << std::endl;
                return nullptr;
            }
        },
        
        // Allocations
        [&](const ValExpr::NewInstance& new_instance) -> std::shared_ptr<const Type> {
            auto size_type = val_expr_type(env, new_instance.size);
            if(!size_type) {
                return nullptr;
            }
            std::shared_ptr<const Type> init_expr_type = val_expr_type(env, new_instance.init_expr);
            if(!init_expr_type) {
                return nullptr;
            }
            init_expr_type = apply_viewpoint_to_type(new_instance.cap, init_expr_type);
            if(!type_assignable(env.type_env.type_context, new_instance.type, init_expr_type)) {
                report_error_location(val_expr->source_span);
                std::cerr << "Initialization expression does not match user type" << std::endl;
                return nullptr;
            }
            auto int_type = std::make_shared<Type>(Type{Type::TInt{}, std::nullopt});
            if(!type_assignable(env.type_env.type_context, int_type, size_type)) {
                report_error_location(val_expr->source_span);
                std::cerr << "Size expression must be of type int" << std::endl;
                return nullptr;
            }
            return std::make_shared<Type>(
                    Type{Type::Pointer{new_instance.type, new_instance.cap}, std::nullopt});
        },

        [&](const ValExpr::ActorConstruction& actor_constr_expr) -> std::shared_ptr<const Type> {
            if(!env.type_env.decl_collection->actor_frontend_map.contains(actor_constr_expr.actor_name)) {
                report_error_location(val_expr->source_span);
                std::cerr << "Actor " << actor_constr_expr.actor_name << " not found" << std::endl;
                return nullptr;
            }
            auto& actor_constructors = 
                (env.type_env.decl_collection->actor_frontend_map.at(actor_constr_expr.actor_name))->constructors;
            if(actor_constructors.find(actor_constr_expr.constructor_name) == actor_constructors.end()) {
                report_error_location(val_expr->source_span);
                std::cerr << "Actor " << actor_constr_expr.actor_name << " does not have constructor "
                    << actor_constr_expr.constructor_name << std::endl;
                return nullptr;
            }
            std::shared_ptr<TopLevelItem::Constructor> constructor =
                actor_constructors.at(actor_constr_expr.constructor_name);
            if(!passed_in_parameters_valid(env, constructor->params, actor_constr_expr.args, false)) {
                report_error_location(val_expr->source_span);
                std::cerr << "Parameters passed into the constructor are not valid" << std::endl;
                return nullptr;
            }
            return std::make_shared<Type>(Type{Type::TActor{actor_constr_expr.actor_name}, std::nullopt});
        },

        // Consume
        [&](const ValExpr::Consume& consume) -> std::shared_ptr<const Type> {
            if(!env.local_var_context.contains(consume.var_name)) {
                report_error_location(val_expr->source_span);
                std::cerr << "Consumed variable is not a local variable" << std::endl;
                return nullptr;
            }
            return unaliased_type(env.local_var_context.at(consume.var_name));
        },

        // Accesses
        [&](const ValExpr::PointerAccess& pointer_access) -> std::shared_ptr<const Type> {
            auto index_type = val_expr_type(env, pointer_access.index);
            if(!index_type) {
                return nullptr;
            }
            auto int_type = std::make_shared<Type>(Type{Type::TInt{}, std::nullopt});
            if(!type_assignable(env.type_env.type_context, int_type, index_type)) {
                report_error_location(val_expr->source_span);
                std::cerr << "Index type is not int" << std::endl;
                return nullptr;
            }
            auto internal_type = val_expr_type(env, pointer_access.value);
            if(!internal_type) {
                return nullptr;
            }
            auto* pointer_type = std::get_if<Type::Pointer>(&internal_type->t);
            if(pointer_type == nullptr) {
                report_error_location(val_expr->source_span);
                std::cerr << "Dereferenced object is not a pointer" << std::endl;
                return nullptr;
            }
            Cap ptr_cap = viewpoint_adaptation_op(internal_type->viewpoint, pointer_type->cap).value();
            return std::visit(Overload{
                [&](const Cap::Tag&) -> std::shared_ptr<const Type> {
                    report_error_location(val_expr->source_span);
                    std::cerr << "Cannot dereference a tag pointer" << std::endl;
                    return nullptr;
                },
                [&](const Cap::Locked&) -> std::shared_ptr<const Type> {
                    if(env.atomic_nest_level == 0) {
                        report_error_location(val_expr->source_span);
                        std::cerr << "Dereferencing an object protected by a lock outside an atomic section" << std::endl;
                        return nullptr;
                    }
                    return get_dereferenced_type(internal_type);
                },
                [&](const auto&) {
                    return get_dereferenced_type(internal_type);
                }
            }, ptr_cap.t);
        },

        [&](const ValExpr::Field& field_access) -> std::shared_ptr<const Type> {
            std::shared_ptr<const Type> struct_type = val_expr_type(env, field_access.base);
            if(!struct_type) {
                return nullptr;
            }
            auto* named_type = std::get_if<Type::TNamed>(&struct_type->t);
            if(!named_type) {
                report_error_location(val_expr->source_span);
                std::cerr << "Field access on an expression which is not of a struct type" << std::endl;
                return nullptr;
            }
            auto struct_contents = get_struct_type(env.type_env.type_context, named_type->name);
            if(!struct_contents) {
                report_error_location(val_expr->source_span);
                std::cerr << "Field access on an expression which is not of a struct type" << std::endl;
                return nullptr;
            }
            for(const auto& [field_name, field_type]: struct_contents->members) {
                if(field_name == field_access.field) {
                    return apply_viewpoint_to_type(struct_type->viewpoint, field_type);
                }
            }
            report_error_location(val_expr->source_span);
            std::cerr << "Field " << field_access.field << " is not part of the struct" << std::endl;
            return nullptr;
        },

        // Assignments
        [&](const ValExpr::Assignment& assignment) -> std::shared_ptr<const Type> {
            auto lhs_type = val_expr_type(env, assignment.lhs);
            if(!lhs_type) { return nullptr; }
            auto rhs_type = val_expr_type(env, assignment.rhs);
            if(!rhs_type) { return nullptr; }
            if(!type_assignable(env.type_env.type_context, lhs_type, rhs_type)) {
                report_error_location(val_expr->source_span);
                std::cerr << "These types are not compatible in an assignment" << std::endl;
                return nullptr;
            }
            // Now just need to check whether the thingy is actually assignable scene
            if(can_appear_in_lhs(env, assignment.lhs)) {
                return unaliased_type(lhs_type);
            }
            else {
                report_error_location(val_expr->source_span);
                std::cerr << "This expression cannot appear in the lhs of an assignment" << std::endl;
                return nullptr;
            }
        },

        // Callables
        [&](const ValExpr::FuncCall& f) -> std::shared_ptr<const Type> {
            // Lookup function definition in the type context
            std::shared_ptr<TopLevelItem::Func> func_def =
                get_func_def(f.func, env.type_env.curr_actor, env.type_env.decl_collection);
            if(func_def == nullptr) {
                report_error_location(val_expr->source_span);
                std::cerr << "Function not found" << std::endl;
                return nullptr;
            }

            if(passed_in_parameters_valid(env, func_def->params, f.args, false)) {
                return func_def->return_type;
            }
            report_error_location(val_expr->source_span);
            std::cerr << "Passed in parameters are not valid" << std::endl;
            return nullptr;
        },

        // Binary operation
        [&](const ValExpr::BinOpExpr& e) -> std::shared_ptr<const Type> {
            std::shared_ptr<const Type> lhs_t = val_expr_type(env, e.lhs);
            std::shared_ptr<const Type> rhs_t = val_expr_type(env, e.rhs);
            if(!(lhs_t && rhs_t)) {
                return nullptr;
            }
            switch(e.op) {
                case BinOp::Eq:
                case BinOp::Neq:
                    if(!type_equality_comparable(env.type_env.type_context, lhs_t, rhs_t)) {
                        report_error_location(val_expr->source_span);
                        std::cerr << "Types are not equality comparable" << std::endl;
                        return nullptr;
                    }
                    return std::make_shared<Type>(Type{Type::TBool{}, std::nullopt});
                case BinOp::Add:
                case BinOp::Sub:
                case BinOp::Mul:
                case BinOp::Div:
                    if(!(type_is_int(env.type_env.type_context, lhs_t) && type_is_int(env.type_env.type_context, rhs_t))) {
                        report_error_location(val_expr->source_span);
                        std::cerr << "Type not compatible for arithmetic" << std::endl;
                        return nullptr;
                    }
                    else {
                        return std::make_shared<Type>(Type{Type::TInt{}, std::nullopt});
                    }
                case BinOp::Geq:
                case BinOp::Leq:
                case BinOp::Gt:
                case BinOp::Lt:
                    if(!type_rel_comparable(env.type_env.type_context, lhs_t, rhs_t)) {
                        report_error_location(val_expr->source_span);
                        std::cerr << "Type not compatible for relative comparisons" << std::endl;
                    }
                    return std::make_shared<Type>(Type{Type::TBool{}, std::nullopt});
            }
            assert(false);
            return nullptr;
        }

    }, val_expr->t);
    if(full_type != nullptr) {
        val_expr->expr_type = full_type;
    }
    return full_type;
}

bool type_check_stmt_list(CoreEnv &env, const std::vector<std::shared_ptr<Stmt>>& stmt_list);

bool type_check_statement(CoreEnv& env, std::shared_ptr<Stmt> stmt) {
    return std::visit(Overload{
        [&](const Stmt::VarDeclWithInit& var_decl_with_init) {
            auto init_expr_type = val_expr_type(env, var_decl_with_init.init);
            if(!init_expr_type) {
                return false;
            }
            auto expected_type = var_decl_with_init.type;
            if(!type_assignable(env.type_env.type_context, expected_type, init_expr_type)) {
                report_error_location(stmt->source_span);
                std::cerr <<  "Assigned expression type is not compatible with the declaration type"
                << std::endl;
                return false;
            }
            env.local_var_context.emplace(var_decl_with_init.name, var_decl_with_init.type);
            return true;
        },
        [&](const Stmt::MemberInitialize& member_init) {
            if(env.local_var_context.contains(member_init.member_name)) {
                report_error_location(stmt->source_span);
                std::cerr << "Cannot initialize a non-member variable" << std::endl;
                return false;
            }
            auto expected_type = get_actor_member_type(env, member_init.member_name);
            if(!expected_type) {
                report_error_location(stmt->source_span);
                std::cerr << "Actor member " << member_init.member_name <<
                " does not exist" << std::endl;
                return false;
            }
            auto present_type = val_expr_type(env, member_init.init);
            if(!present_type) {
                return false;
            }
            return type_assignable(env.type_env.type_context, expected_type, present_type);
        },
        [&](const Stmt::BehaviourCall& b) {
            auto actor_type = val_expr_type(env, b.actor);
            if(!actor_type) { return false; }
            const Type::TActor* named_actor = std::get_if<Type::TActor>(&actor_type->t);
            if(!named_actor) {
                report_error_location(stmt->source_span);
                std::cerr << "Type of the expression is not of an actor" << std::endl;
                return false;
            }

            assert(env.type_env.decl_collection->actor_frontend_map.contains(named_actor->name));
            auto& actor_behaviours = 
                env.type_env.decl_collection->actor_frontend_map.at(named_actor->name)->member_behaviours;
            if(actor_behaviours.find(b.behaviour_name) == actor_behaviours.end()) {
                report_error_location(stmt->source_span);
                std::cerr << "Actor " << named_actor->name << " does not have behaviour "
                    << b.behaviour_name << std::endl;
                return false;
            }
            std::shared_ptr<TopLevelItem::Behaviour> called_behaviour 
                = actor_behaviours[b.behaviour_name];
            if(!passed_in_parameters_valid(env, called_behaviour->params, b.args, true)) {
                report_error_location(stmt->source_span);
                std::cerr << "Passed in parameters to the behaviour are not valid" << std::endl;
                return false;
            }
            return true;
        },
        [&](const Stmt::Print& print_expr) {
            auto type = val_expr_type(env, print_expr.print_expr);
            if(!type_is_printable) {
                report_error_location(print_expr.print_expr->source_span);
                std::cerr << "Expression is not printable" << std::endl;
                return false;
            }
            return true;
        },
        [&](const Stmt::Expr& val_expr) {
            auto type = val_expr_type(env, val_expr.expr);
            return type != nullptr;
        },
        [&](const Stmt::If& if_expr) {
            auto cond_type = val_expr_type(env, if_expr.cond);
            if(!cond_type) {
                return false;
            }
            auto bool_type = std::make_shared<Type>(Type{Type::TBool{}, std::nullopt});
            if(!type_is_bool(env.type_env.type_context, cond_type)) {
                return false;
            }
            // Creating a scope for the if block
            bool if_block_type_check = type_check_stmt_list(env, if_expr.then_body);
            bool else_block_type_check = if_expr.else_body ? type_check_stmt_list(env, *if_expr.else_body) : true;
            return if_block_type_check && else_block_type_check;
        },
        [&](const Stmt::While& while_expr) {
            // creating scope for the cond
            auto cond_type = val_expr_type(env, while_expr.cond);
            if(!cond_type) {
                return false;
            }
            if(!type_is_bool(env.type_env.type_context, cond_type)) {
                report_error_location(while_expr.cond->source_span);
                std::cerr << "Condition does not have type boolean" << std::endl;
                return false;
            }
            // Creating a scope for the body
            bool body_valid = type_check_stmt_list(env, while_expr.body);
            return body_valid;
        },
        [&](std::shared_ptr<Stmt::Atomic> atomic_block) {
            env.atomic_nest_level++;
            bool body_valid = type_check_stmt_list(env, atomic_block->body);
            env.atomic_nest_level--;
            return body_valid;
        },
        [&](const Stmt::Return& return_stmt) {
            auto return_expr_type = val_expr_type(env, return_stmt.expr);
            if(!return_expr_type) {
                return false;
            }
            if(!env.curr_func) {
                report_error_location(stmt->source_span);
                std::cerr << "Returns outside a function are not allowed" << std::endl;
                return false;
            }
            auto expected_return_type = env.curr_func->return_type;
            if (!type_assignable(env.type_env.type_context, expected_return_type, return_expr_type)) {
                report_error_location(stmt->source_span);
                std::cerr << "Return type does not match with the expected return type of the function" << std::endl;
                return false;
            }
            return true;
        }
    }, stmt->t);
}


bool type_check_stmt_list(CoreEnv &env, const std::vector<std::shared_ptr<Stmt>>& stmt_list) {
    bool result = true;
    for(auto stmt: stmt_list) {
        if(!type_check_statement(env, stmt)) {
            result = false;
        }
    }
    return result;
}

bool type_check_callable_body(
    TypeEnv& env,
    std::shared_ptr<TopLevelItem::Func> curr_func,
    const std::vector<TopLevelItem::VarDecl>& params,
    const std::vector<std::shared_ptr<Stmt>>& callable_body) {
    CoreEnv core_env(env, curr_func);
    for(const TopLevelItem::VarDecl& param: params) {
        core_env.local_var_context.emplace(param.name, param.type);
    }
    return type_check_stmt_list(core_env, callable_body);
}