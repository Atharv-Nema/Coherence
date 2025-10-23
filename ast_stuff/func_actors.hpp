#pragma once
#include "stmts.hpp"

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

// ---------- Actors ----------

struct Behaviour {
    std::string name;
    std::vector<VarDecl> params;
    std::vector<std::shared_ptr<Stmt>> body;
};

struct Actor {
    std::string name;
    std::vector<VarDecl> member_vars;
    std::vector<Func> member_funcs;
    std::vector<Behaviour> member_behaviours;
};