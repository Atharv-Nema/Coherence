#pragma once
#include <ostream>
#include <vector>
#include <functional>

template <typename T>
void map_emit_list(
    std::ostream& output_file, 
    const std::vector<T>& ele_list,
    const std::string separator,
    std::function<std::string(T)> string_gen);

template <typename T>
void map_emit_struct(
    std::ostream& output_file,
    const std::string& struct_name,
    const std::vector<T>& ele_list,
    std::function<std::string(T)> string_gen);

template <typename T>
void map_emit_llvm_function_sig(
    std::ostream& output_file,
    const std::string& function_name,
    const std::string& llvm_return_type,
    const std::vector<std::string>& hidden_parameters,
    const std::vector<T>& parameters,
    std::function<std::string(T)> llvm_param_gen);