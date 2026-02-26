#include <iostream>
#include "utils.hpp"
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

