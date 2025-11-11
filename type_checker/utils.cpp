#include "../ast/expr.hpp"
#include <assert.h>
#include "type_checker.hpp"
#include <iostream>
#include <functional>
#include "utils.hpp"

void report_error_location(const SourceSpan& span) {
    std::cerr << "Error between line " << span.start.line << ", column " << span.start.char_no 
    << " and line " << span.end.line << ", column " << span.end.char_no << std::endl;
}

BasicType extract_basic_type(const FullType& full_type) {
    return std::visit(Overload{
        [](const BasicType& basic_type) {return basic_type;},
        [](const FullType::Pointer& ptr) {return ptr.base;}
    }, full_type.t);
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
    }
    return std::nullopt;
}

bool can_appear_in_lhs(TypeEnv& env, std::shared_ptr<ValExpr> expr) {
    // Anything that does not involve going through a deref of a val is game I think
    // This assumes that expr is well-typed ig
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

bool type_is_numeric(TypeContext& type_context, const FullType& type) {
    auto* basic_type = std::get_if<BasicType>(&type.t);
    if(!basic_type) {
        return false;
    }
    return std::visit(Overload{
        [&](const BasicType::TUnit&) {return false;},
        [&](const BasicType::TInt&) {return true;},
        [&](const BasicType::TFloat&) {return true;},
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
            if(auto type_2_basic = std::get_if<BasicType>(&type_1.t)) {
                return basic_type_equal(type_context, type_1_basic, *type_2_basic);
            }
            else {
                return false;
            }
        }
    }, type_1.t);
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

bool check_type_expr_list_valid(
    TypeEnv& env, 
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

        if (!type_assignable(env.type_context, expected_type, *arg_type)) {
            return false;
        }
    }
    return true;
}

