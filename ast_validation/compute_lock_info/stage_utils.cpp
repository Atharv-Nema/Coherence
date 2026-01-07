#include "stage_utils.hpp"

std::shared_ptr<std::unordered_set<std::string>> get_callable_locks(SyncCallable sync_callable) {
    return std::visit(
        [](const auto& callable) {
            return callable->locks_dereferenced;
        }, sync_callable.callable);
}

void set_callable_locks(
    SyncCallable sync_callable,
    std::shared_ptr<std::unordered_set<std::string>> locks_dereferenced) {
    std::visit(
        [&](const auto& callable) {
            callable->locks_dereferenced = locks_dereferenced;
        }, sync_callable.callable);
}