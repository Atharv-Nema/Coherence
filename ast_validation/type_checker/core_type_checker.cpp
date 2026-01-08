#include <assert.h>
#include <iostream>
#include <functional>
#include "core_type_checker.hpp"
#include <iostream>
#include "pattern_matching_boilerplate.hpp"
#include "alpha_renaming.hpp"
#include "defer.cpp"
#include "utils.hpp"


struct CoreEnv {
    TypeEnv& type_env;
    std::shared_ptr<TopLevelItem::Func> curr_func;
    std::unordered_map<std::string, FullType> local_var_context;
    int atomic_nest_level = 0;
    CoreEnv(
        TypeEnv& type_env, 
        std::shared_ptr<TopLevelItem::Func> curr_func): type_env(type_env) {
        this->curr_func = curr_func;
    }
};

BasicType extract_basic_type(const FullType& full_type) {
    return std::visit(Overload{
        [](const BasicType& basic_type) {return basic_type;},
        [](const FullType::Pointer& ptr) {return ptr.base;}
    }, full_type.t);
}



std::optional<BasicType> standardize_type(TypeContext& type_context, const std::string& type_name) {
    if(type_context.find(type_name) == type_context.end()) {
        return std::nullopt;
    }
    auto nameable_type = type_context.find(type_name)->second;
    if(auto* basic_type = std::get_if<NameableType::Basic>(&nameable_type->t)) {
        if(auto* named_type = std::get_if<BasicType::TNamed>(&basic_type->type.t)) {
            auto res = standardize_type(type_context, named_type->name);
            if(res == std::nullopt) {
                type_context.erase(type_name);
            }
            else {
                std::shared_ptr<NameableType> nameable_type_ptr = 
                    std::make_shared<NameableType>(NameableType::Basic {*res});
                type_context.emplace(type_name, nameable_type_ptr);
            }
            return res;
        }
        else {
            return basic_type->type;
        }
    }
    else {
        return BasicType {BasicType::TNamed {type_name}};
    }
    return std::nullopt;
}

bool ref_cap_equal(Cap c1, Cap c2) {
    if (c1.t.index() != c2.t.index()) 
        return false;
    if (auto* locked_1 = std::get_if<Cap::Locked>(&c1.t)) {
        auto* locked_2 = std::get_if<Cap::Locked>(&c2.t);
        assert(locked_2 != nullptr);
        return locked_1->lock_name == locked_2->lock_name;
    }
    return true;
}

bool basic_type_equal(TypeContext& type_context, const BasicType& type_1, const BasicType& type_2) {
    if (type_1.t.index() != type_2.t.index()) 
        return false;
    if (auto* named_type_1 = std::get_if<BasicType::TNamed>(&type_1.t)) {
        auto* named_type_2 = std::get_if<BasicType::TNamed>(&type_1.t);
        assert(named_type_2 != nullptr);
        auto standard_type_1 = standardize_type(type_context, named_type_1->name);
        auto standard_type_2 = standardize_type(type_context, named_type_1->name);
        if(!(standard_type_1 && standard_type_2)) {
            return false;
        }
        if(named_type_1->name == named_type_2->name) {
            return true;
        }
        return basic_type_equal(type_context, *standard_type_1, *standard_type_2);
    }
    return true;
}

bool full_type_equal(TypeContext& type_context, const FullType& type_1, const FullType& type_2) {
    // Matching on type_1
    return std::visit(Overload {
        [&](const FullType::Pointer& type_1_ptr) {
            if(auto* type_2_ptr = std::get_if<FullType::Pointer>(&type_2.t)) {
                return ref_cap_equal(type_1_ptr.cap, type_2_ptr->cap) && 
                    basic_type_equal(type_context, type_1_ptr.base, type_2_ptr->base);
            }
            else {
                return false;
            }
        },
        [&](const BasicType& type_1_basic) {
            if(auto type_2_basic = std::get_if<BasicType>(&type_2.t)) {
                return basic_type_equal(type_context, type_1_basic, *type_2_basic);
            }
            else {
                return false;
            }
        }
    }, type_1.t);
}

