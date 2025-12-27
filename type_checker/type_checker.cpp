// For typing a value expression
#include "utils.hpp"
#include <iostream>
#include <algorithm>
#include <optional>
#include "defer.cpp"

// CR: Allow assignment of sensible struct types
// CR: Fix the weird logic for nameable types (introduce the invariant that the nameable
// type cannot be a BasicType::TNamed)
// CR: Improve the logic of [ScopedStore], and use it to simplify the logic here. Fix the terrible design
// and breaking of SRP in [VarContext]

std::optional<FullType> val_expr_type(TypeEnv& env, std::shared_ptr<ValExpr> val_expr) {
    std::optional<FullType> full_type = std::visit(Overload{
        // Literal values
        [&](const ValExpr::VUnit&) {
            return std::optional(FullType{ BasicType{ BasicType::TUnit{} } });
        },
        [](const ValExpr::VInt&) {
            return std::optional(FullType{ BasicType{ BasicType::TInt{} } });
        },
        [](const ValExpr::VBool&) {
            return std::optional(FullType{ BasicType{ BasicType::TBool{} } });
        },

        // Variable lookup
        [&](const ValExpr::VVar& v) -> std::optional<FullType> {
            std::optional<FullType> type = get_variable_type(env, v.name);
            if(!type) {
                report_error_location(val_expr->source_span);
                std::cerr << "Variable " << v.name << " not defined" << std::endl;
                return std::nullopt;
            }
            return *type;
        },
        // I am not taking in a const as I would want to mutate this
        [&](ValExpr::VStruct& struct_expr) -> std::optional<FullType> {
            std::optional<NameableType::Struct> struct_contents = get_struct_type(env, struct_expr.type_name);
            if(!struct_contents) {
                report_error_location(val_expr->source_span);
                std::cerr << "Internal type of " << struct_expr.type_name << " is not a struct" << std::endl;
                return std::nullopt;
            }
            if(struct_valid(env, *struct_contents, struct_expr)) {
                std::optional<BasicType> basic_type = standardize_type(env.type_context, struct_expr.type_name);
                assert(basic_type);
                return (FullType { *basic_type });
            }
            else {
                report_error_location(val_expr->source_span);
                std::cerr << "Field types do not match the type of the struct" << std::endl;
                return std::nullopt;
            }
        },
        
        // Allocations
        [&](const ValExpr::NewInstance& new_instance) -> std::optional<FullType> {
            auto size_type = val_expr_type(env, new_instance.size);
            if(!size_type) {
                // No need to log as the previous [val_expr_type] call would have performed the logging
                return std::nullopt;
            }
            auto default_type = val_expr_type(env, new_instance.default_value);
            if(default_type == std::nullopt) {
                return std::nullopt;
            }
            auto basic_type = std::get_if<BasicType>(&default_type->t);
            if(!basic_type) {
                report_error_location(val_expr->source_span);
                std::cerr << "Default value expr does not match the intended type" << std::endl;
                return std::nullopt;
            }
            if(basic_type_equal(env.type_context, *basic_type, new_instance.type)) {
                return unaliased_type(env, FullType {FullType::Pointer {new_instance.type, new_instance.cap}});
            }
            report_error_location(val_expr->source_span);
            std::cerr << "Default value expr does not match the intended type" << std::endl;
            return std::nullopt;
        },

        [&](const ValExpr::ActorConstruction& actor_constr_expr) -> std::optional<FullType> {
            if(env.actor_frontend_map.find(actor_constr_expr.actor_name) == env.actor_frontend_map.end()) {
                report_error_location(val_expr->source_span);
                std::cerr << "Actor " << actor_constr_expr.actor_name << " not found" << std::endl;
                return std::nullopt;
            }
            auto& actor_constructors = 
                (env.actor_frontend_map[actor_constr_expr.actor_name])->constructors;
            if(actor_constructors.find(actor_constr_expr.constructor_name) == actor_constructors.end()) {
                report_error_location(val_expr->source_span);
                std::cerr << "Actor " << actor_constr_expr.actor_name << " does not have constructor "
                    << actor_constr_expr.constructor_name << std::endl;
                return std::nullopt; 
            }
            std::shared_ptr<TopLevelItem::Constructor> constructor = 
                actor_constructors[actor_constr_expr.constructor_name];
            if(!passed_in_parameters_valid(env, constructor->params, actor_constr_expr.args, false)) {
                report_error_location(val_expr->source_span);
                std::cerr << "Parameters passed into the constructor are not valid" << std::endl;
                return std::nullopt;
            }
            return FullType {BasicType {BasicType::TActor {actor_constr_expr.actor_name}}};
        },

        // Accesses
        [&](const ValExpr::PointerAccess& pointer_access) -> std::optional<FullType> {
            auto index_type = val_expr_type(env, pointer_access.index);
            if(!index_type) {
                return std::nullopt;
            }
            if(!full_type_equal(env.type_context, *index_type, FullType { BasicType {BasicType::TInt {}}})) {
                report_error_location(val_expr->source_span);
                std::cerr << "Index type is not int" << std::endl;
                return std::nullopt;
            }
            auto internal_type = val_expr_type(env, pointer_access.value);
            if(!internal_type) {
                return std::nullopt;
            }
            auto* pointer_type = std::get_if<FullType::Pointer>(&internal_type->t);
            if(pointer_type == nullptr) {
                report_error_location(val_expr->source_span);
                std::cerr << "Dereferenced object is not a pointer" << std::endl;
                return std::nullopt;
            }
            if(auto* locked_cap = std::get_if<Cap::Locked>(&pointer_type->cap.t)) {
                if(!env.atomic_section_data.in_atomic_section()) {
                    report_error_location(val_expr->source_span);
                    std::cerr << "Dereferencing an object protected by a lock outside an atomic section" << std::endl;
                    return std::nullopt;
                }
                env.atomic_section_data.add_lock(locked_cap->lock_name);
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
                report_error_location(val_expr->source_span);
                std::cerr << "Field access on an expression which is not of a struct type" << std::endl;
                return std::nullopt;
            }
            auto* named_type = std::get_if<BasicType::TNamed>(&basic_type->t);
            auto struct_contents = get_struct_type(env, named_type->name);
            if(!struct_contents) {
                report_error_location(val_expr->source_span);
                std::cerr << "Field access on an expression which is not of a struct type" << std::endl;
                return std::nullopt;
            }
            for(const auto& [field_name, field_basic_type]: struct_contents->members) {
                if(field_name == field_access.field) {
                    return FullType {field_basic_type};
                }
            }
            report_error_location(val_expr->source_span);
            std::cerr << "Field " << field_access.field << " is not part of the struct" << std::endl;
            return std::nullopt;
        },

        // Assignments
        [&](const ValExpr::Assignment& assignment) -> std::optional<FullType> {
            auto lhs_type = val_expr_type(env, assignment.lhs);
            if(!lhs_type) { return std::nullopt; }
            auto rhs_type = val_expr_type(env, assignment.rhs);
            if(!rhs_type) { return std::nullopt; }
            if(!type_assignable(env.type_context, *lhs_type, *rhs_type)) {
                report_error_location(val_expr->source_span);
                std::cerr << "These types are not compatible in an assignment" << std::endl;
                return std::nullopt;
            }
            // Now just need to check whether the thingy is actually assignable scene
            if(can_appear_in_lhs(env, assignment.lhs)) {
                return unaliased_type(env, *lhs_type);
            }
            else {
                report_error_location(val_expr->source_span);
                std::cerr << "This expression cannot appear in the lhs of an assignment" << std::endl;
                return std::nullopt;
            }
        },

        // Callables
        [&](const ValExpr::FuncCall& f) -> std::optional<FullType> {
            // Lookup function definition in the type context
            auto func_def_opt = env.func_name_map.get_value(f.func);
            if(!func_def_opt) {
                report_error_location(val_expr->source_span);
                std::cerr << "Function not found" << std::endl;
                return std::nullopt;
            }
            auto func_def = *func_def_opt;
            env.atomic_section_data.add_locks_from_set(func_def->locks_dereferenced);

            if(passed_in_parameters_valid(env, func_def->params, f.args, false)) {
                return func_def->return_type;
            }
            report_error_location(val_expr->source_span);
            std::cerr << "Passed in parameters are not valid" << std::endl;
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
                        report_error_location(val_expr->source_span);
                        std::cerr << "Types not compatible for equality checking" << std::endl;
                        return std::nullopt;
                    }
                    if(auto* named_type = std::get_if<BasicType::TNamed>(&lhs_base.t)) {
                        auto standard_type = standardize_type(env.type_context, named_type->name);
                        assert(standard_type);
                        if(std::get_if<BasicType::TNamed>(&standard_type->t)) {
                            report_error_location(val_expr->source_span);
                            std::cerr << "Type cannot be equality compared" << std::endl;
                            return std::nullopt;
                        }
                    }
                    return FullType {BasicType {BasicType::TBool {}}};
                case BinOp::Add:
                case BinOp::Sub:
                case BinOp::Mul:
                case BinOp::Div:
                    if(!(type_is_numeric(env.type_context, *lhs_t) && type_is_numeric(env.type_context, *rhs_t))) {
                        report_error_location(val_expr->source_span);
                        std::cerr << "Type not compatible for arithmetic" << std::endl;
                        return std::nullopt;
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
                        report_error_location(val_expr->source_span);
                        std::cerr << "Type not compatible for comparisons" << std::endl;
                        return std::nullopt;
                    }
            }
            assert(false);
            return std::nullopt;
        }

    }, val_expr->t);
    if(full_type != std::nullopt) {
        val_expr->expr_type = *full_type;
    }
    return full_type;
}

