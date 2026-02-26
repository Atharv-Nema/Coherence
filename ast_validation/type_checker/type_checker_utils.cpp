#include "type_checker_utils.hpp"
#include "pattern_matching_boilerplate.hpp"
#include <cassert>
#include <functional>
#include "debug_printer.hpp"

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

std::optional<Cap> viewpoint_adaptation_op(std::optional<Cap> outer_view, std::optional<Cap> inner_view) {
    if(outer_view == std::nullopt) {
        return inner_view;
    }
    if(inner_view == std::nullopt) {
        return outer_view;
    }
    return std::visit(Overload{
        // Nonsensical case: Viewing an iso_cap
        [](const auto&, const Cap::Iso_cap&) -> std::optional<Cap> { 
            assert(false); 
            return Cap{Cap::Tag{}};},
        
        // Tag
        [](const Cap::Tag&, const Cap::Tag&) -> std::optional<Cap> { 
            return Cap{Cap::Tag{}}; 
        },
        [](const Cap::Tag&, const Cap::Ref&) -> std::optional<Cap> { 
            return Cap{Cap::Tag{}}; 
        },
        [](const Cap::Tag&, const Cap::Val&) -> std::optional<Cap> {
            return Cap{Cap::Tag{}};
        },
        [](const Cap::Tag&, const Cap::Iso&) -> std::optional<Cap> { 
            return Cap{Cap::Tag{}}; 
        },
        [](const Cap::Tag&, const Cap::Locked&) -> std::optional<Cap> {
             return Cap{Cap::Tag{}}; 
        },

        // Viewing through ref
        [](const Cap::Ref&, const Cap::Tag&) -> std::optional<Cap> {
            return Cap{Cap::Tag{}}; 
        },
        [](const Cap::Ref&, const Cap::Ref&) -> std::optional<Cap> { 
            return Cap{Cap::Ref{}}; 
        },
        [](const Cap::Ref&, const Cap::Val&) -> std::optional<Cap> { 
            return Cap{Cap::Val{}}; 
        },
        [](const Cap::Ref&, const Cap::Iso&) -> std::optional<Cap> { 
            return Cap{Cap::Iso{}}; 
        },
        [&](const Cap::Ref&, const Cap::Locked&) -> std::optional<Cap> {
            return inner_view; 
        },

        // Viewing through val
        [](const Cap::Val&, const Cap::Tag&) -> std::optional<Cap> {
            return Cap{Cap::Tag{}}; 
        },
        [](const Cap::Val&, const Cap::Ref&) -> std::optional<Cap> {
            return Cap{Cap::Val{}}; 
        },
        [](const Cap::Val&, const Cap::Val&) -> std::optional<Cap> { 
            return Cap{Cap::Val{}}; 
        },
        [](const Cap::Val&, const Cap::Iso&) -> std::optional<Cap> { 
            return Cap{Cap::Val{}}; 
        },
        [&](const Cap::Val&, const Cap::Locked&) -> std::optional<Cap> { 
            return inner_view; 
        },

        // Viewing through iso
        [](const Cap::Iso&, const Cap::Tag&) -> std::optional<Cap> { 
            return Cap{Cap::Tag{}}; 
        },
        [](const Cap::Iso&, const Cap::Ref&) -> std::optional<Cap> { 
            // CR: Potentially [Iso]???
            return Cap{Cap::Iso{}}; 
        },
        [](const Cap::Iso&, const Cap::Val&) -> std::optional<Cap> { 
            return Cap{Cap::Val{}}; 
        },
        [](const Cap::Iso&, const Cap::Iso&) -> std::optional<Cap> { 
            return Cap{Cap::Iso{}}; 
        },
        [&](const Cap::Iso&, const Cap::Locked&) -> std::optional<Cap> { 
            return inner_view; 
        },

        // Viewing through unaliased reference
        [](const Cap::Iso_cap&, const Cap::Tag&) -> std::optional<Cap> { 
            return Cap{Cap::Tag{}}; 
        },
        [](const Cap::Iso_cap&, const Cap::Ref&) -> std::optional<Cap> { 
            return Cap{Cap::Iso_cap{}}; 
        },
        [](const Cap::Iso_cap&, const Cap::Val&) -> std::optional<Cap> { 
            return Cap{Cap::Val{}}; 
        },
        [](const Cap::Iso_cap&, const Cap::Iso&) -> std::optional<Cap> { 
            return Cap{Cap::Iso_cap{}}; 
        },
        [&](const Cap::Iso_cap&, const Cap::Locked&) -> std::optional<Cap> { 
            return inner_view; 
        },

        [](const Cap::Locked&, const Cap::Tag&) -> std::optional<Cap> { 
            return Cap{Cap::Tag{}}; 
        },
        [&](const Cap::Locked&, const Cap::Ref&) -> std::optional<Cap> { 
            return outer_view; 
        },
        [](const Cap::Locked&, const Cap::Val&) -> std::optional<Cap> { 
            return Cap{Cap::Val{}}; 
        },
        [](const Cap::Locked&, const Cap::Iso&) -> std::optional<Cap> { 
            return Cap{Cap::Iso{}}; 
        },
        [&](const Cap::Locked&, const Cap::Locked&) -> std::optional<Cap> { 
            return inner_view; 
        }
    }, outer_view.value().t, inner_view.value().t);
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
        [](const Cap::Tag&, const auto&) {return true;},
        [](const auto&, const auto&) { return false; }
    }, c1.t, c2.t);
}

