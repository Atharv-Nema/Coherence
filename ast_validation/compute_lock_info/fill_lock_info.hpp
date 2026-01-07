#pragma once
#include "callable_graph.hpp"
#include "general_validator_structs.hpp"

void fill_lock_info(
    SyncCallable sync_callable, 
    std::shared_ptr<DeclCollection> decl_collection);