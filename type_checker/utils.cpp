#include "../ast/expr.hpp"
#include <assert.h>
#include "type_checker.hpp"
#include <functional>
#include "utils.hpp"

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

// std::vector<TopLevelItem::VarDecl

bool passed_in_parameters_valid(TypeEnv& env, const std::vector<TopLevelItem::VarDecl>& signature, const std::vector<std::shared_ptr<ValExpr>>& arguments, bool sendable) {
    std::vector<FullType> expected_types;
    expected_types.reserve(signature.size());
    for(const auto& var_decl: signature) {
        expected_types.push_back(var_decl.type);
    }
    return check_type_expr_list_valid(env, expected_types, arguments, (sendable) ? type_sendable: type_assignable);
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

std::optional<BasicType> dereferenced_type(TypeEnv& env, FullType type) {
    auto pointer = std::get_if<FullType::Pointer>(&type.t);
    if(!pointer) {
        return std::nullopt;
    }
    return pointer->base;
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