bool capability_shareable(Cap cap) {
    return std::visit(Overload{
        [](const Cap::Tag&) {return true;},
        [](const Cap::Ref&) {return false;},
        [](const Cap::Val&) {return true;},
        [](const Cap::Iso&) {return true;},
        [](const Cap::Iso_cap&) {assert(false); return false;},
        [](const Cap::Locked&) {return true;}
    }, cap.t);
}

bool capability_mutable(Cap c) {
    return std::visit(Overload{
        [](const Cap::Tag&) {return false;},
        [](const Cap::Val&) {return false;},
        [](const auto&) {return true;}
    }, c.t);
}

bool capability_subtype(Cap c1, Cap c2) {
    if(capabilities_assignable(c1, c2)) {
        return true;
    }
    if(std::holds_alternative<Cap::Iso>(c1.t) && 
       std::holds_alternative<Cap::Iso>(c2.t)) {
        return true;
    }
    return false;
}

// CR: Make the theory more robust
bool capability_aliasable(Cap c1, Cap c2) {
    if(std::holds_alternative<Cap::Iso>(c1.t) &&
       std::holds_alternative<Cap::Iso_cap>(c2.t)) {
        return true;
    }
    return capability_subtype(c1, c2) && capability_subtype(c2, c1);
}

// Takes in a [type]. If [type] is a pointer, it returns the type corresponding to the dereference of 
// it. Otherwise, returns [nullptr] 
std::shared_ptr<const Type> get_dereferenced_type(std::shared_ptr<const Type> type) {
    // It should not modify Type basically
    auto* ptr_type = std::get_if<Type::Pointer>(&type->t);
    if(ptr_type == nullptr) {
        return nullptr;
    }
    std::optional<Cap> deref_view = viewpoint_adaptation_op(
        viewpoint_adaptation_op(type->viewpoint, ptr_type->cap), ptr_type->base_type->viewpoint);
    return std::make_shared<const Type>(Type{
        ptr_type->base_type->t, deref_view});
}

std::shared_ptr<const Type> apply_viewpoint_to_type(std::optional<Cap> viewpoint, std::shared_ptr<const Type> type) {
    std::optional<Cap> adapted_viewpoint = viewpoint_adaptation_op(viewpoint, type->viewpoint);
    return std::make_shared<const Type>(Type{
        type->t, adapted_viewpoint});
}



// !!!New stuff!!!
std::shared_ptr<const Type> get_type_of_nameable(std::shared_ptr<NameableType> nameable_type) {
    assert(nameable_type != nullptr);
    if(!std::holds_alternative<std::shared_ptr<const Type>>(nameable_type->t)) {
        return nullptr;
    }
    return std::get<std::shared_ptr<const Type>>(nameable_type->t);
}

