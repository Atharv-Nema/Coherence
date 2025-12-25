// Class that generates characters for registers to be used somehow
#pragma once
#include <cstddef>
#include <string>
#include <unordered_map>
#include <memory>
#include <ostream>
#include "top_level.hpp"
#include "scoped_store.cpp"

class RegisterLabelGen {
private:
    size_t stack_var_reg_ct = 0;
    size_t temp_reg_ct = 0;
    size_t label_ct = 0;
public:
    void refresh_counters() {
        stack_var_reg_ct = 0;
        temp_reg_ct = 0;
        label_ct = 0;
    }
    std::string new_stack_var() {
        stack_var_reg_ct++;
        return std::to_string(stack_var_reg_ct);
    }
    std::string new_temp_reg() {
        temp_reg_ct++;
        return "temp." + std::to_string(temp_reg_ct);
    }
    std::string new_label() {
        label_ct++;
        return "label." + std::to_string(label_ct);
    }
};

struct LLVMTypeInfo;

struct LLVMStructInfo {
    struct FieldInfo {
        size_t field_index;
        std::shared_ptr<LLVMTypeInfo> field_type;
    };
    // For fast access of field_name -> type + index to use in getelementptr
    std::unordered_map<std::string, FieldInfo> field_ind_map;
    // For fast access of index -> type (for function calls to find out the things)
    std::vector<FieldInfo> ind_field_map;
};

struct LLVMTypeInfo {
    std::string llvm_type_name;
    // [struct_info] is optional
    std::shared_ptr<LLVMStructInfo> struct_info;
};


struct GenState {
    RegisterLabelGen reg_label_gen;
    std::unordered_map<std::string, std::string> var_reg_mapping;
    // A map from the user function name to the generated llvm function name. The associated metadata
    // corresponds to the actor it is in.
    ScopedStore<std::string, std::string> func_llvm_name_map;
    // Need this to figure out what string needs to be inserted while doing load and other stuff
    // CR: Slightly suspicious. The only purpose is to map user defined types
    std::unordered_map<std::string, std::shared_ptr<LLVMTypeInfo>> type_name_info_map;
    // To differentiate between different types of callables, I do:
    // 1. For function, append .fun to the name
    // 2. For behaviours, append .be_<actor type name> to the name
    // 3. For constructors, append .actor_type_name>.constructor to the name
    // These should also be the llvm_function_name. So no need for llvm_callable_info actually
    // The struct has name %llvm_function_name.param
    // Also, the struct associated with the actor is %actor_name.struct
    std::shared_ptr<TopLevelItem::Actor> curr_actor;
    std::unordered_map<std::string, uint64_t> lock_id_map;
    // File to which llvm needs to be written to
    std::ostream output_file;
};