#include "string_utils.hpp"

template <typename T>
void map_emit_list(
    std::ostream& output_file, 
    const std::vector<T>& ele_list,
    const std::string separator,
    std::function<std::string(T)> string_gen) {
    for(size_t i = 0; i < ele_list.size(); i++) {
        output_file << string_gen(ele_list[i]);
        if(i + 1 != ele_list.size()) {
            output_file << separator;
        }
    }
}

template <typename T>
void map_emit_struct(
    std::ostream& output_file,
    const std::string& struct_name,
    const std::vector<T>& ele_list,
    std::function<std::string(T)> string_gen) {
    output_file << struct_name << " = " << "type { ";
    map_emit_list<T>(
        output_file, 
        ele_list, 
        ", ", 
        string_gen
    );
    output_file << " }" << std::endl;
}

template <typename T>
void map_emit_llvm_function_sig(
    std::ostream& output_file,
    const std::string& function_name,
    const std::string& llvm_return_type,
    const std::vector<T>& parameters,
    std::function<std::string(T)> llvm_param_gen) {
    output_file << "define " << llvm_return_type << " " << "@" << function_name << "(";
    map_emit_list<T>(
        output_file, 
        parameters, 
        ", ", 
        llvm_param_gen
    );
    output_file << ")";
}
