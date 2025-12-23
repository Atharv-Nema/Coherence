#include "top_level.hpp"
#include "generator_state.hpp"
#include "pattern_matching_boilerplate.hpp"
#include "string_utils.hpp"
#include "scoped_store.cpp"
#include "alpha_renaming.hpp"
#include <assert.h>
#include <variant>
#include <sstream>


// Units represented using empty struct named [%unit]
// Actor instances are represented as simple integers (the runtime has info about the internal
// struct)
std::shared_ptr<LLVMTypeInfo> llvm_type_of_basic_type(
    GenState& gen_state,
    BasicType basic_type) {
    return std::visit(Overload{
        [&](const BasicType::TUnit&) {
            // Using the fact that [] on unordered_maps automatically calls the
            // default constructor on the value type if the key is not present
            return std::make_shared<LLVMTypeInfo>("%unit");
        },
        [&](const BasicType::TInt&) {
            return std::make_shared<LLVMTypeInfo>("i32");
        },
        [&](const BasicType::TFloat&) {
            return std::make_shared<LLVMTypeInfo>("float");
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

std::string get_llvm_type_size(GenState &gen_state, const std::string& llvm_type_name) {
    // %szptr = getelementptr T, ptr null, i64 1
    // %sz = ptrtoint ptr %szptr to i64
    std::string size_ptr_reg = gen_state.reg_gen.new_temp_reg();
    gen_state.output_file << "%" << size_ptr_reg << " = getlementptr " << llvm_type_name
    << ", ptr null, i64 1" << std::endl;
    std::string size_reg = gen_state.reg_gen.new_temp_reg();
    gen_state.output_file << "%" << size_reg << " = ptrtoint ptr " << "%" << size_ptr_reg 
    << " to i64" << std::endl;
    return size_reg;
}

std::string llvm_struct_of_actor(std::shared_ptr<TopLevelItem::Actor> actor_def) {
    return actor_def->name + ".actor.struct";
}

void emit_type_declaration(
    GenState& gen_state, 
    Program& program_ast, 
    const TopLevelItem::TypeDef& type_def) {
    std::string type_name = type_def.type_name;
    std::visit(Overload{
        [&](const NameableType::Basic& basic_type) {
            gen_state.type_name_info_map[type_name] = 
                llvm_type_of_basic_type(gen_state, basic_type.type);
        },
        [&](const NameableType::Struct& struct_type) {
            std::string struct_name = "%" + type_name + ".struct";
            map_emit_struct<std::pair<std::string, BasicType>>(
                gen_state.output_file,
                struct_name,
                struct_type.members,
                [&](const std::pair<std::string, BasicType>& struct_mem) {
                    return llvm_type_of_basic_type(gen_state, struct_mem.second)->llvm_type_name;
                }
            );
        }
    }, type_def.nameable_type->t);
}

void allocate_var_to_stack(
    GenState& gen_state,
    const std::string& llvm_type,
    const std::string& var_name) {
    std::string stack_reg = gen_state.reg_gen.new_stack_var();
        gen_state.output_file << "%" << stack_reg << " = alloca " 
        << var_name << std::endl;
    gen_state.var_reg_mapping.insert(var_name, stack_reg);
}

std::string convert_i32_to_i64(GenState& gen_state, const std::string& i32_reg) {
    std::string size64_reg = gen_state.reg_gen.new_temp_reg();
    // %size64_reg = zext i32 %size_val_reg_rval to i64
    gen_state.output_file << "%" << size64_reg << " = zext i32 " << "%" 
    << i32_reg << " to i64" << std::endl;
    return size64_reg;
}


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
            std::string temp_reg = gen_state.reg_gen.new_temp_reg();
            gen_state.output_file << temp_reg << " = load " << llvm_type << ", " << llvm_type
            << "* " << reg_name << std::endl;
            return temp_reg;
    }
}


// CR: Fix the %/no % inconsistency

std::pair<std::string, ValueCategory> emit_valexpr(
    GenState& gen_state, 
    std::shared_ptr<ValExpr> val_expr) {
    return std::visit(Overload{
        [&](const ValExpr::VVar& var) {
            return make_pair(
                gen_state.var_reg_mapping[var.name], 
                ValueCategory::LVALUE);
        },
        [&](const ValExpr::VStruct& vstruct) {
            // Will be compiling it down to:
            // %s0 = insertvalue %Struct undef, i32 %a, 0
            // %s1 = insertvalue %Struct %s0, i8 %b, 1
            // ... (these are functional structs)
            FullType struct_type = val_expr->expr_type;  
            std::shared_ptr<LLVMTypeInfo> llvm_type = 
                llvm_type_of_full_type(gen_state, struct_type);
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
                std::string temp_reg = gen_state.reg_gen.new_temp_reg();
                gen_state.output_file << "%" << temp_reg << " = insertvalue " << "%"
                << llvm_type->llvm_type_name << " " << curr_accum_expr << ", " << llvm_field_type
                << " " << "%" << temp_reg << ", " << field_no << std::endl;
                curr_accum_expr = "%" + temp_reg;
            }
            return make_pair(curr_accum_expr.substr(1), ValueCategory::RVALUE);
        },
        [&](const ValExpr::NewInstance& new_instance) {
            // Compiling the default value
            std::string llvm_type_of_default = 
                llvm_type_of_full_type(
                    gen_state, 
                    new_instance.default_value->expr_type)->llvm_type_name; 
            auto [default_val_reg, default_expr_cat] = 
                emit_valexpr(gen_state, new_instance.default_value);
            std::string default_val_reg_rval = 
                convert_to_rvalue(
                    gen_state, 
                    llvm_type_of_default, 
                    default_val_reg,
                    default_expr_cat);
            
            // Compiling the size
            std::string llvm_type_of_size = 
                llvm_type_of_full_type(
                    gen_state, 
                    new_instance.default_value->expr_type)->llvm_type_name; 
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
                llvm_type_of_basic_type(gen_state, new_instance.type)->llvm_type_name;
            
            // Getting the number of bytes
            std::string type_size = get_llvm_type_size(gen_state, allocated_obj_type);
            std::string num_bytes_reg = gen_state.reg_gen.new_temp_reg();
            // %<num_bytes_reg> = mul i64 %<size64_reg>, %<type_size>
            gen_state.output_file << "%" << num_bytes_reg << " mul i64 " << "%" 
            << size64_reg << ", " << "%" << type_size << std::endl;

            // Performing the malloc
            // %<pointer_reg> = call ptr @malloc(i64 %<num_bytes_reg>)
            std::string pointer_reg = gen_state.reg_gen.new_temp_reg();
            gen_state.output_file << "%" << pointer_reg << " = call ptr @malloc(i64 " << 
            "%" << num_bytes_reg << ")" << std::endl;
            return make_pair(pointer_reg, ValueCategory::RVALUE);
        },
        [&](const ValExpr::ActorConstruction& actor_construction) {
            // 1. Allocate space on the heap for the actor struct
            std::string actor_struct = actor_construction.actor_name + ".struct";
            std::string actor_struct_size = get_llvm_type_size(gen_state, actor_struct);
            std::string actor_struct_ptr = gen_state.reg_gen.new_temp_reg();
            // %<actor_struct_ptr> = call ptr @malloc(i64 %<actor_struct_size>)
            gen_state.output_file << "%" << actor_struct_ptr << " = call ptr @malloc(i64 "
            << "%" << actor_struct_size << ")" << std::endl;
            
            // 2. Register the actor by calling [handle_actor_creation]
            std::string actor_id_reg = gen_state.reg_gen.new_temp_reg();
            gen_state.output_file << "%" << actor_id_reg << " = call i64 @handle_actor_creation(ptr "
            << "%" << actor_struct_ptr << ")" << std::endl;

            // 3. Compile all the parameters and call the function
            std::vector<std::pair<std::string, std::string>> func_args;
            for(std::shared_ptr<ValExpr> arg_expr: actor_construction.args) {
                std::string llvm_type = 
                    llvm_type_of_full_type(gen_state, arg_expr->expr_type)->llvm_type_name;
                auto [arg_expr_reg, val_cat] = emit_valexpr(gen_state, arg_expr);
                std::string arg_expr_rval_reg = 
                    convert_to_rvalue(gen_state, llvm_type, arg_expr_reg, val_cat);
                func_args.push_back({llvm_type, arg_expr_rval_reg});
            }
            func_args.push_back({"i64", actor_id_reg});
            func_args.push_back({"ptr", actor_struct_ptr});
            // Adding the current passed-in actor to the end for suspension behaviour
            func_args.push_back({"i64", "this"});
            std::string llvm_func_of_constructor = 
                actor_construction.constructor_name + "." + 
                actor_construction.actor_name + "." + "constructor";
            gen_state.output_file << "call void @" << llvm_func_of_constructor << "(";
            map_emit_list<std::pair<std::string, std::string>>(
                gen_state.output_file,
                func_args,
                ", ",
                [](std::pair<std::string, std::string> &p) {
                    return p.first + " %" + p.second;
                }
            );
            gen_state.output_file << ")" << std::endl;
            // 4. Returning the register storing the pointer as an rvalue
            return make_pair(actor_id_reg, ValueCategory::RVALUE);
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
                llvm_type_of_full_type(gen_state, val_expr->expr_type)->llvm_type_name;
            
            // 3. Use getelementptr to get the correct pointer
            std::string deref_lval_reg = gen_state.reg_gen.new_temp_reg();
            gen_state.output_file << "%" << deref_lval_reg << " = getelementptr " << deref_type
            <<  ", ptr " << "%" << pointer_reg_rval << ", i64 " << "%" << index_i64;

            return make_pair(deref_lval_reg, ValueCategory::RVALUE);
        },
        [&](const ValExpr::Field& field_access) {
            // 1. Compile [field_access.base]
            auto [base_struct_reg, base_val_cat] = emit_valexpr(gen_state, field_access.base);
            std::shared_ptr<LLVMStructInfo> llvm_struct_info = 
                llvm_type_of_full_type(gen_state, field_access.base->expr_type)->struct_info;
            std::string llvm_struct_type_name = 
                llvm_type_of_full_type(gen_state, field_access.base->expr_type)->llvm_type_name;
            assert(llvm_struct_info != nullptr);
            auto [field_ind, field_type] = llvm_struct_info->field_ind_map.at(field_access.field);

            // 2. Depending on the value category of the compiled result, return
            // either a pointer or a value
            switch(base_val_cat){
                case(ValueCategory::LVALUE):
                {    // %field_ptr = getelementptr %struct.T, ptr %base, i32 0, i32 field_ind
                    std::string field_ptr_reg = gen_state.reg_gen.new_temp_reg();
                    gen_state.output_file << "%" + field_ptr_reg << " = getelementptr " <<
                    llvm_struct_type_name << ", ptr " << "%" + base_struct_reg << ", i32 0, i32 "
                    << std::to_string(field_ind) << std::endl;
                    return std::make_pair(field_ptr_reg, ValueCategory::LVALUE);
                }
                case(ValueCategory::RVALUE):
                {   // %<field_val_reg> = extractvalue %<struct_type> %<struct_reg>, field_ind
                    std::string field_val_reg = gen_state.reg_gen.new_temp_reg();
                    gen_state.output_file << "%" + field_val_reg << " = extractvalue " << 
                    llvm_struct_type_name << " " << base_struct_reg << ", " 
                    << std::to_string(field_ind);
                    return std::make_pair(field_val_reg, ValueCategory::RVALUE);
                }
            }
        },
        [&](const ValExpr::Assignment& assignment) {
            // 1. Compile lhs and rhs and convert rhs to an rvalue
            std::string llvm_type = 
                llvm_type_of_full_type(gen_state, val_expr->expr_type)->llvm_type_name;
            auto [lhs_reg, lhs_val_cat] = emit_valexpr(gen_state, assignment.lhs);
            assert(lhs_val_cat == ValueCategory::LVALUE);
            auto [rhs_reg, rhs_val_cat] = emit_valexpr(gen_state, assignment.rhs);
            std::string rhs_reg_rval = convert_to_rvalue(gen_state, llvm_type, rhs_reg, rhs_val_cat);
            
            // 2. Store the previous value of the output register
            std::string prev_val = gen_state.reg_gen.new_temp_reg();
            // %<prev_val> = load <llvm_type>, ptr %<lhs_reg>
            gen_state.output_file << "%" + prev_val << " = load " << llvm_type << ", ptr " <<
            "%" + lhs_reg << std::endl;

            // 3. Store the rhs value to [lhs_reg]
            // store <llvm_type> %rhs, ptr %lhs
            gen_state.output_file << "store " << llvm_type << " " << "%" + rhs_reg_rval 
            << ", ptr " << "%" + lhs_reg << std::endl;  
            
            return make_pair(prev_val, ValueCategory::RVALUE);
        },
        [&](const ValExpr::FuncCall& func_call) {
            std::vector<std::pair<std::string, std::string>> func_args;
            // CR: Separate this common code to another function
            for(std::shared_ptr<ValExpr> arg_expr: func_call.args) {
                std::string llvm_type = 
                    llvm_type_of_full_type(gen_state, arg_expr->expr_type)->llvm_type_name;
                auto [arg_expr_reg, val_cat] = emit_valexpr(gen_state, arg_expr);
                std::string arg_expr_rval_reg = 
                    convert_to_rvalue(gen_state, llvm_type, arg_expr_reg, val_cat);
                func_args.push_back({llvm_type, arg_expr_rval_reg});
            }
            // If it is a member function, need to pass in the member struct
            if(gen_state.func_llvm_name_map.get_scope_ind(func_call.func) == 1) {
                // Get the actor struct
                assert(gen_state.curr_actor != nullptr);
                std::string actor_struct = "%" + gen_state.curr_actor->name + ".struct";
                func_args.push_back({actor_struct, "this.mem"});
            }
            func_args.push_back({"i64", "this"});
            auto llvm_func_opt = gen_state.func_llvm_name_map.get_value(func_call.func);
            assert(llvm_func_opt != std::nullopt);
            std::string llvm_func = *llvm_func_opt;
            std::string func_return_reg = gen_state.reg_gen.new_temp_reg();
            std::string llvm_return_type = 
                llvm_type_of_full_type(gen_state, val_expr->expr_type)->llvm_type_name;
            gen_state.output_file << "%" + func_return_reg << " = " <<
            "call " + llvm_return_type + " @" << llvm_func << "(";
            map_emit_list<std::pair<std::string, std::string>>(
                gen_state.output_file,
                func_args,
                ", ",
                [](std::pair<std::string, std::string> &p) {
                    return p.first + " %" + p.second;
                }
            );
            gen_state.output_file << ")" << std::endl;
            return func_return_reg;
        },
        [&](const ValExpr::BinOpExpr& bin_op_expr) {
            // TODO
        }
    }, val_expr->t);
}

std::string emit_valexpr_rvalue(
    GenState& gen_state,
    std::shared_ptr<ValExpr> val_expr) {
    auto [expr_reg, val_cat] = emit_valexpr(gen_state, val_expr);
    std::string llvm_type = llvm_type_of_full_type(gen_state, val_expr->expr_type)->llvm_type_name;
    return convert_to_rvalue(gen_state, llvm_type, expr_reg, val_cat);
}

void emit_statement_codegen(GenState& gen_state, std::shared_ptr<Stmt> stmt) {
    std::visit(Overload{
        [&](const Stmt::VarDeclWithInit& var_decl_with_init) {
            // We have already allocated memory at the start of the function. Just need to assign it
            std::string stack_reg = gen_state.var_reg_mapping.at(var_decl_with_init.name);
            std::string init_reg = emit_valexpr_rvalue(gen_state, var_decl_with_init.init);
            // store %<stack_reg> %<init>
            gen_state.output_file << "store " << "%" << stack_reg << " " << "%" << init_reg << std::endl;
        },
        [&](const Stmt::MemberInitialize& member_init) {
            std::shared_ptr<LLVMStructInfo> curr_actor_mem_info =
                gen_state.type_name_info_map.at(gen_state.curr_actor->name)->struct_info;
            std::string llvm_actor_struct_name = llvm_struct_of_actor(gen_state.curr_actor);
            // The actor body is stored in [this.mem] register
            auto [field_ind, field_type] = curr_actor_mem_info->field_ind_map.at(member_init.member_name);
            // CR: I think that this is the correct thing to do. Modify the other stuff to comply with this
            // No need to store this on the stack
            std::string actor_struct_pointer_reg = "this.mem";
            std::string init_val_reg = emit_valexpr_rvalue(gen_state, member_init.init);

            // %field_ptr = getelementptr %struct.T, ptr %base, i32 0, i32 field_ind
            std::string field_ptr_reg = gen_state.reg_gen.new_temp_reg();
            gen_state.output_file << "%" + field_ptr_reg << " = getelementptr " <<
            llvm_actor_struct_name << ", ptr " << "%" + actor_struct_pointer_reg << ", i32 0, i32 "
            << std::to_string(field_ind) << std::endl;
        },
        [&](cosnt )
    }, stmt->t);
}

// CR: Do not forget to modify [gen_state.type_name_info_map] when changing actors and stuff.

void emit_function_codegen(GenState& gen_state, std::shared_ptr<TopLevelItem::Func> func_def) {
    // Idea is that after emit_function_codegen is called, [gen_state] will be updated with the
    // function information. This will be done outside [emit_function_codegen]
    gen_state.reg_gen.refresh_counters();
    std::vector<std::pair<std::string, std::string>> func_params;
    for(TopLevelItem::VarDecl &var_decl: func_def->params) {
        func_params.push_back({
            llvm_type_of_full_type(gen_state, var_decl.type)->llvm_type_name,
            var_decl.name
        });
    }
    // Pairs of {<llvm_type>, <var/reg_name>}
    func_params.push_back({"i64", "this"});
    if(gen_state.curr_actor) {
        func_params.push_back({llvm_struct_of_actor(gen_state.curr_actor), "this.mem"});
    }
    map_emit_llvm_function_sig<std::pair<std::string, std::string>>(
        gen_state.output_file,
        func_def->name,
        llvm_type_of_full_type(gen_state, func_def->return_type)->llvm_type_name,
        func_params,
        [&](const std::pair<std::string, std::string>& var_decl_pair) {
            return var_decl_pair.first + " %" + var_decl_pair.second;
        }
    );
    gen_state.output_file << " {" << std::endl;

    // Store the parameters on the stack
    // Generating line like:
    // %1 = alloca i64
    // store %1 %this
    for(auto &var_decl_pair: func_params) {
        allocate_var_to_stack(gen_state, var_decl_pair.first, var_decl_pair.second);
        // The below code may be a bit confusing, but the idea is simple.
        // The function parameters register names are the same as the variable names
        // But the local variables stored on the stack will have registers pointing to
        // them with different names. So we are storing the function parameter register
        // values in the local register.
        std::string stack_reg = gen_state.var_reg_mapping.at(var_decl_pair.second);
        gen_state.output_file << "store " << "%" << stack_reg << " "
        << "%" << var_decl_pair.second << std::endl;
        gen_state.var_reg_mapping.insert(var_decl_pair.second, stack_reg);
    }

    // Now alpha rename the function body and store all the local variables on the stack
    std::unordered_map<std::string, FullType> local_vars = 
        alpha_rename_callable_body(func_def->body);
    for(const auto& [var, full_type]: local_vars) {
        allocate_var_to_stack(
            gen_state, 
            var, 
            llvm_type_of_full_type(gen_state, full_type)->llvm_type_name);
    }

    // Now everything is set up properly. Can proceed with the generation of statements
    for(std::shared_ptr<Stmt> stmt: func_def->body) {
        emit_statement_codegen(gen_state, stmt);
    }

    // Finally, just add the empty brace
    gen_state.output_file << "}" << std::endl; 
}

void emit_declarations(GenState& gen_state, Program& program_ast) {
    for(const TopLevelItem& top_level_item: program_ast.top_level_items) {
        std::visit(Overload{
            [&](const TopLevelItem::TypeDef& type_def) {
                emit_type_declaration(gen_state, program_ast, type_def);
            },
            [&](std::shared_ptr<TopLevelItem::Func> func_def) {
                emit_function_codegen(gen_state, func_def);
            },
            [&](std::shared_ptr<TopLevelItem::Actor> actor_def) {
                // Create an associated struct for the actor
                
                return;
            }
        }, top_level_item.t);
    }
}