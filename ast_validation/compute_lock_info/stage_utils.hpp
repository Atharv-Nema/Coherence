#include "callable_graph_builder.hpp"
#include "stage_structs.hpp"

std::shared_ptr<std::unordered_set<std::string>> get_callable_locks(SyncCallable sync_callable);

void set_callable_locks(
    SyncCallable sync_callable,
    std::shared_ptr<std::unordered_set<std::string>> locks_dereferenced);

void add_valexpr_lock_info(std::shared_ptr<ValExpr> val_expr, LockInfoEnv& env);