#pragma once
#include "generator_state.hpp"

// Generates all of the structs needed for user defined types, actors and behaviours
// and also collects any type information to [gen_state.type_name_info_map]
void generate_llvm_structs(GenState& gen_state, Program* program_ast);