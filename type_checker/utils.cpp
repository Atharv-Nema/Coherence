#include "../ast/expr.hpp"
#include "env.hpp"
#include "overload.hpp"
#include <assert.h>
#include "type_checker.hpp"

template<typename ... Ts>
struct Overload : Ts ... { 
    using Ts::operator() ...;
};
template<class... Ts> Overload(Ts...) -> Overload<Ts...>;

BasicType extract_basic_type(const FullType& full_type) {
    std::visit(Overload{
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
                type_context.emplace(type_name, NameableType {NameableType::Basic {*res}});
            }
            return res;
        }
    }
}

bool can_appear_in_lhs(TypeEnv& env, std::shared_ptr<ValExpr> expr) {
    // Anything that does not involve going through a deref of a val is game I think
    // This assumes that expr is well-typed ig
    return std::visit(Overload{
        [](const ValExpr::VVar&) {return true;},
        [&](const ValExpr::ArrayAccess& array_access) {
            // First, need to check whether the internal pointer of array_access is of an assignable type
            auto arr_type = val_expr_type(env, array_access.value);
            assert(arr_type);
            auto* ptr_type = std::get_if<FullType::Pointer>(&arr_type->t);
            if(!capability_mutable(ptr_type->cap)) {
                return false;
            }
            return can_appear_in_lhs(env, array_access.value);
        },
        [&](const ValExpr::Deref& deref) {
            auto inner_type = val_expr_type(env, deref.inner);
            assert(inner_type);
            auto* ptr_type = std::get_if<FullType::Pointer>(&inner_type->t);
            if(!capability_mutable(ptr_type->cap)) {
                return false;
            }
            return can_appear_in_lhs(env, deref.inner);
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
            return type_is_numeric(type_context, FullType { FullType::Raw {*standard_type}});
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

bool passed_in_parameters_valid(TypeEnv& env, const std::vector<TopLevelItem::VarDecl>& signature, 
    const std::vector<std::shared_ptr<ValExpr>>& arguments) {
    // This works perfectly if overloading is not supported, which is not for now
    if (signature.size() != arguments.size()) {
        return false;
    }
    for (size_t i = 0; i < signature.size(); ++i) {
        // infer type of argument
        std::optional<FullType> arg_type = val_expr_type(env, arguments[i]);
        if(!arg_type) {
            return false;
        }
        const FullType& expected = signature[i].type;

        if (!type_assignable(env.type_context, *arg_type, expected)) {
            return false;
        }
    }
    return true;
}

std::optional<BasicType> dereferenced_type(TypeEnv& env, FullType type) {
    auto pointer = std::get_if<FullType::Pointer>(&type.t);
    if(!pointer) {
        return std::nullopt;
    }
    return pointer->base;
} 