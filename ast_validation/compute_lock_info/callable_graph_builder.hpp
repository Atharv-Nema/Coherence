#pragma once
#include "stage_structs.hpp"
#include "general_validator_structs.hpp"

// Builds the call graph
std::shared_ptr<CallableGraph> build_graph(Program* root, std::shared_ptr<DeclCollection> decl_collection);