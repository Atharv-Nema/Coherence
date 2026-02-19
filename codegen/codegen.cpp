#include "codegen.hpp"
#include "alpha_renaming.hpp"
#include "codegen_utils.hpp"
#include "pattern_matching_boilerplate.hpp"
#include "string_utils.hpp"
#include "scoped_store.cpp"
#include "runtime_traps.hpp"
#include "generate_llvm_structs.hpp"
#include "special_reg_names.hpp"
#include "defer.cpp"
#include <assert.h>
#include <variant>
#include <sstream>
#include <fstream>

enum class ValueCategory { LVALUE, RVALUE };

std::string convert_to_rvalue(
    GenState &gen_state, 
    const std::string& llvm_type, 
    const std::string& reg_name, 
    ValueCategory val_cat) {
    switch(val_cat) {
        case(ValueCategory::RVALUE):
            return reg_name;
        case(ValueCategory::LVALUE):
            std::string temp_reg = gen_state.reg_label_gen.new_temp_reg();
            std::string pointer_type = llvm_type;
            if(llvm_type != "ptr") {
                pointer_type += "*";
            }
            gen_state.out_stream << "%" + temp_reg << " = load " << llvm_type << ", " << pointer_type
            << " " << "%" + reg_name << std::endl;
            return temp_reg;
    }
    assert(false);
}

std::string emit_valexpr_rvalue(
    GenState& gen_state,
    std::shared_ptr<ValExpr> val_expr);

