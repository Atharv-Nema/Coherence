#pragma once
#include "stmts.hpp"

// TODO: Change the type here to include a span probably
using TypeDef = std::pair<std::string, NameableType>;

struct TopLevelItem {
    struct VarDecl {
        std::string name;
        FullType type;
    };
    struct Func {
        std::string name;
        FullType return_type;
        std::vector<VarDecl> params;
        std::vector<std::shared_ptr<Stmt>> body;
    };
    struct ActorEndpoints {
        std::string name;
        std::vector<VarDecl> params;
        std::vector<std::shared_ptr<Stmt>> body;
    };
    struct Actor {
        std::string name;
        std::vector<VarDecl> member_vars;
        std::vector<Func> member_funcs;
        std::vector<ActorEndpoints> constructors;
        std::vector<ActorEndpoints> member_behaviours;
    };
    SourceSpan source_span;
    std::variant<TypeDef, Func, Actor> t;
};

struct Program {
    std::vector<TopLevelItem> top_level_items;
};
