#pragma once
#include <string>
#include "generator_state.hpp"
#include "top_level.hpp"
#include "runtime_traps.hpp"

std::string llvm_name_of_func(GenState& gen_state, const std::string& func_name);
std::string llvm_name_of_constructor(
    const std::string& constructor_name, 
    const std::string& actor_name);

std::string llvm_name_of_behaviour(const std::string& be_name, const std::string& actor_name);
std::string llvm_struct_of_behaviour(const std::string& be_name, const std::string& actor_name);
std::string llvm_struct_of_actor(std::shared_ptr<TopLevelItem::Actor> actor_def);
std::shared_ptr<LLVMTypeInfo> llvm_type_of_basic_type(
    GenState& gen_state,
    BasicType basic_type);
std::shared_ptr<LLVMTypeInfo> llvm_type_of_full_type(
    GenState& gen_state,
    FullType full_type);
std::string actor_name_of_full_type(FullType full_type);
std::string get_llvm_type_size(GenState &gen_state, const std::string& llvm_type_name);
void allocate_var_to_stack(
    GenState& gen_state,
    const std::string& llvm_type,
    const std::string& var_name);
std::string convert_i32_to_i64(GenState& gen_state, const std::string& i32_reg);
void branch_label(GenState& gen_state, const std::string& label);
void generate_suspend_call(
    GenState& gen_state,
    SuspendTag suspend_tag);