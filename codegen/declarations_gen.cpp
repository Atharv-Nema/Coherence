#include "top_level.hpp"
#include "generator_state.hpp"
#include "pattern_matching_boilerplate.hpp"
#include "string_utils.hpp"
#include <variant>
#include <sstream>


// Units represented using empty struct named [%Unit]
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


void emit_function_codegen(GenState& gen_state, std::shared_ptr<TopLevelItem::Func> func_def) {
    // Emitting the definition
    map_emit_llvm_function_sig<TopLevelItem::VarDecl>(
        gen_state.output_file,
        func_def->name,
        llvm_type_of_full_type(gen_state, func_def->return_type)->llvm_type_name,
        std::vector<std::string>{"i64 %this"},
        func_def->params,
        [&](const TopLevelItem::VarDecl& var_decl) {
            return llvm_type_of_full_type(gen_state, var_decl.type)->llvm_type_name 
                    + " %" + var_decl.name;
        }
    );
    gen_state.output_file << " {" << std::endl;

    // Store the parameters on the stack
    // need something like
    // %1 = alloca i64
    // store %1 %this
    // And then in a loop, do for all the other parameters
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