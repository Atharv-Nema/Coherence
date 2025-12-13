#pragma once
#include "stmts.hpp"
#include <unordered_map>

// TODO: Change the type here to include a span probably

struct TopLevelItem {
    struct TypeDef {
        std::string type_name;
        std::shared_ptr<NameableType> nameable_type;
    };
    struct VarDecl {
        std::string name;
        FullType type;
    };
    struct Func {
        std::string name;
        FullType return_type;
        std::vector<VarDecl> params;
        std::vector<std::shared_ptr<Stmt>> body;
        std::unordered_set<std::string> locks_dereferenced;
    };
    struct Behaviour {
        std::string name;
        std::vector<VarDecl> params;
        std::vector<std::shared_ptr<Stmt>> body;
    };
    struct Constructor {
        std::string name;
        std::vector<VarDecl> params;
        std::vector<std::shared_ptr<Stmt>> body;
        std::unordered_set<std::string> locks_dereferenced;
    };
    struct Actor {
        std::string name;
        std::unordered_map<std::string, FullType> member_vars;
        std::vector<std::variant<
            std::shared_ptr<Func>,
            std::shared_ptr<Constructor>,
            std::shared_ptr<Behaviour>
        >> actor_members;
    };
    SourceSpan source_span;
    std::variant<TypeDef, std::shared_ptr<Func>, std::shared_ptr<Actor>> t;
};

struct Program {
    std::vector<TopLevelItem> top_level_items;
};