// Returns nullptr if the type name has not been defined yet
std::shared_ptr<const Type> get_standardized_type_from_name(
    TypeContext& type_context, 
    std::optional<Cap> viewpoint, 
    const std::string& type_name) {
    if(type_context.find(type_name) == type_context.end()) {
        return nullptr;
    }
    std::shared_ptr<NameableType> nameable_type = type_context.at(type_name);
    std::shared_ptr<const Type> aliased_type = get_type_of_nameable(nameable_type);
    if(aliased_type == nullptr) {
        return std::make_shared<const Type>(Type{Type::TNamed{type_name}, viewpoint});
    }
    else {
        if(auto* named_type = std::get_if<Type::TNamed>(&aliased_type->t)) {
            std::optional<Cap> new_viewpoint = viewpoint_adaptation_op(viewpoint, aliased_type->viewpoint);
            return get_standardized_type_from_name(type_context, new_viewpoint, named_type->name);
        }
        else {
            return apply_viewpoint_to_type(viewpoint, aliased_type); 
        }
    }
}

std::shared_ptr<const Type> get_standardized_type(
    TypeContext& type_context,
    std::shared_ptr<const Type> type) {
    return std::visit(Overload{
        [&](const Type::TNamed& t_named) {
            return get_standardized_type_from_name(type_context, type->viewpoint, t_named.name);
        },
        [&](const auto&) {
            return type;
        }
    }, type->t);
}



// CR: Think carefully about the copying going on
std::optional<NameableType::Struct> get_struct_type(TypeContext& type_context, const std::string& struct_type_name) {
    std::shared_ptr<const Type> standard_struct_type = 
        get_standardized_type_from_name(type_context, std::nullopt, struct_type_name);
    if(standard_struct_type == nullptr) {
        return std::nullopt;
    }
    auto named_type = std::get_if<Type::TNamed>(&standard_struct_type->t);
    if(!named_type) {
        return std::nullopt;
    }
    const std::string& standardized_type_name = named_type->name;
    auto struct_type = type_context.at(standardized_type_name);
    const auto& struct_contents = std::get<NameableType::Struct>(struct_type->t);
    return struct_contents;
}


bool type_is_printable(TypeContext &type_context, std::shared_ptr<const Type> type) {
    return std::visit(Overload{
        [&](const Type::TInt&) {return true;},
        [&](const Type::TNamed& named_type) {
            auto standard_type = get_standardized_type_from_name(type_context, type->viewpoint, named_type.name);
            if(standard_type == nullptr) {
                return false;
            }
            return std::holds_alternative<Type::TInt>(standard_type->t);
        },
        [&](const auto&) {return false;}
    }, type->t);
}

bool type_is_int(TypeContext& type_context, std::shared_ptr<const Type> type) {
    type = get_standardized_type(type_context, type);
    return std::holds_alternative<Type::TInt>(type->t);
}

bool type_is_bool(TypeContext& type_context, std::shared_ptr<const Type> type) {
    type = get_standardized_type(type_context, type);
    return std::holds_alternative<Type::TBool>(type->t);
}

bool type_rel_comparable(
    TypeContext& type_context, 
    std::shared_ptr<const Type> type_1, 
    std::shared_ptr<const Type> type_2) {
    type_1 = get_standardized_type(type_context, type_1);
    type_2 = get_standardized_type(type_context, type_2);
    return std::visit(Overload{
        [](const Type::TInt&, const Type::TInt&) {return true;},
        [](const auto&, const auto&) {return false;}
    }, type_1->t, type_2->t);
}

bool type_equality_comparable(
    TypeContext& type_context, 
    std::shared_ptr<const Type> type_1, 
    std::shared_ptr<const Type> type_2) {
    type_1 = get_standardized_type(type_context, type_1);
    type_2 = get_standardized_type(type_context, type_2);
    return std::visit(Overload{
        [](const Type::TUnit&, const Type::TUnit&) {return true;},
        [](const Type::TInt&, const Type::TInt&) {return true;},
        [](const Type::TBool&, const Type::TBool&) {return true;},
        [](const Type::Pointer&, const Type::Pointer&) {return true;},
        [](const Type::Pointer&, const Type::TNullptr&) {return true;},
        [](const Type::TNullptr&, const Type::Pointer&) {return true;},
        [](const Type::TNullptr&, const Type::TNullptr&) {return true;},
        [](const Type::TActor&, const Type::TActor&) {return true;},
        [](const auto&, const auto&) {return false;}
    }, type_1->t, type_2->t);
}

