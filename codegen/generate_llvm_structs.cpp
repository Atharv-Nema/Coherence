#include "generate_llvm_structs.hpp"
#include "special_reg_names.hpp"
#include "codegen_utils.hpp"
#include "string_utils.hpp"
#include "pattern_matching_boilerplate.hpp"
#include "defer.cpp"
#include <cassert>



void emit_type_definition(
    GenState& gen_state, 
    const TopLevelItem::TypeDef& type_def) {
    std::string type_name = type_def.type_name;
    std::visit(Overload{
        [&](std::shared_ptr<const Type> type) {
            gen_state.type_name_info_map.emplace(
                type_name, 
                llvm_type_of_coh_type(gen_state, type));
        },
        [&](const NameableType::Struct& struct_type) {
            std::string struct_name = type_name + ".struct";
            map_emit_struct<std::pair<std::string, std::shared_ptr<const Type>>>(
                gen_state.out_stream,
                struct_name,
                struct_type.members,
                [&](const std::pair<std::string, std::shared_ptr<const Type>>& struct_mem) {
                    return llvm_type_of_coh_type(gen_state, struct_mem.second)->llvm_type_name;
                }
            );
            std::shared_ptr<LLVMStructInfo> llvm_struct_info = std::make_shared<LLVMStructInfo>();
            for(size_t i = 0; i < struct_type.members.size(); i++) {
                auto &[field_name, basic_type] = struct_type.members[i];
                std::shared_ptr<LLVMTypeInfo> field_type_info = llvm_type_of_coh_type(gen_state, basic_type);
                LLVMStructInfo::FieldInfo field_info{i, field_type_info};
                llvm_struct_info->ind_field_map.emplace_back(field_info);
                llvm_struct_info->field_ind_map.emplace(field_name, field_info);
            }
            std::shared_ptr<LLVMTypeInfo> llvm_type_info = std::make_shared<LLVMTypeInfo>();
            llvm_type_info->llvm_type_name = "%" + struct_name;
            llvm_type_info->struct_info = llvm_struct_info;
            gen_state.type_name_info_map.emplace(
                type_name,
                llvm_type_info);
        }
    }, type_def.nameable_type->t);
}

void emit_actor_struct(
    GenState& gen_state,
    const std::shared_ptr<TopLevelItem::Actor>& actor_def) {
    // Emit the actor struct type
    std::string llvm_actor_struct = llvm_struct_of_actor(actor_def->name);
    std::vector<std::pair<std::string, std::shared_ptr<const Type>>> struct_mem_vec;
    struct_mem_vec.reserve(actor_def->member_vars.size());
    for (auto const& mem_var : actor_def->member_vars) {
        struct_mem_vec.push_back(mem_var);
    }

    map_emit_struct<std::pair<std::string, std::shared_ptr<const Type>>>(
        gen_state.out_stream,
        llvm_actor_struct,
        struct_mem_vec,
        [&](const std::pair<std::string, std::shared_ptr<const Type>>& mem) {
            return llvm_type_of_coh_type(gen_state, mem.second)->llvm_type_name;
        }
    );

    // Register struct info for member GEPs later
    auto struct_type_info = std::make_shared<LLVMTypeInfo>(LLVMTypeInfo{"%" + llvm_actor_struct, nullptr});
    struct_type_info->struct_info = std::make_shared<LLVMStructInfo>();

    for (size_t ind = 0; ind < struct_mem_vec.size(); ind++) {
        LLVMStructInfo::FieldInfo field_info{
            ind,
            llvm_type_of_coh_type(gen_state, struct_mem_vec[ind].second)
        };
        struct_type_info->struct_info->field_ind_map.emplace(struct_mem_vec[ind].first, field_info);
        struct_type_info->struct_info->ind_field_map.emplace_back(field_info);
    }

    gen_state.type_name_info_map.emplace(actor_def->name, struct_type_info);
}

void emit_behaviour_struct(
    GenState& gen_state,
    const std::shared_ptr<TopLevelItem::Behaviour>& behaviour_def) {
    assert(gen_state.curr_actor != nullptr);
    std::string actor_name = gen_state.curr_actor->name;
    std::string be_struct_llvm = llvm_struct_of_behaviour(behaviour_def->name, actor_name);

    std::vector<std::pair<std::string, std::string>> struct_mem_vec;
    struct_mem_vec.reserve(behaviour_def->params.size() + 1);

    for (auto const& var_decl : behaviour_def->params) {
        struct_mem_vec.push_back({
            var_decl.name,
            llvm_type_of_coh_type(gen_state, var_decl.type)->llvm_type_name
        });
    }

    struct_mem_vec.push_back({THIS_ACTOR_ID_REG, "i64"});

    map_emit_struct<std::pair<std::string, std::string>>(
        gen_state.out_stream,
        be_struct_llvm,
        struct_mem_vec,
        [&](const std::pair<std::string, std::string>& m) { return m.second; }
    );
}

void generate_llvm_structs(GenState& gen_state, Program* program_ast) {
    assert(gen_state.curr_actor == nullptr);
    for(const TopLevelItem& toplevel_item: program_ast->top_level_items) {
        std::visit(Overload{
            [&](const TopLevelItem::TypeDef& type_def) {
                emit_type_definition(gen_state, type_def);
            },
            [&](std::shared_ptr<TopLevelItem::Actor> actor_def) {
                emit_actor_struct(gen_state, actor_def);
                gen_state.curr_actor = actor_def;
                Defer d([&](){gen_state.curr_actor = nullptr;});
                for(auto actor_mem: actor_def->actor_members) {
                    std::visit(Overload{
                        [&](std::shared_ptr<TopLevelItem::Behaviour> be) {
                            emit_behaviour_struct(gen_state, be);
                        },
                        [&](const auto&){}
                    }, actor_mem);
                }
            },
            [&](std::shared_ptr<TopLevelItem::Func> func) {}
        }, toplevel_item.t);
    }
}