#pragma once
#include "expr.hpp"
#include <unordered_set>

// These define everything that can be in a line ending with a semi-colon as a part of a body (so not
// top level function declarations and stuff like that). Unlike expr, it also includes statements
// that do not return anything, like if and atomic. The purpose of separating stmts and expr is to 
// prevent weird things like an if in the condition of an if and so on (although perhaps I could deal
// with the types of such weirdness, but I don't want to for now)

struct Stmt {
    struct VarDeclWithInit { 
        std::string name;
        FullType type;
        std::shared_ptr<ValExpr> init; 
    };
    struct MemberInitialize {
        std::string member_name;
        std::shared_ptr<ValExpr> init;
    };
    struct BehaviourCall {
        std::shared_ptr<ValExpr> actor;
        std::string behaviour_name;
        std::vector<std::shared_ptr<ValExpr>> args; 
    };
    struct Expr { std::shared_ptr<ValExpr> expr; };
    struct If { 
        std::shared_ptr<ValExpr> cond;
        std::vector<std::shared_ptr<Stmt>> then_body;
        std::optional<std::vector<std::shared_ptr<Stmt>>> else_body;
    };
    struct While { 
        std::shared_ptr<ValExpr> cond;
        std::vector<std::shared_ptr<Stmt>> body; 
    };
    struct Atomic { 
        std::unordered_set<std::string> locks_dereferenced;
        std::vector<std::shared_ptr<Stmt>> body;
    };
    struct Return { std::shared_ptr<ValExpr> expr; };
    SourceSpan source_span;
    std::variant<VarDeclWithInit, MemberInitialize, BehaviourCall, Expr, If, While, std::shared_ptr<Atomic>, Return> t;
};