// This function compares [lhs] and [rhs]. Whenever it reaches pointer types, it applies the
// [cap_compare] on it, and then recursively applies [type_compare] with the lhs_cap on the
// dereferenced types (so that it can apply different [cap_compare] if needed). The interpretation
// of it independently is not that clear. But the point of this is to factor out the common/recursive
// part of covariance, invariance and assignable checkers.
bool pointer_property_compare_template(
    TypeContext& type_context,
    std::shared_ptr<const Type> lhs,
    std::shared_ptr<const Type> rhs,
    std::function<bool(Cap, Cap)> cap_compare,
    std::function<bool(
        Cap, std::shared_ptr<const Type>, std::shared_ptr<const Type>)> type_compare) {
    lhs = get_standardized_type(type_context, lhs);
    rhs = get_standardized_type(type_context, rhs);
    return std::visit(Overload{
        [&](const Type::TUnit&, const Type::TUnit&) {
            return true;
        },
        [&](const Type::TInt&, const Type::TInt&) {
            return true;
        },
        [&](const Type::TBool&, const Type::TBool&) {
            return true;
        },
        [&](const Type::TActor& actor_lhs, const Type::TActor& actor_rhs) {
            return actor_lhs.name == actor_rhs.name;
        },
        [&](const Type::TNamed& named_lhs, const Type::TNamed& named_rhs) {
            if(named_lhs.name != named_rhs.name) {
                return false;
            }
            NameableType::Struct struct_type = get_struct_type(type_context, named_lhs.name).value();
            for(size_t i = 0; i < struct_type.members.size(); i++) {
                auto& [mem_name, mem_type] = struct_type.members[i];
                std::shared_ptr<const Type> viewed_lhs_type = 
                    apply_viewpoint_to_type(lhs->viewpoint, mem_type);
                std::shared_ptr<const Type> viewed_rhs_type =
                    apply_viewpoint_to_type(rhs->viewpoint, mem_type);
                if(!pointer_property_compare_template(type_context, viewed_lhs_type, viewed_rhs_type, cap_compare, type_compare)) {
                    return false;
                }
            }
            return true;
        },
        [&](const Type::TNamed& named_lhs, const auto&) {
            return false;
        },
        [&](const auto&, const Type::TNamed& named_rhs) {
            return false;
        },
        [&](const Type::Pointer&, const Type::TNullptr&) {
            return true;
        },
        [&](const Type::Pointer& ptr_lhs, const Type::Pointer& ptr_rhs) {
            // 1. Checking whether the capabilities are assignable
            auto lhs_cap = viewpoint_adaptation_op(lhs->viewpoint, ptr_lhs.cap).value();
            auto rhs_cap = viewpoint_adaptation_op(rhs->viewpoint, ptr_rhs.cap).value();
            if(!cap_compare(lhs_cap, rhs_cap)) {
                return false;
            }
            // 2. In cases of recursive types, a simple recursive check will lead to an infinite
            // loop. To prevent this, we can do an early check on whether the two base types are 
            // named with the same viewpoint. If so, we can simply return true. Note that this
            // is only true for the pointer assignment case. It is questionable whether I should
            // be doing this check here, but it seems like the cleanest solution at the moment.
            bool viewpoint_equal = 
            ((ptr_lhs.base_type->viewpoint == std::nullopt) && (ptr_rhs.base_type->viewpoint == std::nullopt)) || 
            (ref_cap_equal(ptr_lhs.base_type->viewpoint.value(), ptr_lhs.base_type->viewpoint.value()));
            bool recursive_case = viewpoint_equal && std::visit(Overload{
                    [](const Type::TNamed& lhs_named, const Type::TNamed& rhs_named) {
                        return lhs_named.name == rhs_named.name;
                    },
                    [](const auto&, const auto&) { return false; }
                }, ptr_lhs.base_type->t, ptr_rhs.base_type->t);
            if(recursive_case) {
                return true;
            }

            // 3. Checking whether the dereferenced type make sense. If the outer capability
            // is mutable, the dereferenced type needs to be invariant. Else, it can just be
            // covariant.
            
            auto lhs_deref_type = get_dereferenced_type(lhs);
            auto rhs_deref_type = get_dereferenced_type(rhs);
            assert(lhs_deref_type != nullptr);
            assert(rhs_deref_type != nullptr);
            return type_compare(lhs_cap, lhs_deref_type, rhs_deref_type);
        },
        [&](const auto&, const auto&) {
            return false;
        }
    }, lhs->t, rhs->t);
}