std::pair<std::string, ValueCategory> emit_valexpr(
    GenState& gen_state, 
    std::shared_ptr<ValExpr> val_expr) {
    return std::visit(Overload{
        [&](ValExpr::VUnit) {
            std::string unit_reg = gen_state.reg_label_gen.new_temp_reg();
            gen_state.out_stream << "%" + unit_reg << " = add i1 0, 0" << std::endl;
            return make_pair(
                unit_reg,
                ValueCategory::RVALUE
            );
        },
        [&](ValExpr::VNullptr) {
            std::string null_reg = gen_state.reg_label_gen.new_temp_reg();
            gen_state.out_stream << "%" + null_reg << " = null" << std::endl;
            return make_pair(
                null_reg,
                ValueCategory::RVALUE
            );
        },
        [&](ValExpr::VBool v_bool) {
            std::string bool_reg = gen_state.reg_label_gen.new_temp_reg();
            gen_state.out_stream << "%" + bool_reg << " = add i1 0, " << v_bool.v << std::endl;
            return make_pair(
                bool_reg,
                ValueCategory::RVALUE
            );
        },
        [&](ValExpr::VInt v_int) {
            std::string int_reg = gen_state.reg_label_gen.new_temp_reg();
            gen_state.out_stream << "%" + int_reg << " = add i32 0, " << v_int.v << std::endl;
            return make_pair(
                int_reg,
                ValueCategory::RVALUE
            );
        },
        [&](const ValExpr::VVar& var) {
            if(var.name == "this") {
                return make_pair(
                    THIS_ACTOR_ID_REG,
                    ValueCategory::RVALUE);
            }
            return make_pair(
                gen_state.var_reg_mapping[var.name], 
                ValueCategory::LVALUE);
        },
        [&](const ValExpr::VStruct& vstruct) {
            // Will be compiling it down to:
            // %s0 = insertvalue %Struct undef, i32 %a, 0
            // %s1 = insertvalue %Struct %s0, i8 %b, 1
            // ... (these are functional structs)
            std::shared_ptr<const Type> struct_type = val_expr->expr_type;  
            std::shared_ptr<LLVMTypeInfo> llvm_type = 
                llvm_type_of_coh_type(gen_state, struct_type);
            std::shared_ptr<LLVMStructInfo> llvm_struct_info = llvm_type->struct_info;
            assert(llvm_struct_info != nullptr);
            std::string curr_accum_expr = "undef";
            for(auto &[field_name, val_expr]: vstruct.fields) {
                std::string llvm_field_type = 
                    (llvm_struct_info->field_ind_map[field_name]).field_type->llvm_type_name;
                size_t field_no = (llvm_struct_info->field_ind_map[field_name]).field_index;
                auto [val_expr_reg, val_cat] = emit_valexpr(gen_state, val_expr);
                std::string val_expr_reg_rval = 
                    convert_to_rvalue(gen_state, llvm_field_type, val_expr_reg, val_cat);
                std::string temp_reg = gen_state.reg_label_gen.new_temp_reg();
                gen_state.out_stream << "%" << temp_reg << " = insertvalue " << llvm_type->llvm_type_name 
                << " " << curr_accum_expr << ", " << llvm_field_type << " " << "%" << val_expr_reg << ", " 
                << field_no << std::endl;
                curr_accum_expr = "%" + temp_reg;
            }
            return make_pair(curr_accum_expr.substr(1), ValueCategory::RVALUE);
        },
        [&](const ValExpr::NewInstance& new_instance) {
            // Compiling the default value
            std::string llvm_type_of_default = 
                llvm_type_of_coh_type(
                    gen_state, 
                    new_instance.init_expr->expr_type)->llvm_type_name; 
            auto [default_val_reg, default_expr_cat] = 
                emit_valexpr(gen_state, new_instance.init_expr);
            std::string default_val_reg_rval = 
                convert_to_rvalue(
                    gen_state, 
                    llvm_type_of_default, 
                    default_val_reg,
                    default_expr_cat);
            
            // Compiling the size
            std::string llvm_type_of_size = "i32";
            auto [size_val_reg, size_expr_cat] = 
                emit_valexpr(gen_state, new_instance.size);
            std::string size_val_reg_rval = 
                convert_to_rvalue(
                    gen_state, 
                    llvm_type_of_size, 
                    size_val_reg,
                    size_expr_cat);
            std::string size64_reg = convert_i32_to_i64(gen_state, size_val_reg_rval);

            // Getting the llvm type of the internal object
            std::string allocated_obj_type = 
                llvm_type_of_coh_type(gen_state, new_instance.type)->llvm_type_name;
            
            // Getting the number of bytes
            std::string type_size = get_llvm_type_size(gen_state, allocated_obj_type);
            std::string num_bytes_reg = gen_state.reg_label_gen.new_temp_reg();
            // %<num_bytes_reg> = mul i64 %<size64_reg>, %<type_size>
            gen_state.out_stream << "%" << num_bytes_reg << " = mul i64 " << "%" 
            << size64_reg << ", " << "%" << type_size << std::endl;

            // Performing the malloc
            // %<pointer_reg> = call ptr @malloc(i64 %<num_bytes_reg>)
            std::string pointer_reg = gen_state.reg_label_gen.new_temp_reg();
            gen_state.out_stream << "%" << pointer_reg << " = call ptr @malloc(i64 " << 
            "%" << num_bytes_reg << ")" << std::endl;

            // Loop to fill out the default value at all indices
            /*
            br label %fill.preheader

            fill.preheader:
            br label %fill.cond

            fill.cond:
            %i = phi i64 [ 0, %fill.preheader ], [ %i.next, %fill.body ]
            %cmp = icmp ult i64 %i, %n
            br i1 %cmp, label %fill.body, label %fill.end

            fill.body:
            %elem.ptr = getelementptr T, ptr %base, i64 %i
            store T %def, ptr %elem.ptr
            %i.next = add i64 %i, 1
            br label %fill.cond

            fill.end:
            */
            std::string ind_preheader_label = gen_state.reg_label_gen.new_label();
            std::string ind_comp_label = gen_state.reg_label_gen.new_label();
            std::string fill_body_label = gen_state.reg_label_gen.new_label();
            std::string fill_end_label = gen_state.reg_label_gen.new_label();
            branch_label(gen_state, ind_preheader_label);
            gen_state.out_stream << ind_preheader_label << ":" << std::endl;
            branch_label(gen_state, ind_comp_label);
            gen_state.out_stream << ind_comp_label << ":" << std::endl;
            std::string ind_reg = gen_state.reg_label_gen.new_temp_reg();
            std::string next_ind_reg = gen_state.reg_label_gen.new_temp_reg();
            gen_state.out_stream << "%" + ind_reg << " = phi i64 [ 0, %" + ind_preheader_label << " ], [ "
            << "%" + next_ind_reg << ", %" + fill_body_label + " ]" << std::endl;
            std::string size_cmp_result = gen_state.reg_label_gen.new_temp_reg();
            gen_state.out_stream << "%" + size_cmp_result << " = icmp ult i64 " << "%" + ind_reg << ", "
            << "%" + size64_reg << std::endl;
            gen_state.out_stream << "br i1 " << "%" + size_cmp_result << ", label " << "%" + fill_body_label 
            << ", label " << "%" + fill_end_label << std::endl;
            gen_state.out_stream << fill_body_label << ":" << std::endl;
            std::string arr_ele_reg = gen_state.reg_label_gen.new_temp_reg();
            gen_state.out_stream << "%" + arr_ele_reg << " = getelementptr " << llvm_type_of_default << ", ptr "
            << "%" + pointer_reg << ", i64 " << "%" + ind_reg << std::endl;
            gen_state.out_stream << "store " << llvm_type_of_default << " " << "%" + default_val_reg 
            << ", ptr " << "%" + arr_ele_reg << std::endl;
            gen_state.out_stream << "%" + next_ind_reg << " = add i64 " << "%" + ind_reg << ", 1" << std::endl;
            branch_label(gen_state, ind_comp_label);
            gen_state.out_stream << fill_end_label << ":" << std::endl; 
            return make_pair(pointer_reg, ValueCategory::RVALUE);
        },
        [&](const ValExpr::ActorConstruction& actor_construction) {
            // 1. Allocate space on the heap for the actor struct
            std::string actor_struct_type = "%" + llvm_struct_of_actor(actor_construction.actor_name);
            std::string actor_struct_size = get_llvm_type_size(gen_state, actor_struct_type);
            std::string actor_struct_ptr = gen_state.reg_label_gen.new_temp_reg();
            // %<actor_struct_ptr> = call ptr @malloc(i64 %<actor_struct_size>)
            gen_state.out_stream << "%" << actor_struct_ptr << " = call ptr @malloc(i64 "
            << "%" << actor_struct_size << ")" << std::endl;
            
            // 2. Register the actor by calling [handle_actor_creation]
            std::string actor_id_reg = gen_state.reg_label_gen.new_temp_reg();
            gen_state.out_stream << "%" << actor_id_reg << " = call i64 @handle_actor_creation(ptr "
            << "%" << actor_struct_ptr << ")" << std::endl;

            // 3. Compile all the parameters and call the function
            std::vector<std::pair<std::string, std::string>> func_args;
            for(std::shared_ptr<ValExpr> arg_expr: actor_construction.args) {
                std::string llvm_type = 
                    llvm_type_of_coh_type(gen_state, arg_expr->expr_type)->llvm_type_name;
                auto [arg_expr_reg, val_cat] = emit_valexpr(gen_state, arg_expr);
                std::string arg_expr_rval_reg = 
                    convert_to_rvalue(gen_state, llvm_type, arg_expr_reg, val_cat);
                func_args.push_back({llvm_type, arg_expr_rval_reg});
            }
            func_args.push_back({"i64", actor_id_reg});
            // Adding the current passed-in actor to the end for suspension behaviour
            func_args.push_back({"i64", SYNCHRONOUS_ACTOR_ID_REG});
            std::string constr_func_name_llvm = 
                llvm_name_of_constructor(actor_construction.constructor_name, actor_construction.actor_name);
            gen_state.out_stream << "call void @" << constr_func_name_llvm << "(";
            map_emit_list<std::pair<std::string, std::string>>(
                gen_state.out_stream,
                func_args,
                ", ",
                [](std::pair<std::string, std::string> p) {
                    return p.first + " %" + p.second;
                }
            );
            gen_state.out_stream << ")" << std::endl;
            // 4. Returning the register storing the pointer as an rvalue
            return make_pair(actor_id_reg, ValueCategory::RVALUE);
        },
        [&](const ValExpr::Consume& consume) {
            // consume x is basically the same as just the internal variable name
            // There really is no need to create an RValue, because we can afford to
            // modify the variable in-place (the variable will not be used until it is
            // assigned again)
            return make_pair(
                gen_state.var_reg_mapping[consume.var_name], 
                ValueCategory::LVALUE);
        },
        [&](const ValExpr::PointerAccess& pointer_access) {
            // 1. Compile the value and index
            auto [pointer_reg, pointer_val_cat] =
                emit_valexpr(gen_state, pointer_access.value);
            std::string pointer_reg_rval = 
                convert_to_rvalue(gen_state, "ptr", pointer_reg, pointer_val_cat);
            auto [index_reg, index_val_cat] = 
                emit_valexpr(gen_state, pointer_access.index);
            std::string index_reg_rval =
                convert_to_rvalue(gen_state, "i32", index_reg, index_val_cat);
            std::string index_i64 = convert_i32_to_i64(gen_state, index_reg_rval);
            
            // 2. Get the llvm type of the internal element
            std::string deref_type = 
                llvm_type_of_coh_type(gen_state, val_expr->expr_type)->llvm_type_name;
            
            // 3. Use getelementptr to get the correct pointer
            std::string deref_lval_reg = gen_state.reg_label_gen.new_temp_reg();
            gen_state.out_stream << "%" << deref_lval_reg << " = getelementptr " << deref_type
            <<  ", ptr " << "%" << pointer_reg_rval << ", i64 " << "%" << index_i64 << std::endl;

            return make_pair(deref_lval_reg, ValueCategory::LVALUE);
        },
        [&](const ValExpr::Field& field_access) {
            // 1. Compile [field_access.base]
            auto [base_struct_reg, base_val_cat] = emit_valexpr(gen_state, field_access.base);
            std::shared_ptr<LLVMStructInfo> llvm_struct_info = 
                llvm_type_of_coh_type(gen_state, field_access.base->expr_type)->struct_info;
            std::string llvm_struct_type_name = 
                llvm_type_of_coh_type(gen_state, field_access.base->expr_type)->llvm_type_name;
            assert(llvm_struct_info != nullptr);
            auto [field_ind, field_type] = llvm_struct_info->field_ind_map.at(field_access.field);

            // 2. Depending on the value category of the compiled result, return
            // either a pointer or a value
            switch(base_val_cat){
                case(ValueCategory::LVALUE):
                {    // %field_ptr = getelementptr %struct.T, ptr %base, i32 0, i32 field_ind
                    std::string field_ptr_reg = gen_state.reg_label_gen.new_temp_reg();
                    gen_state.out_stream << "%" + field_ptr_reg << " = getelementptr " <<
                    llvm_struct_type_name << ", ptr " << "%" + base_struct_reg << ", i32 0, i32 "
                    << std::to_string(field_ind) << std::endl;
                    return std::make_pair(field_ptr_reg, ValueCategory::LVALUE);
                }
                case(ValueCategory::RVALUE):
                {   // %<field_val_reg> = extractvalue %<struct_type> %<struct_reg>, field_ind
                    std::string field_val_reg = gen_state.reg_label_gen.new_temp_reg();
                    gen_state.out_stream << "%" + field_val_reg << " = extractvalue " << 
                    llvm_struct_type_name << " " << base_struct_reg << ", " 
                    << std::to_string(field_ind);
                    return std::make_pair(field_val_reg, ValueCategory::RVALUE);
                }
            }
            assert(false);
        },
        [&](const ValExpr::Assignment& assignment) {
            // 1. Compile lhs and rhs and convert rhs to an rvalue
            std::string llvm_type = 
                llvm_type_of_coh_type(gen_state, val_expr->expr_type)->llvm_type_name;
            auto [lhs_reg, lhs_val_cat] = emit_valexpr(gen_state, assignment.lhs);
            assert(lhs_val_cat == ValueCategory::LVALUE);
            auto [rhs_reg, rhs_val_cat] = emit_valexpr(gen_state, assignment.rhs);
            std::string rhs_reg_rval = convert_to_rvalue(gen_state, llvm_type, rhs_reg, rhs_val_cat);
            
            // 2. Store the previous value of the output register
            std::string prev_val = gen_state.reg_label_gen.new_temp_reg();
            // %<prev_val> = load <llvm_type>, ptr %<lhs_reg>
            gen_state.out_stream << "%" + prev_val << " = load " << llvm_type << ", ptr " <<
            "%" + lhs_reg << std::endl;

            // 3. Store the rhs value to [lhs_reg]
            // store <llvm_type> %rhs, ptr %lhs
            gen_state.out_stream << "store " << llvm_type << " " << "%" + rhs_reg_rval 
            << ", ptr " << "%" + lhs_reg << std::endl;  
            
            return make_pair(prev_val, ValueCategory::RVALUE);
        },
        [&](const ValExpr::FuncCall& func_call) {
            std::vector<std::pair<std::string, std::string>> func_args;
            // CR: Separate this common code to another function
            for(std::shared_ptr<ValExpr> arg_expr: func_call.args) {
                std::string llvm_type = 
                    llvm_type_of_coh_type(gen_state, arg_expr->expr_type)->llvm_type_name;
                auto [arg_expr_reg, val_cat] = emit_valexpr(gen_state, arg_expr);
                std::string arg_expr_rval_reg = 
                    convert_to_rvalue(gen_state, llvm_type, arg_expr_reg, val_cat);
                func_args.push_back({llvm_type, arg_expr_rval_reg});
            }
            func_args.push_back({"i64", THIS_ACTOR_ID_REG});
            func_args.push_back({"i64", SYNCHRONOUS_ACTOR_ID_REG});
            
            auto llvm_func_opt = gen_state.func_llvm_name_map.get_value(func_call.func);
            assert(llvm_func_opt != std::nullopt);
            std::string llvm_func = *llvm_func_opt;
            std::string func_return_reg = gen_state.reg_label_gen.new_temp_reg();
            std::string llvm_return_type = 
                llvm_type_of_coh_type(gen_state, val_expr->expr_type)->llvm_type_name;
            gen_state.out_stream << "%" + func_return_reg << " = " <<
            "call " + llvm_return_type + " @" << llvm_func << "(";
            map_emit_list<std::pair<std::string, std::string>>(
                gen_state.out_stream,
                func_args,
                ", ",
                [](std::pair<std::string, std::string> p) {
                    return p.first + " %" + p.second;
                }
            );
            gen_state.out_stream << ")" << std::endl;
            return make_pair(func_return_reg, ValueCategory::RVALUE);
        },
        [&](const ValExpr::BinOpExpr& bin_op_expr) {
            // I am getting rid of floats for now
            std::string llvm_cmp_type = 
                llvm_type_of_coh_type(gen_state, bin_op_expr.lhs->expr_type)->llvm_type_name;
            std::string lhs_reg = emit_valexpr_rvalue(gen_state, bin_op_expr.lhs);
            std::string rhs_reg = emit_valexpr_rvalue(gen_state, bin_op_expr.rhs);
            std::string result_reg = gen_state.reg_label_gen.new_temp_reg();
            gen_state.out_stream << "%" + result_reg << " = ";
            switch(bin_op_expr.op) {
                case BinOp::Add:
                    gen_state.out_stream 
                    << " add " << llvm_cmp_type  << " %" + lhs_reg << ", " << "%" + rhs_reg << std::endl;
                    break;
                case BinOp::Sub:
                    gen_state.out_stream << " sub i32 " << "%" + lhs_reg << ", " << "%" + rhs_reg << std::endl;
                    break;
                case BinOp::Mul:
                    gen_state.out_stream << " mul i32 " << "%" + lhs_reg << ", " << "%" + rhs_reg << std::endl;
                    break;
                case BinOp::Div:
                    gen_state.out_stream << " udiv i32 " << "%" + lhs_reg << ", " << "%" + rhs_reg << std::endl;
                    break;
                case BinOp::Geq:
                    gen_state.out_stream << " icmp sge i32 " << "%" + lhs_reg << ", " << "%" + rhs_reg << std::endl;
                    break;
                case BinOp::Leq:
                    gen_state.out_stream << " icmp sle i32 " << "%" + lhs_reg << ", " << "%" + rhs_reg << std::endl;
                    break;
                case BinOp::Eq:
                    gen_state.out_stream << " icmp eq i32 " << "%" + lhs_reg << ", " << "%" + rhs_reg << std::endl;
                    break;
                case BinOp::Neq:
                    gen_state.out_stream << " icmp ne i32 " << "%" + lhs_reg << ", " << "%" + rhs_reg << std::endl;
                    break;
                case BinOp::Gt:
                    gen_state.out_stream << " icmp sgt i32 " << "%" + lhs_reg << ", " << "%" + rhs_reg << std::endl;
                    break;
                case BinOp::Lt:
                    gen_state.out_stream << " icmp slt i32 " << "%" + lhs_reg << ", " << "%" + rhs_reg << std::endl;
                    break;
            }
            return make_pair(result_reg, ValueCategory::RVALUE);
        }
    }, val_expr->t);
}

