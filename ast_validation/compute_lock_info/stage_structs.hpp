#pragma once
#include "top_level.hpp"
#include "general_validator_structs.hpp"

using CallableVariant = 
    std::variant<
        std::shared_ptr<TopLevelItem::Func>, 
        std::shared_ptr<TopLevelItem::Constructor>>;

struct SyncCallable {
    CallableVariant callable;
    std::shared_ptr<TopLevelItem::Actor> curr_actor;
    bool operator==(const SyncCallable& other) const {
        return std::visit([](auto const& x, auto const& y){
            return static_cast<const void*>(x.get()) == static_cast<const void*>(y.get());
        }, this->callable, other.callable);
    }
};

namespace std {
    template<>
    struct hash<SyncCallable> {
        size_t operator()(const SyncCallable& sync_callable) const {
            return std::visit(
                [&](auto callable) {
                    return std::hash<const void*>{}(callable.get());
                }
            , sync_callable.callable);
        }
    };
}


using CallableGraph = 
    std::unordered_map<SyncCallable, std::unordered_set<SyncCallable>>;

struct LockInfoEnv {
    std::shared_ptr<std::unordered_set<std::string>> locks_dereferenced;
    std::shared_ptr<TopLevelItem::Actor> curr_actor;
    std::shared_ptr<DeclCollection> decl_collection;
};