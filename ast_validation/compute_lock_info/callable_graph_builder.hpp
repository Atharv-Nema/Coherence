#pragma once
#include "callable_graph.hpp"
#include "general_validator_structs.hpp"

std::shared_ptr<CallableGraph> build_graph(Program* root, std::shared_ptr<DeclCollection> decl_collection);