std::string emit_valexpr_rvalue(
    GenState& gen_state,
    std::shared_ptr<ValExpr> val_expr) {
    auto [expr_reg, val_cat] = emit_valexpr(gen_state, val_expr);
    std::string llvm_type = llvm_type_of_coh_type(gen_state, val_expr->expr_type)->llvm_type_name;
    return convert_to_rvalue(gen_state, llvm_type, expr_reg, val_cat);
}

void emit_statement_codegen(GenState& gen_state, std::shared_ptr<Stmt> stmt) {
    std::visit(Overload{
        [&](const Stmt::VarDeclWithInit& var_decl_with_init) {
            // We have already allocated memory at the start of the function. Just need to assign it
            std::string var_type = llvm_type_of_coh_type(gen_state, var_decl_with_init.init->expr_type)->llvm_type_name;
            assert(gen_state.var_reg_mapping.find(var_decl_with_init.name) != gen_state.var_reg_mapping.end());
            std::string stack_reg = gen_state.var_reg_mapping.at(var_decl_with_init.name);
            std::string init_reg = emit_valexpr_rvalue(gen_state, var_decl_with_init.init);
            // store <llvm_type> %<init_reg>, ptr %<stack_reg>
            gen_state.out_stream << "store " << var_type << " " << "%" << init_reg << ", ptr " << "%" 
            << stack_reg << std::endl;
        },
        [&](const Stmt::MemberInitialize& member_init) {
            // The same as [same as assigning to a variable]
            std::string init_val_reg = emit_valexpr_rvalue(gen_state, member_init.init);
            assert(gen_state.var_reg_mapping.find(member_init.member_name) != gen_state.var_reg_mapping.end());
            std::string member_reg = gen_state.var_reg_mapping.at(member_init.member_name);
            std::string init_type = llvm_type_of_coh_type(gen_state, member_init.init->expr_type)->llvm_type_name;
            gen_state.out_stream << "store " << init_type << " " << "%" << init_val_reg << ", ptr " 
            << "%" + member_reg << std::endl;
        },
        [&](const Stmt::BehaviourCall& be_call) {
            // Compiling the actor
            std::string actor_id_reg = emit_valexpr_rvalue(gen_state, be_call.actor);
            // Need to package all of these arguments into a struct.
            // Each behaviour has an associated struct. The info of the struct is everywhere, but it is not really
            // needed. The last two parameters of the struct are the actor_id and the actor_struct_pointer
            // std::string actor_struct_pointer_reg = gen_state.reg_label_gen.new_temp_reg();
            // // %struct_pointer = call ptr @get_instance_struct(i64 %<actor_id_reg>)
            // gen_state.out_stream << "%" + actor_struct_pointer_reg << " = call ptr @get_instance_struct(i64 " 
            // << "%" + actor_id_reg << ")" << std::endl;
            std::string be_actor_name = actor_name_of_coh_type(be_call.actor->expr_type);
            std::string be_name_llvm = llvm_name_of_behaviour(be_call.behaviour_name, be_actor_name);
            std::string be_struct_name = llvm_struct_of_behaviour(be_call.behaviour_name, be_actor_name);
            // Allocating memory for the struct
            // Getting the size of the struct
            std::string struct_size = get_llvm_type_size(gen_state, "%" + be_struct_name);
            // %<struct_ptr> = call ptr @malloc(i64 %<struct_size>)
            std::string msg_struct_ptr = gen_state.reg_label_gen.new_temp_reg();
            gen_state.out_stream << "%" + msg_struct_ptr << " = call ptr @malloc(i64 " << "%" + struct_size <<
            ")" << std::endl;
            // Compiling all of the behaviour arguments
            std::vector<std::pair<std::string, std::string>> compiler_args_info;
            for(size_t i = 0; i < be_call.args.size(); i++) {
                std::string arg_reg = emit_valexpr_rvalue(gen_state, be_call.args[i]);
                std::string arg_llvm_type = llvm_type_of_coh_type(gen_state, be_call.args[i]->expr_type)->llvm_type_name;
                compiler_args_info.push_back({arg_llvm_type, arg_reg});
            }
            compiler_args_info.push_back({"i64", actor_id_reg});
            // Now need to fill out the struct
            for(size_t i = 0; i < compiler_args_info.size(); i++) {
                auto &[llvm_type, llvm_reg] = compiler_args_info[i];
                std::string field_ptr = gen_state.reg_label_gen.new_temp_reg();
                // %<field_ptr> = getelementptr %<BeStruct>, ptr %<StructObj>, i32 0, i32 i
                gen_state.out_stream << "%" + field_ptr << " = getelementptr " << "%" + be_struct_name << ", ptr "
                << "%" + msg_struct_ptr << ", i32 0, i32 " << i << std::endl;
                // store <llvm_type> %<llvm_reg>, ptr %<field_ptr>
                gen_state.out_stream << "store " << llvm_type << " " << "%" + llvm_reg << ", ptr " 
                << "%" + field_ptr << std::endl;
            }
            // Now, pass this struct to [handle_behaviour_call]
            gen_state.out_stream << "call void @handle_behaviour_call(i64 " << "%" + actor_id_reg << ", ptr " 
            << "%" + msg_struct_ptr << ", ptr @" << be_name_llvm << ")" << std::endl;
        },
        [&](const Stmt::Print& print_expr) {
            std::string print_int_reg = emit_valexpr_rvalue(gen_state, print_expr.print_expr);
            std::string print_llvm_type_name = 
                llvm_type_of_coh_type(gen_state, print_expr.print_expr->expr_type)->llvm_type_name;
            std::string print_func_name;
            if(print_llvm_type_name == "i32") {
                print_func_name = "print_int";
            }
            else {
                assert(false);
            }
            gen_state.out_stream << "call void @" << print_func_name << "(" << print_llvm_type_name << " " << "%" + print_int_reg
            << ")" << std::endl;
        },
        [&](const Stmt::Expr& expr) {
            emit_valexpr(gen_state, expr.expr);
        },
        [&](const Stmt::If& if_stmt) {
            // Compiling the condition
            std::string cond_reg = emit_valexpr_rvalue(gen_state, if_stmt.cond);
            std::string then_label = gen_state.reg_label_gen.new_label();
            std::string else_label = gen_state.reg_label_gen.new_label();
            std::string end_label = gen_state.reg_label_gen.new_label();
            // br i1 %<cond_reg>, label %<then_label>, label %<else_label>
            gen_state.out_stream << "br i1 " << "%" + cond_reg << ", label " << "%" + then_label << ", "
            << "label " << "%" + else_label << std::endl;
            gen_state.out_stream << then_label << ":" << std::endl;
            // Compiling the if-block
            for(std::shared_ptr<Stmt> then_block_stmt: if_stmt.then_body) {
                emit_statement_codegen(gen_state, then_block_stmt);
            }
            branch_label(gen_state, end_label);
            // Compiling the else-block
            gen_state.out_stream << else_label << ":" << std::endl;
            if(if_stmt.else_body != std::nullopt) {
                for(std::shared_ptr<Stmt> else_block_stmt: *if_stmt.else_body) {
                    emit_statement_codegen(gen_state, else_block_stmt);
                }
            }
            branch_label(gen_state, end_label);
            gen_state.out_stream << end_label << ":" << std::endl;
        },
        [&](const Stmt::While& while_stmt) {
            std::string cond_label = gen_state.reg_label_gen.new_label();
            std::string body_label = gen_state.reg_label_gen.new_label();
            std::string end_label = gen_state.reg_label_gen.new_label();
            branch_label(gen_state, cond_label);
            gen_state.out_stream << cond_label << ":" << std::endl;
            std::string cond_reg = emit_valexpr_rvalue(gen_state, while_stmt.cond);
            // br i1 %<cond_reg>, label %<body_label>, label %<end_label>
            gen_state.out_stream << "br i1 " << "%" + cond_reg << ", label " << "%" + body_label << ", label "
            << "%" + end_label << std::endl;
            gen_state.out_stream << body_label << ":" << std::endl;
            for(std::shared_ptr<Stmt> body_stmt: while_stmt.body) {
                emit_statement_codegen(gen_state, body_stmt);
            }
            branch_label(gen_state, cond_label);
            gen_state.out_stream << end_label << ":" << std::endl;
        },
        [&](std::shared_ptr<Stmt::Atomic> atomic_stmt) {
            std::vector<uint64_t> locks_acquired;
            locks_acquired.reserve(atomic_stmt->locks_dereferenced->size());
            for(const std::string& lock: *(atomic_stmt->locks_dereferenced)) {
                if(gen_state.lock_id_map.find(lock) == gen_state.lock_id_map.end()) {
                    gen_state.lock_id_map.emplace(lock, gen_state.lock_id_map.size());
                }
                assert(gen_state.lock_id_map.find(lock) != gen_state.lock_id_map.end());
                uint64_t lock_id = gen_state.lock_id_map.at(lock);
                locks_acquired.push_back(lock_id);
            }
            sort(locks_acquired.begin(), locks_acquired.end());
            for(uint64_t lock_id: locks_acquired) {
                SuspendTag suspend_tag;
                suspend_tag.kind = SuspendTagKind::LOCK;
                suspend_tag.lock_id = lock_id;
                generate_suspend_call(gen_state, suspend_tag);
            }
            for(std::shared_ptr<Stmt> stmt: atomic_stmt->body) {
                emit_statement_codegen(gen_state, stmt);
            }
            for(uint64_t lock_id: locks_acquired) {
                gen_state.out_stream << "call void @handle_unlock(i64 " << lock_id << ")" << std::endl;
            }
        },
        [&](const Stmt::Return& return_stmt) {
            std::string return_expr_reg = emit_valexpr_rvalue(gen_state, return_stmt.expr);
            std::string llvm_return_type = llvm_type_of_coh_type(gen_state, return_stmt.expr->expr_type)->llvm_type_name;
            gen_state.out_stream << "ret " << llvm_return_type << " " << "%" + return_expr_reg << std::endl;
        }
    }, stmt->t);
}