bool type_check_statement(TypeEnv& env, std::shared_ptr<Stmt> stmt) {
    return std::visit(Overload{
        [&](const Stmt::VarDeclWithInit& var_decl_with_init) {
            if(!variable_overridable_in_scope(env, var_decl_with_init.name)) {
                return false;
            }
            auto init_expr_type = val_expr_type(env, var_decl_with_init.init);
            if(!init_expr_type) {
                return false;
            }
            auto expected_type = var_decl_with_init.type;
            if(!type_assignable(env.type_context, expected_type, *init_expr_type)) {
                report_error_location(stmt->source_span);
                std::cerr <<  "Assigned expression type is not compatible with the declaration type" 
                << std::endl;
                return false;
            }
            env.var_context.insert(var_decl_with_init.name, var_decl_with_init.type);
            return true;
        },
        [&](const Stmt::MemberInitialize& member_init) {
            if(env.var_context.get_value(member_init.member_name)) {
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
            return type_assignable(env.type_context, *expected_type, *present_type);
        },
        [&](const Stmt::BehaviourCall& b) {
            auto actor_type = val_expr_type(env, b.actor);
            if(!actor_type) { return false; }
            auto basic_type = std::get_if<BasicType>(&actor_type->t);
            if(!basic_type) {
                report_error_location(stmt->source_span);
                std::cerr << "Type of the expression is not of an actor" << std::endl;
                return false;
            }
            BasicType::TActor* named_actor = std::get_if<BasicType::TActor>(&basic_type->t);
            if(!named_actor) {
                report_error_location(stmt->source_span);
                std::cerr << "Type of the expression is not of an actor" << std::endl;
                return false;
            }

            assert(env.actor_frontend_map.find(named_actor->name) != env.actor_frontend_map.end());
            auto& actor_behaviours = env.actor_frontend_map[named_actor->name]->member_behaviours;
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
            ScopeGuard scoped_guard(env.var_context);
            env.atomic_section_data.enter_atomic_section(atomic_block);
            bool body_valid = type_check_stmt_list(env, atomic_block->body);
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
            if (!full_type_equal(env.type_context, *return_expr_type, expected_return_type)) {
                report_error_location(stmt->source_span);
                std::cerr << "Return type does not match with the expected return type of the function" << std::endl;
                return false;
            }
            return true;
        }
    }, stmt->t);
}


bool type_check_toplevel_item(TypeEnv& env, TopLevelItem toplevel_item) {
    return std::visit(Overload{
        [&](const TopLevelItem::TypeDef& type_def) {
            if(env.type_context.find(type_def.type_name) != env.type_context.end()) {
                report_error_location(toplevel_item.source_span);
                std::cerr << "Type " << type_def.type_name << " already defined" << std::endl;
                return false;
            }
            env.type_context.emplace(type_def.type_name, type_def.nameable_type);
            return true;
        },
        [&](std::shared_ptr<TopLevelItem::Func> func_def) {
            return type_check_function(env, func_def, toplevel_item.source_span);
        },
        [&](std::shared_ptr<TopLevelItem::Actor> actor_def) {
            // Checking that the fields are unique
            if(env.actor_frontend_map.find(actor_def->name) != env.actor_frontend_map.end()) {
                report_error_location(toplevel_item.source_span);
                std::cerr << "Actor " << actor_def->name << " already defined." << std::endl;
                return false;
            }
            // Create an actor frontend
            std::shared_ptr<ActorFrontend> actor_frontend = std::make_shared<ActorFrontend>();
            actor_frontend->actor_name = actor_def->name;
            env.actor_frontend_map.emplace(actor_def->name, actor_frontend);
            
            // Creating a scope for the functions within the actor
            ScopeGuard actor_func_scope(env.func_name_map);
            env.curr_actor = actor_def;
            Defer d([&](void){env.curr_actor = nullptr;});
            bool type_checked_successfully = true;
            // Typecheck members as they come
            for(auto actor_mem: actor_def->actor_members) {
                type_checked_successfully =  std::visit(Overload{
                    [&](std::shared_ptr<TopLevelItem::Func> mem_func) {
                        return type_check_function(env, mem_func, toplevel_item.source_span);
                    },
                    [&](std::shared_ptr<TopLevelItem::Behaviour> mem_behaviour) {
                        actor_frontend->member_behaviours[mem_behaviour->name] = mem_behaviour;
                        return type_check_behaviour(env, mem_behaviour);
                    },
                    [&](std::shared_ptr<TopLevelItem::Constructor> mem_constructor) {
                        actor_frontend->constructors[mem_constructor->name] = mem_constructor;
                        return type_check_constructor(env, mem_constructor);
                    }
                }, actor_mem) && type_checked_successfully;
            }
            return type_checked_successfully;
        }
    }, toplevel_item.t);
}

bool type_check_program(Program* root) {
    bool program_typechecks = true;
    TypeEnv env;
    env.func_name_map.create_new_scope();
    for(auto top_level_item: root->top_level_items) {
        program_typechecks = type_check_toplevel_item(env, top_level_item) && program_typechecks;
    }
    if(!program_typechecks) {
        // CR: This error message is probably redundant
        std::cerr << "Program does not typecheck" << std::endl;
        return false;
    }
    // Checking whether there is an actor with name [Main]
    if(env.actor_frontend_map.find("Main") == env.actor_frontend_map.end()) {
        std::cerr << "Error: Actor Main not found" << std::endl;
        return false;
    }
    return true;
}


