#pragma once
#include "top_level.hpp"
#include <unordered_map>
#include <unordered_set>
#include <assert.h>
#include "scoped_store.cpp"
#include "pattern_matching_boilerplate.hpp"

class VarContext {
public:
    using Callable = std::variant<
        std::monostate,
        std::shared_ptr<TopLevelItem::Func>,
        std::shared_ptr<TopLevelItem::Behaviour>,
        std::shared_ptr<TopLevelItem::Constructor>
    >;
private:
    struct ScopeMetadata {
        std::shared_ptr<Stmt::Atomic> atomic_section;
        std::shared_ptr<TopLevelItem::Actor> curr_actor; // For the member variables of the actor
        Callable curr_callable;
        // std::shared_ptr<TopLevelItem::Func> curr_func; // For adding any lock info here
        ScopeMetadata(std::shared_ptr<Stmt::Atomic> atomic_section, 
              std::shared_ptr<TopLevelItem::Actor> curr_actor,
              Callable curr_callable
            ): atomic_section(atomic_section), curr_actor(curr_actor), curr_callable(curr_callable) {}
    };
    ScopedStore<std::string, FullType, ScopeMetadata> var_type_map;
public:
    VarContext() {}
    void create_new_scope(
        std::shared_ptr<Stmt::Atomic> atomic_section, 
        std::shared_ptr<TopLevelItem::Actor> curr_actor,
        Callable curr_callable
    ) { 
        var_type_map.create_new_scope(atomic_section, curr_actor, curr_callable);
    }
    void create_new_scope() {
        // Basically just use the same stuff as the previous scope
        auto prev_scope_metadata = var_type_map.get_current_scope_metadata();
        create_new_scope(prev_scope_metadata.atomic_section, prev_scope_metadata.curr_actor, prev_scope_metadata.curr_callable);
    }
    void create_new_scope(std::shared_ptr<Stmt::Atomic> atomic_section) {
        // Basically just use the same stuff as the previous scope
        auto& prev_scope_metadata = var_type_map.get_current_scope_metadata();
        create_new_scope(atomic_section, prev_scope_metadata.curr_actor, prev_scope_metadata.curr_callable);
    }
    void create_new_scope(Callable curr_callable) {
        // Basically just use the same stuff as the previous scope
        std::shared_ptr<TopLevelItem::Actor> curr_actor = nullptr;
        if(var_type_map.num_scopes() != 0) {
            curr_actor = var_type_map.get_current_scope_metadata().curr_actor;
        }
        create_new_scope(nullptr, curr_actor, curr_callable);
    }

    void add_lock(const std::string& lock) {
        ScopeMetadata& curr_scope_metadata = var_type_map.get_current_scope_metadata();
        assert(curr_scope_metadata.atomic_section != nullptr);
        curr_scope_metadata.atomic_section->locks_dereferenced.emplace(lock);
        std::visit(Overload{
            [&](std::shared_ptr<TopLevelItem::Func> func_def) {
                func_def->locks_dereferenced.emplace(lock);
            },
            [&](std::shared_ptr<TopLevelItem::Constructor> constructor_def) {
                constructor_def->locks_dereferenced.emplace(lock);
            },
            [&](auto) {}
        }, curr_scope_metadata.curr_callable);
    }

    std::optional<FullType> get_expected_function_return_type() {
        assert(var_type_map.num_scopes() > 0);
        ScopeMetadata& curr_scope_metadata = var_type_map.get_current_scope_metadata();
        if(auto* func_def = std::get_if<std::shared_ptr<TopLevelItem::Func>>(&curr_scope_metadata.curr_callable)) {
            return (*func_def)->return_type;
        }
        return std::nullopt;
    }

    bool in_atomic_section() { 
        ScopeMetadata& curr_scope_metadata = var_type_map.get_current_scope_metadata();
        return curr_scope_metadata.atomic_section != nullptr; 
    }

    bool variable_overridable_in_scope(const std::string& var_name) {
        // CR: For now, it should not override any actor member or any variable in the local scope
        return !(var_type_map.key_in_curr_scope(var_name)) && !(get_actor_member(var_name));
    }

    std::shared_ptr<TopLevelItem::Actor> get_enclosing_actor() {
        ScopeMetadata& curr_metadata = var_type_map.get_current_scope_metadata();
        return curr_metadata.curr_actor;
    }

    // creates a copy and stores it in the map
    void insert_variable(const std::string& var_name, const FullType& var_type) {
        // Note that insert assumes that [var_name] is not in the latest scope. 
        // Shadowing is allowed. However, multiple declarations in the same as
        assert(var_type_map.num_scopes() > 0);
        assert(variable_overridable_in_scope(var_name));
        var_type_map.insert(var_name, var_type);
    }
    std::optional<FullType> get_actor_member(const std::string& var_name) {
        assert(var_type_map.num_scopes() > 0);
        ScopeMetadata& curr_metadata = var_type_map.get_current_scope_metadata();
        auto& actor = curr_metadata.curr_actor;
        if(actor == nullptr) {
            return std::nullopt;
        }
        if(actor->member_vars.find(var_name) == actor->member_vars.end()) {
            return std::nullopt;
        }
        return actor->member_vars[var_name];
    }
    std::optional<FullType> get_local_member(const std::string& var_name) {
        ScopeMetadata& curr_metadata = var_type_map.get_current_scope_metadata();
        auto local_var_type = var_type_map.get_value(var_name);
        return local_var_type;
    }

    std::optional<FullType> get_variable_type(const std::string& var_name) {
        // First priority for any local variables, then for actor variables
        // Does not perform the logging
        auto local_member = get_local_member(var_name);
        if(local_member) {
            return local_member;
        }
        return get_actor_member(var_name);
    }
    void pop_scope() {
        var_type_map.pop_scope();
    }
};

// map from type variables to the concrete types
using TypeContext = std::unordered_map<std::string, std::shared_ptr<NameableType>>;

// map from function names to their declaration
using FuncNameMap = ScopedStore<std::string, std::shared_ptr<TopLevelItem::Func>>;


struct ActorFrontend {
    std::string actor_name;
    std::unordered_map<std::string, std::shared_ptr<TopLevelItem::Constructor>> constructors;
    std::unordered_map<std::string, std::shared_ptr<TopLevelItem::Behaviour>> member_behaviours;
};

using ActorFrontendMap = std::unordered_map<std::string, std::shared_ptr<ActorFrontend>>;

// More work needs to be done here.
// Need to think about var_context, functions stuff (for atomic and all, how will mutually recursive stuff work though?). Need
// to think about how consume should propogate the context.



struct TypeEnv {
    TypeContext type_context;
    VarContext var_context;
    FuncNameMap func_name_map;
    ActorFrontendMap actor_frontend_map;
};