void compile_callable_body(
    GenState &gen_state, 
    std::vector<std::shared_ptr<Stmt>> callable_body) {
    if(gen_state.curr_actor) {
        // Get the structure corresponding to the body of the current actor and unpack it on the stack
        std::string curr_actor_llvm_struct = llvm_struct_of_actor(gen_state.curr_actor->name);
        std::string curr_actor_name = gen_state.curr_actor->name;
        // Getting the struct member from the runtime
        std::string actor_struct_reg = gen_state.reg_label_gen.new_temp_reg();
        gen_state.out_stream << "%" + actor_struct_reg << " = call ptr @get_instance_struct(i64 "
        << "%" + THIS_ACTOR_ID_REG << ")" << std::endl;
        assert(gen_state.type_name_info_map.find(curr_actor_name) != gen_state.type_name_info_map.end());
        std::shared_ptr<LLVMStructInfo> actor_struct_info = 
            gen_state.type_name_info_map.at(curr_actor_name)->struct_info;
        // Now for each std::unordered_map<std::string, FieldInfo> field_ind_map;
        for(const auto&[mem_name, mem_info]: actor_struct_info->field_ind_map) {
            std::string mem_ptr_reg = gen_state.reg_label_gen.new_temp_reg();
            gen_state.out_stream << "%" + mem_ptr_reg << " = getelementptr " << "%" + curr_actor_llvm_struct << 
            ", ptr " << "%" + actor_struct_reg << ", i32 0, i32 " << mem_info.field_index << std::endl;
            assert(gen_state.var_reg_mapping.find(mem_name) == gen_state.var_reg_mapping.end());
            gen_state.var_reg_mapping.emplace(mem_name, mem_ptr_reg);
        }
    }

    std::unordered_map<std::string, std::shared_ptr<const Type>> local_vars = 
        collect_local_variable_types(callable_body);
    for(const auto& [var, full_type]: local_vars) {
        allocate_var_to_stack(
            gen_state, 
            llvm_type_of_coh_type(gen_state, full_type)->llvm_type_name,
            var);
    }

    // Now everything is set up properly. Can proceed with the generation of statements
    for(std::shared_ptr<Stmt> stmt: callable_body) {
        emit_statement_codegen(gen_state, stmt);
    }
}

