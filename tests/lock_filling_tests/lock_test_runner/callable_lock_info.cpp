#include <algorithm>
#include <assert.h>
#include "parse_file.hpp"
#include "ast_validator.hpp"
#include "pattern_matching_boilerplate.hpp"
#include "callable_lock_info.hpp"

void CallableLockInfo::include_callable(
    const std::optional<std::string>& actor_name,
    CallableType type,
    const std::string& name,
    LockSet locks) {
    assert(locks != nullptr);
    parsed_data[actor_name][type][name] = locks;
}

CallableLockInfo::CallableLockInfo(FILE* file) {
    Program* program = parse_file(file);
    bool type_checks = validate_program(program);
    assert(type_checks);
    assert(program != nullptr);

    for (const TopLevelItem& item : program->top_level_items) {
        std::visit(Overload{
            [&](std::shared_ptr<TopLevelItem::Func> func) {
                include_callable(
                    std::nullopt,
                    CallableType::FUNCTION,
                    func->name,
                    func->locks_dereferenced);
            },

            [&](std::shared_ptr<TopLevelItem::Actor> actor) {
                for (const auto& member : actor->actor_members) {
                    std::visit(Overload{
                        [&](std::shared_ptr<TopLevelItem::Func> func) {
                            include_callable(
                                actor->name,
                                CallableType::FUNCTION,
                                func->name,
                                func->locks_dereferenced);
                        },

                        [&](std::shared_ptr<TopLevelItem::Constructor> constructor) {
                            include_callable(
                                actor->name,
                                CallableType::CONSTRUCTOR,
                                constructor->name,
                                constructor->locks_dereferenced);
                        },
                        [&](auto const&) {}
                    }, member);
                }
            },
            [&](auto const&) {}
        }, item.t);


    }
}

std::vector<std::string> CallableLockInfo::lock_info(
    const std::string& callable_name,
    CallableType type,
    const std::optional<std::string>& actor_name) {
    assert(parsed_data.contains(actor_name));
    auto& actor_data = parsed_data.at(actor_name);
    assert(actor_data.contains(type));
    auto& type_data = actor_data[type];
    assert(type_data.contains(callable_name));
    auto lock_info = type_data.at(callable_name);
    std::vector<std::string> locks(lock_info->begin(), lock_info->end());
    sort(locks.begin(), locks.end());
    return locks;
}

