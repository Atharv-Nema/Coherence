#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdio>
#include "top_level.hpp"

enum class CallableType {
    FUNCTION,
    CONSTRUCTOR
};

class CallableLockInfo {
private:
    using LockSet = std::shared_ptr<std::unordered_set<std::string>>;

    using ActorData =
        std::unordered_map<
            CallableType,
            std::unordered_map<std::string, LockSet>>;

    std::unordered_map<std::optional<std::string>, ActorData> parsed_data;

    void include_callable(
        const std::optional<std::string>& actor_name,
        CallableType type,
        const std::string& name,
        LockSet locks
    );
public:
    explicit CallableLockInfo(FILE* file);
    std::vector<std::string> lock_info(
        const std::string& callable_name,
        CallableType type = CallableType::FUNCTION,
        const std::optional<std::string>& actor_name = std::nullopt
    );
};