void compile_synchronous_callable(
    GenState& gen_state, 
    const std::string& llvm_func_name,
    const std::string& llvm_return_type,
    std::vector<TopLevelItem::VarDecl>& params,
    std::vector<std::shared_ptr<Stmt>>& callable_body) {
    // Pairs of {<llvm_type>, <var/reg_name>}
    std::vector<std::pair<std::string, std::string>> callable_params;
    for(TopLevelItem::VarDecl &var_decl: params) {
        callable_params.push_back({
            llvm_type_of_coh_type(gen_state, var_decl.type)->llvm_type_name,
            var_decl.name
        });
    }
    callable_params.push_back({"i64", THIS_ACTOR_ID_REG});
    callable_params.push_back({"i64", SYNCHRONOUS_ACTOR_ID_REG});
    map_emit_llvm_function_sig<std::pair<std::string, std::string>>(
        gen_state.out_stream,
        llvm_func_name,
        llvm_return_type,
        callable_params,
        [&](const std::pair<std::string, std::string>& var_decl_pair) {
            return var_decl_pair.first + " %" + var_decl_pair.second;
        }
    );
    gen_state.out_stream << " {" << std::endl;

    // Do not want to copy the hidden parameters on the stack
    callable_params.pop_back();
    callable_params.pop_back();

    for(auto &var_decl_pair: callable_params) {
        allocate_var_to_stack(gen_state, var_decl_pair.first, var_decl_pair.second);
        // The below code may be a bit confusing, but the idea is simple.
        // The function parameters register names are the same as the variable names
        // But the local variables stored on the stack will have registers pointing to
        // them with different names. So we are storing the function parameter register
        // values in the local register.
        // store <rhs_type> %<rhs>, ptr %<lhs>
        assert(gen_state.var_reg_mapping.find(var_decl_pair.second) != gen_state.var_reg_mapping.end());
        std::string stack_reg = gen_state.var_reg_mapping.at(var_decl_pair.second);
        gen_state.out_stream << "store " << var_decl_pair.first << " " << "%" + var_decl_pair.second 
        << ", " << "ptr " << "%" + stack_reg << std::endl;
        gen_state.var_reg_mapping.emplace(var_decl_pair.second, stack_reg);
    }
    compile_callable_body(gen_state, callable_body);
    if(llvm_return_type == "void") {
        gen_state.out_stream << "ret void" << std::endl;
    }
    gen_state.out_stream << "unreachable" << std::endl;
    gen_state.out_stream << "}" << std::endl;
}