bool capabilities_assignable(Cap c1, Cap c2) {
    return std::visit(Overload{
        // same kind: Ref, Val, Iso, Iso_cap
        [](const Cap::Ref&, const Cap::Ref&) { return true; },
        [](const Cap::Ref&, const Cap::Iso_cap&) { return true; },
        [](const Cap::Val&, const Cap::Val&) { return true; },
        [](const Cap::Val&, const Cap::Iso_cap&) { return true; },
        [](const Cap::Iso&, const Cap::Iso_cap&) { return true; },
        [](const Cap::Locked& l1, const Cap::Locked l2) {
            return l1.lock_name == l2.lock_name;
        },
        [](const Cap::Locked&, const Cap::Iso_cap&) {return true;},
        [](const auto&, const auto&) { return false; }
    }, c1.t, c2.t);
}

bool capabilities_sendable(Cap c1, Cap c2) {
    return std::visit(Overload{
        [](const Cap::Ref&, const Cap::Iso_cap&) { return true; },
        [](const Cap::Val&, const Cap::Val&) { return true; },
        [](const Cap::Val&, const Cap::Iso_cap&) { return true; },
        [](const Cap::Iso&, const Cap::Iso_cap&) { return true; },
        [](const Cap::Locked& l1, const Cap::Locked l2) {
            return l1.lock_name == l2.lock_name;
        },
        [](const Cap::Locked&, const Cap::Iso_cap&) {return true;},
        [](const auto&, const auto&) { return false; }
    }, c1.t, c2.t);
}

bool capability_mutable(Cap c) {
    return std::visit(Overload{
        [](const Cap::Val&) {return false;},
        [](const auto&) {return true;}
    }, c.t);
}

std::optional<FullType> val_expr_type(CoreEnv& env, std::shared_ptr<ValExpr> val_expr);

