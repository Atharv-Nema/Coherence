#pragma once
#include "../ast/top_level.hpp"
#include <unordered_map>
#include <unordered_set>
#include <assert.h>


template <typename K, typename V>
class ScopedStore {
private:
    // A list of unordered sets
    std::vector<std::unordered_set<K>> scope_list;
    std::unordered_map<K, std::vector<V>> key_val_map;
public:
    ScopedStore() {}
    void create_new_scope() {
        scope_list.emplace_back();
    }
    // creates a copy and stores it in the map
    void insert(const K& key, const V& value) {
        // Note that insert assumes that [key] is not in the latest scope
        assert(scope_list.size() > 0);
        assert(scope_list.back().find(key) == scope_list.back().end());
        scope_list.back().insert(key);
        key_val_map[key].push_back(value);
    }
    std::optional<V> get_value(const K& key) {
        if(key_val_map.find(key) == key_val_map.end()) {
            return std::nullopt;
        }
        assert(key_val_map[key].size() > 0);
        return key_val_map[key].back();
    }
    void pop_scope() {
        assert(scope_list.size() > 0);
        for(const K& k: scope_list.back()) {
            assert(key_val_map.find(k) != key_val_map.end());
            assert(key_val_map[k].size() > 0);
            key_val_map[k].pop_back();
        }
        scope_list.pop_back();
    }
};

// map from type variables to the concrete types
using TypeContext = std::unordered_map<std::string, std::shared_ptr<NameableType>>;

// map from variables to their type
using VarContext = ScopedStore<std::string, FullType>;

// map from function names to their declaration
using FuncNameMap = std::unordered_map<std::string, std::shared_ptr<TopLevelItem::Func>>;

// For now, I will not allow for variable name shadowing. Note that there are no top level variables
using ActorNameMap = std::unordered_map<std::string, std::shared_ptr<TopLevelItem::Actor>>;

// More work needs to be done here.
// Need to think about var_context, functions stuff (for atomic and all, how will mutually recursive stuff work though?). Need
// to think about how consume should propogate the context.



struct TypeEnv {
    TypeContext type_context;
    VarContext var_context;
    FuncNameMap func_name_map;
    ActorNameMap actor_name_map;
};