void emit_function(GenState& gen_state, std::shared_ptr<TopLevelItem::Func> func_def) {
    gen_state.refresh_var_reg_info();
    gen_state.var_reg_mapping.clear();
    std::string func_name_llvm = llvm_name_of_func(gen_state, func_def->name);
    std::string func_return_type_llvm = 
        llvm_type_of_coh_type(gen_state, func_def->return_type)->llvm_type_name;
    compile_synchronous_callable(gen_state, func_name_llvm, func_return_type_llvm, func_def->params, func_def->body);
    // Add this function to gen_state
    gen_state.func_llvm_name_map.insert(func_def->name, func_name_llvm);
}

void emit_constructor(GenState& gen_state, std::shared_ptr<TopLevelItem::Constructor> constructor_def) {
    assert(gen_state.curr_actor != nullptr);
    gen_state.refresh_var_reg_info();
    std::string constructor_name_llvm = llvm_name_of_constructor(constructor_def->name, gen_state.curr_actor->name);
    compile_synchronous_callable(gen_state, constructor_name_llvm, "void", constructor_def->params, constructor_def->body);
}

void generate_fake_start_actor(GenState& gen_state) {
    gen_state.refresh_var_reg_info();
    // Generates a function that will act as a behaviour to be scheduled (we are going to fool the runtime)
    gen_state.out_stream << "define void @start.runtime(ptr %message) {" << std::endl;
    // Extract [SYNCHRONOUS_ACTOR_ID_REG] from %message
    gen_state.out_stream << "%" + SYNCHRONOUS_ACTOR_ID_REG << " = load i64, ptr %message" << std::endl;
    std::string main_struct =  "%Main.struct";
    std::string main_struct_size = get_llvm_type_size(gen_state, main_struct);
    std::string main_instance_ptr_reg = gen_state.reg_label_gen.new_temp_reg();
    gen_state.out_stream << "%" + main_instance_ptr_reg << " = call ptr @malloc(i64 " << "%" + main_struct_size
    << ")" << std::endl;
    std::string actor_id_reg = gen_state.reg_label_gen.new_temp_reg();
    gen_state.out_stream << "%" + actor_id_reg << " = call i64 @handle_actor_creation(ptr "
    << "%" << main_instance_ptr_reg << ")" << std::endl;
    // Calling the constructor
    std::string create_constructor_llvm_name = "create.Main.constr";
    gen_state.out_stream << "call void @" << create_constructor_llvm_name << "(i64 " << "%" + actor_id_reg << ", "
    << "i64 " << "%" + SYNCHRONOUS_ACTOR_ID_REG << ")" << std::endl;
    SuspendTag suspend_tag;
    suspend_tag.kind = SuspendTagKind::RETURN;
    generate_suspend_call(gen_state, suspend_tag);
    gen_state.out_stream << "ret void" << std::endl;
    gen_state.out_stream << "}" << std::endl;
}