bool can_appear_in_lhs(CoreEnv& env, std::shared_ptr<ValExpr> expr) {
    // This assumes that expr is well-typed
    return std::visit(Overload{
        [](const ValExpr::VVar&) {return true;},
        [&](const ValExpr::PointerAccess& pointer_access) {
            // First, need to check whether the internal pointer of array_access is of an assignable type
            auto arr_type = val_expr_type(env, pointer_access.value);
            assert(arr_type);
            auto* ptr_type = std::get_if<FullType::Pointer>(&arr_type->t);
            if(!capability_mutable(ptr_type->cap)) {
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


bool type_is_numeric(TypeContext& type_context, FullType type) {
    auto* basic_type = std::get_if<BasicType>(&type.t);
    if(!basic_type) {
        return false;
    }
    return std::visit(Overload{
        [&](const BasicType::TInt&) {return true;},
        [&](const BasicType::TNamed& named_type) {
            auto standard_type = standardize_type(type_context, named_type.name);
            if(standard_type == std::nullopt) {
                return false;
            }
            if(std::get_if<BasicType::TNamed>(&standard_type->t)) {
                return false;
            }
            return type_is_numeric(type_context, FullType {*standard_type});
        },
        [&](const auto&) {return false;}
    }, basic_type->t);
}

bool type_is_printable(TypeContext &type_context, FullType full_type) {
    auto* basic_type = std::get_if<BasicType>(&full_type.t);
    if(!basic_type) {
        return false;
    }
    return std::visit(Overload{
        [&](const BasicType::TUnit&) {return false;},
        [&](const BasicType::TInt&) {return true;},
        [&](const BasicType::TBool&) {return false;},
        [&](const BasicType::TNamed& named_type) {
            auto standard_type = standardize_type(type_context, named_type.name);
            if(standard_type == std::nullopt) {
                return false;
            }
            if(std::get_if<BasicType::TNamed>(&standard_type->t)) {
                return false;
            }
            return type_is_numeric(type_context, FullType {*standard_type});
        },
        [&](const BasicType::TActor&) {return false;}
    }, basic_type->t);
}


bool type_assignable(TypeContext& type_context, const FullType& lhs, const FullType& rhs) {
    if(type_is_numeric(type_context, lhs) && type_is_numeric(type_context, rhs)) {
        return true;
    }
    if(!basic_type_equal(type_context, extract_basic_type(lhs), extract_basic_type(rhs))) {
        return false;
    }
    auto lhs_ptr = std::get_if<FullType::Pointer>(&lhs.t);
    if(lhs_ptr == nullptr) {
        // As the types are raw and equal and hence assignable
        return true;
    }
    // Now need to look at the assignability matrix
    auto rhs_ptr = std::get_if<FullType::Pointer>(&rhs.t);
    assert(rhs_ptr);
    return capabilities_assignable(lhs_ptr->cap, rhs_ptr->cap);
}

bool type_sendable(TypeContext& type_context, const FullType& lhs, const FullType& rhs) {
    // CR figure out how to reduce code duplication
    if(type_is_numeric(type_context, lhs) && type_is_numeric(type_context, rhs)) {
        return true;
    }
    if(!basic_type_equal(type_context, extract_basic_type(lhs), extract_basic_type(rhs))) {
        return false;
    }
    auto lhs_ptr = std::get_if<FullType::Pointer>(&lhs.t);
    if(lhs_ptr == nullptr) {
        // As the types are raw and equal and hence sendable
        return true;
    }
    // Now need to look at the assignability matrix
    auto rhs_ptr = std::get_if<FullType::Pointer>(&rhs.t);
    assert(rhs_ptr);
    return capabilities_sendable(lhs_ptr->cap, rhs_ptr->cap);
}

std::optional<FullType> get_actor_member_type(CoreEnv& env, const std::string& var_name) {
    if(!env.type_env.curr_actor) {
        return std::nullopt;
    }
    if(env.type_env.curr_actor->member_vars.find(var_name) == 
        env.type_env.curr_actor->member_vars.end()) {
        return std::nullopt;
    }
    return env.type_env.curr_actor->member_vars[var_name];
}

std::optional<FullType> get_variable_type(CoreEnv& env, const std::string& var_name) {
    if(env.local_var_context.find(var_name) != env.local_var_context.end()) {
        return env.local_var_context.at(var_name);
    }
    return get_actor_member_type(env, var_name);
}


bool check_type_expr_list_valid(
    CoreEnv& env, 
    const std::vector<FullType>& expected_types, 
    const std::vector<std::shared_ptr<ValExpr>>& val_expr_list, 
    std::function<bool(
        TypeContext& type_context, 
        const FullType& expected_type, 
        const FullType& arg_type)> arg_type_valid ) {
    if (expected_types.size() != val_expr_list.size()) {
        return false;
    }
    for(size_t i = 0; i < expected_types.size(); i++) {
        std::optional<FullType> arg_type = val_expr_type(env, val_expr_list[i]);
        if(!arg_type) {
            return false;
        }
        const FullType& expected_type = expected_types[i];

        if (!arg_type_valid(env.type_env.type_context, expected_type, *arg_type)) {
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
    std::vector<FullType> expected_types;
    expected_types.reserve(signature.size());
    for(const auto& var_decl: signature) {
        expected_types.push_back(var_decl.type);
    }
    return check_type_expr_list_valid(env, expected_types, arguments, (parameters_being_sent) ? type_sendable: type_assignable);
}



bool struct_valid(CoreEnv &env, NameableType::Struct& struct_contents, ValExpr::VStruct& struct_expr) {
    // Does not perform logging
    std::sort(struct_contents.members.begin(), struct_contents.members.end(), 
    [](const auto& a, const auto& b) { return a.first < b.first; });
    std::sort(struct_expr.fields.begin(), struct_expr.fields.end(),
    [](const auto& a, const auto& b) { return a.first < b.first; });
    if (struct_contents.members.size() != struct_expr.fields.size()) {
        return false;
    }
    std::vector<FullType> expected_types;
    std::vector<std::shared_ptr<ValExpr>> struct_members;
    size_t size = struct_contents.members.size();
    expected_types.reserve(size);
    struct_members.reserve(size);
    for(int i = 0; i < size; i++) {
        if(struct_contents.members[i].first != struct_expr.fields[i].first) {
            return false;
        }
        expected_types.emplace_back(FullType { struct_contents.members[i].second});
        struct_members.emplace_back(struct_expr.fields[i].second);
    }
    return check_type_expr_list_valid(env, expected_types, struct_members, full_type_equal);
}
// CR: Think carefully about the copying going on

std::optional<NameableType::Struct> get_struct_type(CoreEnv& env, const std::string& struct_type_name) {
    std::optional<BasicType> basic_type = standardize_type(env.type_env.type_context, struct_type_name);
    if(!basic_type) {
        return std::nullopt;
    }
    auto named_type = std::get_if<BasicType::TNamed>(&basic_type->t);
    if(!named_type) {
        return std::nullopt;
    }
    const std::string& standardized_type_name = named_type->name;
    assert(env.type_env.type_context.find(standardized_type_name) != env.type_env.type_context.end());
    auto struct_type = env.type_env.type_context[standardized_type_name];
    assert(std::holds_alternative<NameableType::Struct>(struct_type->t));
    const auto& struct_contents = std::get<NameableType::Struct>(struct_type->t);
    return struct_contents;

}

FullType unaliased_type(const FullType& full_type) {
    auto* pointer = std::get_if<FullType::Pointer>(&full_type.t);
    if(!pointer) {
        return full_type;
    }
    if(!std::holds_alternative<Cap::Iso>(pointer->cap.t)) {
        return full_type;
    }
    else {
        return FullType {FullType::Pointer {pointer->base, Cap::Iso_cap{}}};
    }
}

std::optional<FullType> val_expr_type(CoreEnv& env, std::shared_ptr<ValExpr> val_expr) {
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
                std::optional<BasicType> basic_type = standardize_type(env.type_env.type_context, struct_expr.type_name);
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
            if(basic_type_equal(env.type_env.type_context, *basic_type, new_instance.type)) {
                return unaliased_type(FullType {FullType::Pointer {new_instance.type, new_instance.cap}});
            }
            report_error_location(val_expr->source_span);
            std::cerr << "Default value expr does not match the intended type" << std::endl;
            return std::nullopt;
        },

        [&](const ValExpr::ActorConstruction& actor_constr_expr) -> std::optional<FullType> {
            if(!env.type_env.decl_collection->actor_frontend_map.contains(actor_constr_expr.actor_name)) {
                report_error_location(val_expr->source_span);
                std::cerr << "Actor " << actor_constr_expr.actor_name << " not found" << std::endl;
                return std::nullopt;
            }
            auto& actor_constructors = 
                (env.type_env.decl_collection->actor_frontend_map.at(actor_constr_expr.actor_name))->constructors;
            if(actor_constructors.find(actor_constr_expr.constructor_name) == actor_constructors.end()) {
                report_error_location(val_expr->source_span);
                std::cerr << "Actor " << actor_constr_expr.actor_name << " does not have constructor "
                    << actor_constr_expr.constructor_name << std::endl;
                return std::nullopt; 
            }
            std::shared_ptr<TopLevelItem::Constructor> constructor = 
                actor_constructors.at(actor_constr_expr.constructor_name);
            if(!passed_in_parameters_valid(env, constructor->params, actor_constr_expr.args, false)) {
                report_error_location(val_expr->source_span);
                std::cerr << "Parameters passed into the constructor are not valid" << std::endl;
                return std::nullopt;
            }
            return FullType {BasicType {BasicType::TActor {actor_constr_expr.actor_name}}};
        },

        // Consume
        [&](const ValExpr::Consume& consume) -> std::optional<FullType> {
            if(!env.local_var_context.contains(consume.var_name)) {
                report_error_location(val_expr->source_span);
                std::cerr << "Consumed variable is not a local variable" << std::endl;
                return std::nullopt;
            }
            return unaliased_type(env.local_var_context.at(consume.var_name));
        },

        // Accesses
        [&](const ValExpr::PointerAccess& pointer_access) -> std::optional<FullType> {
            auto index_type = val_expr_type(env, pointer_access.index);
            if(!index_type) {
                return std::nullopt;
            }
            if(!full_type_equal(env.type_env.type_context, *index_type, FullType { BasicType {BasicType::TInt {}}})) {
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
                if(env.atomic_nest_level == 0) {
                    report_error_location(val_expr->source_span);
                    std::cerr << "Dereferencing an object protected by a lock outside an atomic section" << std::endl;
                    return std::nullopt;
                }
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
            if(!type_assignable(env.type_env.type_context, *lhs_type, *rhs_type)) {
                report_error_location(val_expr->source_span);
                std::cerr << "These types are not compatible in an assignment" << std::endl;
                return std::nullopt;
            }
            // Now just need to check whether the thingy is actually assignable scene
            if(can_appear_in_lhs(env, assignment.lhs)) {
                return unaliased_type(*lhs_type);
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
            std::shared_ptr<TopLevelItem::Func> func_def = 
                get_func_def(f.func, env.type_env.curr_actor, env.type_env.decl_collection);
            if(func_def == nullptr) {
                report_error_location(val_expr->source_span);
                std::cerr << "Function not found" << std::endl;
                return std::nullopt;
            }

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
                    if(type_is_numeric(env.type_env.type_context, *lhs_t) && type_is_numeric(env.type_env.type_context, *rhs_t)) {
                        return FullType {BasicType { BasicType::TBool {}}};
                    }
                    
                    if(!basic_type_equal(env.type_env.type_context, lhs_base, rhs_base)) {
                        report_error_location(val_expr->source_span);
                        std::cerr << "Types not compatible for equality checking" << std::endl;
                        return std::nullopt;
                    }
                    if(auto* named_type = std::get_if<BasicType::TNamed>(&lhs_base.t)) {
                        auto standard_type = standardize_type(env.type_env.type_context, named_type->name);
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
                    if(!(type_is_numeric(env.type_env.type_context, *lhs_t) && type_is_numeric(env.type_env.type_context, *rhs_t))) {
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
                    if(type_is_numeric(env.type_env.type_context, *lhs_t) && type_is_numeric(env.type_env.type_context, *rhs_t)) {
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

bool type_check_stmt_list(CoreEnv &env, const std::vector<std::shared_ptr<Stmt>>& stmt_list);

bool type_check_statement(CoreEnv& env, std::shared_ptr<Stmt> stmt) {
    return std::visit(Overload{
        [&](const Stmt::VarDeclWithInit& var_decl_with_init) {
            auto init_expr_type = val_expr_type(env, var_decl_with_init.init);
            if(!init_expr_type) {
                return false;
            }
            auto expected_type = var_decl_with_init.type;
            if(!type_assignable(env.type_env.type_context, expected_type, *init_expr_type)) {
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
            return type_assignable(env.type_env.type_context, *expected_type, *present_type);
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
            return type != std::nullopt;
        },
        [&](const Stmt::If& if_expr) {
            auto cond_type = val_expr_type(env, if_expr.cond);
            if(!cond_type) {
                return false;
            }
            if(!full_type_equal(env.type_env.type_context, FullType {BasicType {BasicType::TBool{}}}, *cond_type)) {
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
            if(!full_type_equal(env.type_env.type_context, FullType {BasicType {BasicType::TBool{}}}, *cond_type)) {
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
            if (!full_type_equal(env.type_env.type_context, *return_expr_type, expected_return_type)) {
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