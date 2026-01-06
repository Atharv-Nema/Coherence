#include <iostream>
#include "utils.hpp"
#include <iostream>
#include "defer.cpp"
#include "pattern_matching_boilerplate.hpp"

void report_error_location(const SourceSpan& span) {
    std::cerr << "Error between line " << span.start.line << ", column " << span.start.char_no 
    << " and line " << span.end.line << ", column " << span.end.char_no << std::endl;
}


std::string orig_name(const std::string& s) {
    size_t pos = s.find('.');
    return s.substr(0, pos);
}

std::shared_ptr<TopLevelItem::Func> get_func_def(
    const std::string& func_name, 
    std::shared_ptr<TopLevelItem::Actor> curr_actor,
    std::shared_ptr<DeclCollection> decl_collection) {
    std::shared_ptr<TopLevelItem::Func> func_def = nullptr;
    if(decl_collection->func_name_map.contains(func_name)) {
        func_def = decl_collection->func_name_map.at(func_name);
    }
    if(curr_actor != nullptr) {
        if(decl_collection->actor_frontend_map.contains(curr_actor->name)) {
            std::shared_ptr<ActorFrontend> actor_frontend = decl_collection->actor_frontend_map.at(curr_actor->name);
            if(actor_frontend->member_functions.contains(func_name)) {
                func_def = actor_frontend->member_functions.at(func_name);
            }
        }
    }
    return func_def;
}

// CR: Better name
bool predicate_valexpr_walker(
    std::shared_ptr<ValExpr> val_expr, 
    std::function<bool(std::shared_ptr<ValExpr>)> predicate) {
    return std::visit(Overload{
        [&](ValExpr::VStruct& struct_val) {
            for(auto& [field_name, field_expr]: struct_val.fields) {
                if(!predicate(field_expr)) {
                    return false;
                }
            }
            return true;
        },
        [&](ValExpr::NewInstance& new_instance) {
            return predicate(new_instance.default_value) && predicate(new_instance.size);
        },
        [&](ValExpr::ActorConstruction& actor_construction) {
            for(auto arg: actor_construction.args) {
                if(!predicate(arg)) {
                    return false;
                }
            }
            return true;
        },
        [&](ValExpr::PointerAccess& pointer_access) {
            // First the pointer then the index
            return predicate(pointer_access.value) && predicate(pointer_access.index);
        },
        [&](const ValExpr::Field& field_access) {
            return predicate(field_access.base);
        },
        [&](const ValExpr::Assignment& assignment) {
            return predicate(assignment.lhs) && predicate(assignment.rhs);
        },
        [&](const ValExpr::FuncCall& func_call) {
            for(auto arg: func_call.args) {
                if(!predicate(arg)) {
                    return false;
                }
            }
            return true;
        },
        [&](const ValExpr::BinOpExpr& bin_op_expr) {
            return predicate(bin_op_expr.lhs) && predicate(bin_op_expr.rhs);
        },
        [&](const auto&) {return true;}
    }, val_expr->t);
}