void generate_coherence_initialize(GenState& gen_state) {
    gen_state.refresh_var_reg_info();
    gen_state.out_stream << "define void @coherence_initialize() {" << std::endl;
    std::string instance_id_reg = gen_state.reg_label_gen.new_temp_reg();
    gen_state.out_stream << "%" + instance_id_reg << " = call i64 @handle_actor_creation(ptr null)" << std::endl;
    // Allocating the message
    std::string message_ptr_reg = gen_state.reg_label_gen.new_temp_reg();
    gen_state.out_stream << "%" + message_ptr_reg << " = call ptr @malloc(i64 8)" << std::endl;
    gen_state.out_stream << "store i64 " << "%" + instance_id_reg << ", ptr " << "%" + message_ptr_reg << std::endl;
    gen_state.out_stream << "call void @handle_behaviour_call(i64 " << "%" + instance_id_reg << 
    ", ptr " << "%" + message_ptr_reg << ", ptr @start.runtime)" << std::endl;
    gen_state.out_stream << "ret void" << std::endl;
    gen_state.out_stream << "}" << std::endl; 
}

void emit_behaviour(GenState& gen_state, std::shared_ptr<TopLevelItem::Behaviour> behaviour_def) {
    // CR: [struct_mem_vec] is not needed here. Refactor to remove it.
    assert(gen_state.curr_actor != nullptr);
    gen_state.refresh_var_reg_info();
    std::string be_name_llvm = llvm_name_of_behaviour(behaviour_def->name, gen_state.curr_actor->name);
    std::string be_struct_llvm = llvm_struct_of_behaviour(behaviour_def->name, gen_state.curr_actor->name);
    std::vector<std::pair<std::string, std::string>> struct_mem_vec;
    for(auto &var_decl: behaviour_def->params) {
        struct_mem_vec.push_back({
            var_decl.name,
            llvm_type_of_coh_type(gen_state, var_decl.type)->llvm_type_name
        });
    }
    // Creating the behaviour function signature
    // Single parameter [ptr %message]
    map_emit_llvm_function_sig<std::string>(
        gen_state.out_stream,
        be_name_llvm,
        "void",
        std::vector<std::string>{"ptr %message"},
        [](const std::string& s) {return s;}
    );
    gen_state.out_stream << " {" << std::endl;
    // Now simply unpack and store all the stuff on the stack
    size_t be_struct_size = struct_mem_vec.size() + 1; // There is the [this] pointer at the end
    size_t last_ind = be_struct_size - 1;
    // Assigning %this.id to the actor id
    // %this.id = getelementptr %<BeStruct>, ptr %<message>, i32 0, i32 last_ind
    std::string this_actor_id_ptr_reg = gen_state.reg_label_gen.new_temp_reg();
    gen_state.out_stream << "%" + this_actor_id_ptr_reg << " = getelementptr " << "%" + be_struct_llvm << ", ptr "
    << "%message" << ", i32 0, i32 " << last_ind << std::endl;
    gen_state.out_stream << "%" + THIS_ACTOR_ID_REG << " = load i64, i64* " << "%" + this_actor_id_ptr_reg << std::endl;
    gen_state.out_stream << "%" + SYNCHRONOUS_ACTOR_ID_REG << " = " << "add i64 " << "%" + THIS_ACTOR_ID_REG << ", 0" << std::endl;
    // Unpacking the message
    for(size_t i = 0; i < struct_mem_vec.size(); i++) {
        std::string param_reg = gen_state.reg_label_gen.new_temp_reg();
        gen_state.out_stream << "%" + param_reg << " = getelementptr " << "%" + be_struct_llvm << ", ptr "
        << "%message" << ", i32 0, i32 " << i << std::endl;
        gen_state.var_reg_mapping.emplace(struct_mem_vec[i].first, param_reg);
    }
    compile_callable_body(gen_state, behaviour_def->body);
    // Returning to the runtime
    SuspendTag suspend_tag;
    suspend_tag.kind = SuspendTagKind::RETURN;
    generate_suspend_call(gen_state, suspend_tag);
    gen_state.out_stream << "unreachable" << std::endl;
    gen_state.out_stream << "}" << std::endl;
}

