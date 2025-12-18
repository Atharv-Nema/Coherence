#pragma once
#include "top_level.hpp"
#include <unordered_map>
#include <unordered_set>
#include <assert.h>
#include "scoped_store.cpp"
#include "pattern_matching_boilerplate.hpp"

using VarContext = ScopedStore<std::string, FullType>;

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

class AtomicSectionData {
private:
    std::shared_ptr<Stmt::Atomic> enclosing_atomic_section;
    // CR: make the name [enclosing_callable_locks] better
    std::shared_ptr<std::unordered_set<std::string>> enclosing_callable_locks;
public:
    AtomicSectionData(): enclosing_atomic_section(nullptr), enclosing_callable_locks(nullptr) {}
    void add_lock(const std::string& lock) {
        if(enclosing_atomic_section) {
            enclosing_atomic_section->locks_dereferenced.insert(lock);
        }
        if(enclosing_callable_locks) {
            enclosing_callable_locks->insert(lock);
        }
    }
    void add_locks_from_set(std::shared_ptr<std::unordered_set<std::string>> lock_set) {
        assert(lock_set != nullptr);
        for(const std::string& s: *lock_set) {
            add_lock(s);
        }
    }
    bool in_atomic_section() {
        return enclosing_atomic_section != nullptr;
    }
    void enter_atomic_section(std::shared_ptr<Stmt::Atomic> atomic_section) {
        if(!enclosing_atomic_section) {
            enclosing_atomic_section = atomic_section;
        }
    }
    void exit_atomic_section(std::shared_ptr<Stmt::Atomic> atomic_section) {
        if(enclosing_atomic_section == atomic_section) {
            enclosing_atomic_section = nullptr;
        }
    }
    void change_callable(std::shared_ptr<std::unordered_set<std::string>> callable_lock_set) {
        enclosing_atomic_section = nullptr;
        enclosing_callable_locks = callable_lock_set;
    }
    void exit_callable() {
        enclosing_atomic_section = nullptr;
        enclosing_callable_locks = nullptr;
    }
};


struct TypeEnv {
    TypeContext type_context;
    VarContext var_context;
    FuncNameMap func_name_map;
    ActorFrontendMap actor_frontend_map;
    std::shared_ptr<TopLevelItem::Actor> curr_actor;
    std::shared_ptr<TopLevelItem::Func> curr_func;
    AtomicSectionData atomic_section_data;
};