bool type_invariant(
    TypeContext& type_context, 
    std::shared_ptr<const Type> t1,
    std::shared_ptr<const Type> t2) {
    auto curried_invariant = [&](Cap c, std::shared_ptr<const Type> t1, std::shared_ptr<const Type> t2) {
        return type_invariant(type_context, t1, t2);
    };
    return pointer_property_compare_template(type_context, t1, t2, capability_aliasable, curried_invariant);
}

bool type_covariant(
    TypeContext& type_context,
    std::shared_ptr<const Type> t1,
    std::shared_ptr<const Type> t2);

// Rules for when to allow assigning of pointers. t1 and t2 are the types of the dereference of the two pointer
bool pointer_subtyping_on_dereferenced_types(
    TypeContext& type_context,
    Cap pointer_cap,
    std::shared_ptr<const Type> t1,
    std::shared_ptr<const Type> t2) {
    if(capability_mutable(pointer_cap)) {
        // Need to be invariant if [pointer_cap] is mutable
        return type_invariant(type_context, t1, t2);
    }
    else {
        return type_covariant(type_context, t1, t2);
    }
}

bool type_covariant(
    TypeContext& type_context,
    std::shared_ptr<const Type> t1,
    std::shared_ptr<const Type> t2) {
    auto curried_action = [&](Cap c, std::shared_ptr<const Type> t1, std::shared_ptr<const Type> t2) {
        return pointer_subtyping_on_dereferenced_types(type_context, c, t1, t2);
    };
    return pointer_property_compare_template(type_context, t1, t2, capability_subtype, curried_action);
}

bool type_assignable(
    TypeContext& type_context,
    std::shared_ptr<const Type> t1,
    std::shared_ptr<const Type> t2) {
    auto curried_action = [&](Cap c, std::shared_ptr<const Type> t1, std::shared_ptr<const Type> t2) {
        return pointer_subtyping_on_dereferenced_types(type_context, c, t1, t2);
    };
    return pointer_property_compare_template(type_context, t1, t2, capabilities_assignable, curried_action);
}



bool type_shareable(TypeContext& type_context, std::shared_ptr<const Type> type) {
    return std::visit(Overload{
        [&](const Type::TNamed& t_named){
            std::shared_ptr<const Type> standard_type = 
                get_standardized_type_from_name(type_context, type->viewpoint, t_named.name);
            if(!std::holds_alternative<Type::TNamed>(standard_type->t)) {
                return type_shareable(type_context, standard_type);
            }
            std::string struct_name = std::get<Type::TNamed>(standard_type->t).name;
            NameableType::Struct struct_type = get_struct_type(type_context, struct_name).value();
            for(auto& [mem_name, mem_type]: struct_type.members) {
                std::shared_ptr<const Type> viewed_type = 
                    apply_viewpoint_to_type(standard_type->viewpoint, mem_type);
                if(!type_shareable(type_context, viewed_type)) {
                    return false;
                }
            }
            return true;
        },
        [&](const Type::Pointer& pointer) {
            return capability_shareable(viewpoint_adaptation_op(type->viewpoint, pointer.cap).value());
        },
        [](const auto&) {return true;}
    }, type->t);
}

std::shared_ptr<const Type> unaliased_type(std::shared_ptr<const Type> type) {
    return apply_viewpoint_to_type(Cap{Cap::Iso_cap{}}, type);
}