void generate_declarations(GenState& gen_state) {
    std::string external_decls = R"(
%SuspendTag.runtime = type <{ i32, i64 }>

declare void @print_int(i32)
declare ptr @malloc(i64)
declare void @handle_unlock(i64)
declare void @handle_behaviour_call(i64, ptr, ptr)
declare ptr @get_instance_struct(i64)
declare i64 @handle_actor_creation(ptr)
declare void @suspend_instance(i64, ptr)
)";
    gen_state.out_stream << external_decls << std::endl;
    
}

void ast_codegen(Program* program_ast, std::string output_file_name) {
    std::ofstream out_stream(output_file_name); 
    GenState gen_state(out_stream);
    gen_state.curr_actor = nullptr;
    ScopeGuard top_level(gen_state.func_llvm_name_map);
    generate_declarations(gen_state);
    generate_llvm_structs(gen_state, program_ast);
    // Collecting all the toplevel functions
    for(const TopLevelItem& top_level_item: program_ast->top_level_items) {
        std::visit(Overload{
            [&](const TopLevelItem::TypeDef& type_def) {},
            [&](std::shared_ptr<TopLevelItem::Func> func_def) {
                gen_state.func_llvm_name_map.insert(
                    func_def->name, 
                    llvm_name_of_func(gen_state, func_def->name));
            },
            [&](std::shared_ptr<TopLevelItem::Actor> actor_def) {}
        }, top_level_item.t);
    }
    for(const TopLevelItem& top_level_item: program_ast->top_level_items) {
        std::visit(Overload{
            [&](const TopLevelItem::TypeDef& type_def) {},
            [&](std::shared_ptr<TopLevelItem::Func> func_def) {
                emit_function(gen_state, func_def);
            },
            [&](std::shared_ptr<TopLevelItem::Actor> actor_def) {
                gen_state.curr_actor = actor_def;
                Defer d([&](){gen_state.curr_actor = nullptr;});
                ScopeGuard actor_level(gen_state.func_llvm_name_map);
                for(auto &actor_mem: actor_def->actor_members) {
                    std::visit(Overload{
                        [&](std::shared_ptr<TopLevelItem::Func> func_def) {
                            gen_state.func_llvm_name_map.insert(
                                func_def->name, 
                                llvm_name_of_func(gen_state, func_def->name));
                        },
                        [&](const auto&){}
                    }, actor_mem);
                }
                
                // Compiling the members
                for(auto &actor_mem: actor_def->actor_members) {
                    std::visit(Overload{
                        [&](std::shared_ptr<TopLevelItem::Func> func_def) {
                            emit_function(gen_state, func_def);
                        },
                        [&](std::shared_ptr<TopLevelItem::Constructor> constr_def) {
                            emit_constructor(gen_state, constr_def);
                        },
                        [&](std::shared_ptr<TopLevelItem::Behaviour> be_def) {
                            emit_behaviour(gen_state, be_def);
                        }
                    }, actor_mem);
                }
                gen_state.curr_actor = nullptr;
            }
        }, top_level_item.t);
    }
    generate_fake_start_actor(gen_state);
    generate_coherence_initialize(gen_state);
    gen_state.out_stream << "@num_locks = global i64 " << gen_state.lock_id_map.size() << std::endl;
}