// CR: Need to improve the API here. A boolean flag is pretty bad code
bool passed_in_parameters_valid(
    TypeEnv& env, 
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

bool struct_valid(TypeEnv &env, NameableType::Struct& struct_contents, ValExpr::VStruct& struct_expr) {
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

std::optional<NameableType::Struct> get_struct_type(TypeEnv& env, const std::string& struct_type_name) {
    std::optional<BasicType> basic_type = standardize_type(env.type_context, struct_type_name);
    if(!basic_type) {
        return std::nullopt;
    }
    auto named_type = std::get_if<BasicType::TNamed>(&basic_type->t);
    if(!named_type) {
        return std::nullopt;
    }
    const std::string& standardized_type_name = named_type->name;
    assert(env.type_context.find(standardized_type_name) != env.type_context.end());
    auto struct_type = env.type_context[standardized_type_name];
    assert(std::holds_alternative<NameableType::Struct>(struct_type->t));
    const auto& struct_contents = std::get<NameableType::Struct>(struct_type->t);
    return struct_contents;

}

FullType unaliased_type(TypeEnv& env, const FullType& full_type) {
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

bool type_check_stmt_list(TypeEnv &env, const std::vector<std::shared_ptr<Stmt>>& stmt_list) {
    for(auto stmt: stmt_list) {
        if(!type_check_statement(env, stmt)) {
            return false;
        }
    }
    return true;
}

bool statement_returns(TypeEnv &env, std::shared_ptr<Stmt> last_statement) {
    // Adds error messages here I guess
    return std::visit(Overload{
        [&](const Stmt::Atomic& atomic_block) {
            if(atomic_block.body.size() == 0) {
                return false;
            }
            return statement_returns(env, atomic_block.body.back());
        },
        [&](const Stmt::Return& return_stmt) {
            // It is already checked that [return_stmt] has the correct type.
            // Perhaps an assert here will be good, but I guess semantically
            // this guy just checks that the statement returns.
            return true;
        },
        [&](const Stmt::If& if_stmt) {
            if(if_stmt.then_body.size() == 0) {
                return false;
            }
            if(!if_stmt.else_body) {
                return false;
            }
            if(if_stmt.else_body->size() == 0) {
                return false;
            }
            bool if_block_returns = statement_returns(env, if_stmt.then_body.back());
            bool else_block_returns = statement_returns(env, if_stmt.else_body->back());
            return if_block_returns && else_block_returns;
        },
        [&](const auto&) {return false;}
    }, last_statement->t);
}
// Principle to be followed: Error messages are added when the real error occurs

bool add_arguments_to_scope(TypeEnv& env, const std::vector<TopLevelItem::VarDecl>& args) {
    // Returns false if there are duplicates, true otherwise
    auto curr_actor = env.var_context.get_enclosing_actor();
    if(curr_actor != nullptr) {
        env.var_context.insert_variable("this", FullType {BasicType {BasicType::TActor {curr_actor->name}}});
    }
    for(const TopLevelItem::VarDecl& arg: args) {
        if(!env.var_context.variable_overridable_in_scope(arg.name)) {
            return false;
        }
        env.var_context.insert_variable(arg.name, arg.type);
    }
    return true;
}

bool type_check_function(TypeEnv& env, std::shared_ptr<TopLevelItem::Func> func_def) {
    if(env.func_name_map.key_in_curr_scope(func_def->name)) {
        return false;
    }
    env.func_name_map.insert(func_def->name, func_def);
    // Creating a scope
    ScopeGuard guard(env.var_context, func_def);

    if(!add_arguments_to_scope(env, func_def->params)) {
        return false;
    }
    
    // Checking that each individual statement is well-formed
    if(!type_check_stmt_list(env, func_def->body)) {
        return false;
    }
    // Now checking that the last statement actually returns something
    if(func_def->body.size() == 0) {
        return false;
    }
    return statement_returns(env, func_def->body.back());
}

bool type_check_behaviour(TypeEnv& env, std::shared_ptr<TopLevelItem::Behaviour> behaviour_def) {
    // Creating a scope
    ScopeGuard guard(env.var_context, behaviour_def);
    
    if(!add_arguments_to_scope(env, behaviour_def->params)) {
        return false;
    }
    // Checking that each individual statement is well-formed
    return type_check_stmt_list(env, behaviour_def->body);
}

bool valexpr_accesses_vars(const std::unordered_set<std::string>& vars, std::shared_ptr<ValExpr> val_expr);

bool valexpr_list_accesses_vars(
    const std::unordered_set<std::string>& vars, 
    const std::vector<std::shared_ptr<ValExpr>>& val_expr_list) {
    for(std::shared_ptr<ValExpr> val_expr: val_expr_list) {
        if(valexpr_accesses_vars(vars, val_expr)) {
            return true;
        }
    }
    return false;
}
// CR: This is a general comment. Think carefully about the shadowing of the members of an actor

bool valexpr_accesses_vars(const std::unordered_set<std::string>& vars, std::shared_ptr<ValExpr> val_expr) {
    // Returns whether [valexpr] accesses any variable in [vars]
    // CR: Think carefully about do I want to log error messages here when I get to that
    return std::visit(Overload{
        [&](const ValExpr::VVar& var) {
            return vars.find(var.name) != vars.end();
        },
        [&](const ValExpr::VStruct& struct_instance) {
            // CR: Try using advanced C++ features to do this functionally using [valexpr_list_accesses_vars]
            for(const auto&[k, v]: struct_instance.fields) {
                if(valexpr_accesses_vars(vars, v)) {
                    return true;
                }
            }
            return false;
        },
        [&](const ValExpr::NewInstance& new_instance) {
            return valexpr_accesses_vars(vars, new_instance.default_value) || valexpr_accesses_vars(vars, new_instance.size);
        },
        [&](const ValExpr::ActorConstruction& actor_construction) {
            return valexpr_list_accesses_vars(vars, actor_construction.args);
        },
        [&](const ValExpr::PointerAccess& pointer_access) {
            return valexpr_accesses_vars(vars, pointer_access.index) || valexpr_accesses_vars(vars, pointer_access.value);
        },
        [&](const ValExpr::Field& field_access) {
            return valexpr_accesses_vars(vars, field_access.base);
        },
        [&](const ValExpr::Assignment& assignment) {
            return valexpr_accesses_vars(vars, assignment.lhs) || valexpr_accesses_vars(vars, assignment.rhs);
        },
        [&](const ValExpr::FuncCall& func_call) {
            return valexpr_list_accesses_vars(vars, func_call.args);
        },
        [&](const ValExpr::BeCall& be_call) {
            return valexpr_list_accesses_vars(vars, be_call.args);
        },
        [&](const ValExpr::BinOpExpr& bin_op_expr) {
            return valexpr_accesses_vars(vars, bin_op_expr.lhs) || valexpr_accesses_vars(vars, bin_op_expr.rhs);
        },
        [&](const auto&) { return false; }
    }, val_expr->t);
}

bool valexpr_accesses_uninitialized(std::unordered_set<std::string>& unassigned_members, std::shared_ptr<ValExpr> val_expr) {
    // This is a wrapper around [valexpr_accesses_vars] to add the [this] variable when necessary
    assert(unassigned_members.find("this") == unassigned_members.end());
    if(unassigned_members.size() != 0) {
        unassigned_members.insert("this");
    }
    bool result = valexpr_accesses_vars(unassigned_members, val_expr);
    unassigned_members.erase("this");
    return result;
}

// NOTE: All these functions are very specific to the constructor case.
std::optional<std::unordered_set<std::string>> new_assigned_var_in_stmt_list(
    TypeEnv& env, 
    std::unordered_set<std::string>& unassigned_members, 
    const std::vector<std::shared_ptr<Stmt>>& stmt_list);
std::optional<std::unordered_set<std::string>> new_assigned_variable_in_stmt(
    TypeEnv& env, 
    std::unordered_set<std::string>& unassigned_members, 
    std::shared_ptr<Stmt> stmt) {
    // returns the newly assigned variables in the statement. If it encounters a violation, it logs
    // and returns std::nullopt
    std::unordered_set<std::string> new_assigned_var;
    return std::visit(Overload{
        [&](const Stmt::VarDeclWithInit&) -> std::optional<std::unordered_set<std::string>> {
            return new_assigned_var;
        },
        [&](const Stmt::MemberInitialize& mem_init) -> std::optional<std::unordered_set<std::string>> {
            if(unassigned_members.find(mem_init.member_name) != unassigned_members.end()) {
                new_assigned_var.emplace(mem_init.member_name);
            }
            return new_assigned_var;
        },
        [&](const Stmt::Expr& expr) -> std::optional<std::unordered_set<std::string>> {
            if(valexpr_accesses_uninitialized(unassigned_members, expr.expr)) {
                return std::nullopt;
            }
            return new_assigned_var;
        },
        [&](const Stmt::If& if_expr) -> std::optional<std::unordered_set<std::string>> {
            if(valexpr_accesses_uninitialized(unassigned_members, if_expr.cond)) {
                return std::nullopt;
            }
            auto then_block_assigned_vars = new_assigned_var_in_stmt_list(env, unassigned_members, if_expr.then_body);
            if(!then_block_assigned_vars) {
                return std::nullopt;
            }
            if(!if_expr.else_body) {
                return new_assigned_var;
            }
            auto else_block_assigned_vars = new_assigned_var_in_stmt_list(env, unassigned_members, *if_expr.else_body);
            if(!else_block_assigned_vars) {
                return std::nullopt;
            }
            // Now need to take the intersection to be conservative
            for(const auto& var: *then_block_assigned_vars) {
                if(else_block_assigned_vars->find(var) != else_block_assigned_vars->end()) {
                    new_assigned_var.emplace(var);
                }
            }
            return new_assigned_var;
        },
        [&](const Stmt::While& while_expr) -> std::optional<std::unordered_set<std::string>> {
            // TODO hard thing need to implement
            if(valexpr_accesses_uninitialized(unassigned_members, while_expr.cond)) {
                return std::nullopt;
            }
            return new_assigned_var_in_stmt_list(env, unassigned_members, while_expr.body);
        },
        [&](std::shared_ptr<Stmt::Atomic> atomic_expr) -> std::optional<std::unordered_set<std::string>> {
            return new_assigned_var_in_stmt_list(env, unassigned_members, atomic_expr->body);
        },
        [&](const Stmt::Return& return_expr) -> std::optional<std::unordered_set<std::string>> {
            if(valexpr_accesses_uninitialized(unassigned_members, return_expr.expr)) {
                return std::nullopt;
            }
            return new_assigned_var;
        }
    }, stmt->t);
}

std::optional<std::unordered_set<std::string>> new_assigned_var_in_stmt_list(
    TypeEnv& env, 
    std::unordered_set<std::string>& unassigned_members, 
    const std::vector<std::shared_ptr<Stmt>>& stmt_list) {
    std::unordered_set<std::string> curr_unassigned_members = unassigned_members;
    std::unordered_set<std::string> newly_assigned_members;
    for(auto stmt: stmt_list) {
        auto new_assigned_var = new_assigned_variable_in_stmt(env, curr_unassigned_members, stmt);
        if(!new_assigned_var) {
            return std::nullopt;
        }
        for(const std::string& var: *new_assigned_var) {
            assert(curr_unassigned_members.find(var) != curr_unassigned_members.end());
            curr_unassigned_members.erase(var);
            newly_assigned_members.emplace(var);
        }
    }
    return newly_assigned_members;
}


bool type_check_constructor(TypeEnv& env, std::shared_ptr<TopLevelItem::Constructor> constructor_def) {
    // Creating a scope for the constructor

    ScopeGuard guard(env.var_context, constructor_def);
    if(!add_arguments_to_scope(env, constructor_def->params)) {
        return false;
    }
    // The rule is that you cannot use a member variable unless it has been assigned, and you cannot use the
    // [this] member until all members have been assigned

    // First we do a simple top-level check not doing an analysis into the initialization stuff
    if(!type_check_stmt_list(env, constructor_def->body)) {
        return false;
    }
    // After this is done, we do a recursive check of the initialization
    std::unordered_set<std::string> unassigned_members;
    auto enclosing_actor = env.var_context.get_enclosing_actor();
    assert(enclosing_actor != nullptr);
    for(const auto& [k, v]: enclosing_actor->member_vars) {
        assert(unassigned_members.find(k) == unassigned_members.end());
        unassigned_members.insert(k);
    }
    auto assigned_vars = new_assigned_var_in_stmt_list(env, unassigned_members, constructor_def->body);
    if(!assigned_vars) {
        return false;
    }
    bool all_assigned = true;
    for(const auto& k: unassigned_members) {
        if(assigned_vars->find(k) == assigned_vars->end()) {
            all_assigned = false;
        }
    }
    return all_assigned;
}