#include "codegen_utils.hpp"
#include "pattern_matching_boilerplate.hpp"

std::string llvm_name_of_func(GenState& gen_state, const std::string& func_name) {
    if(gen_state.curr_actor != nullptr) {
        return func_name + "." + gen_state.curr_actor->name + ".func";
    }
    else {
        return func_name + ".func";
    }
}

std::string llvm_name_of_constructor(
    const std::string& constructor_name, 
    const std::string& actor_name) {
    return constructor_name + "." + actor_name + ".constr";
}

std::string llvm_name_of_behaviour(const std::string& be_name, const std::string& actor_name) {
    return be_name + "." + actor_name + ".be";
}

std::string llvm_struct_of_behaviour(const std::string& be_name, const std::string& actor_name) {
    return be_name + "." + actor_name + ".be.struct";
}

std::string llvm_struct_of_actor(std::shared_ptr<TopLevelItem::Actor> actor_def) {
    return "%" + actor_def->name + ".actor.struct";
}

std::shared_ptr<LLVMTypeInfo> llvm_type_of_basic_type(
    GenState& gen_state,
    BasicType basic_type) {
    return std::visit(Overload{
        [&](const BasicType::TUnit&) {
            // Using the fact that [] on unordered_maps automatically calls the
            // default constructor on the value type if the key is not present
            return std::make_shared<LLVMTypeInfo>("i1");
        },
        [&](const BasicType::TInt&) {
            return std::make_shared<LLVMTypeInfo>("i32");
        },
        [&](const BasicType::TBool&) {
            return std::make_shared<LLVMTypeInfo>("i1");
        },
        [&](const BasicType::TNamed& t_named) {
            return gen_state.type_name_info_map[t_named.name];
        },
        [&](const BasicType::TActor&) {
            return std::make_shared<LLVMTypeInfo>("i64");
        }
    }, basic_type.t);
}

std::shared_ptr<LLVMTypeInfo> llvm_type_of_full_type(
    GenState& gen_state,
    FullType full_type) {
    return std::visit(Overload{
        [&](BasicType basic_type) {
            // Using the fact that [] on unordered_maps automatically calls the
            // default constructor on the value type if the key is not present
            return llvm_type_of_basic_type(gen_state, basic_type);
        },
        [&](FullType::Pointer) {
            return std::make_shared<LLVMTypeInfo>("ptr");
        }
    }, full_type.t);
}

std::string actor_name_of_full_type(FullType full_type) {
    return std::get<BasicType::TActor>(std::get<BasicType>(full_type.t).t).name;
}

std::string get_llvm_type_size(GenState &gen_state, const std::string& llvm_type_name) {
    // %szptr = getelementptr T, ptr null, i64 1
    // %sz = ptrtoint ptr %szptr to i64
    std::string size_ptr_reg = gen_state.reg_label_gen.new_temp_reg();
    gen_state.output_file << "%" << size_ptr_reg << " = getlementptr " << llvm_type_name
    << ", ptr null, i64 1" << std::endl;
    std::string size_reg = gen_state.reg_label_gen.new_temp_reg();
    gen_state.output_file << "%" << size_reg << " = ptrtoint ptr " << "%" << size_ptr_reg 
    << " to i64" << std::endl;
    return size_reg;
}

void allocate_var_to_stack(
    GenState& gen_state,
    const std::string& llvm_type,
    const std::string& var_name) {
    std::string stack_reg = gen_state.reg_label_gen.new_stack_var();
        gen_state.output_file << "%" << stack_reg << " = alloca " 
        << var_name << std::endl;
    gen_state.var_reg_mapping.emplace(var_name, stack_reg);
}

std::string convert_i32_to_i64(GenState& gen_state, const std::string& i32_reg) {
    std::string size64_reg = gen_state.reg_label_gen.new_temp_reg();
    // %size64_reg = zext i32 %size_val_reg_rval to i64
    gen_state.output_file << "%" << size64_reg << " = zext i32 " << "%" 
    << i32_reg << " to i64" << std::endl;
    return size64_reg;
}

void branch_label(GenState& gen_state, const std::string& label) {
    gen_state.output_file << "br label " << "%" + label << std::endl;
}

void generate_suspend_call(
    GenState& gen_state,
    SuspendTag suspend_tag) {
    // struct name is %SuspendTag.runtime
    // Creating the [SuspendTag] struct on the heap
    std::string llvm_struct_type_name = "%SuspendTag.runtime";
    std::string suspend_struct_size = get_llvm_type_size(gen_state, llvm_struct_type_name);
    std::string suspend_struct_ptr_reg = gen_state.reg_label_gen.new_temp_reg();
    gen_state.output_file << "%" << suspend_struct_ptr_reg << " = call ptr @malloc(i64 " << 
    "%" << suspend_struct_size << ")" << std::endl;

    // Getting the pointer to the kind field
    std::string kind_ptr_reg = gen_state.reg_label_gen.new_temp_reg();
    gen_state.output_file << "%" + kind_ptr_reg << " = getelementptr " << llvm_struct_type_name <<
    ", ptr " << "%" + suspend_struct_ptr_reg << ", i32 0, i32 0" << std::endl;
    // store i32 0, ptr %<kind_ptr_reg>
    gen_state.output_file << "store i32 " + std::to_string(suspend_tag.kind) << ", ptr " 
    << "%" + kind_ptr_reg << std::endl;
    if(suspend_tag.kind == SuspendTagKind::LOCK) {
        // Getting the pointer to the lock field
        std::string lock_ptr_reg = gen_state.reg_label_gen.new_temp_reg();
        gen_state.output_file << "%" + lock_ptr_reg << " = getelementptr " << llvm_struct_type_name <<
        ", ptr " << "%" + suspend_struct_ptr_reg << ", i32 0, i32 1" << std::endl;
        // store i32 <lock_id>, ptr %<lock_ptr>
        gen_state.output_file << "store i32 " << suspend_tag.lock_id << ", ptr " << "%" + lock_ptr_reg 
        << std::endl;
    }
    // Calling the [suspend_instance] trap
    // The actor instance to be locked is stored in %lock_instance.runtime.
    gen_state.output_file << "call void @suspend_instance(i64 " << "%lock_instance.runtime" 
    << ", ptr " << "%" + suspend_struct_ptr_reg << ")" << std::endl;
}