#pragma once
#include "expr.hpp"
#include <unordered_set>

struct Stmt {
    struct VarDeclWithInit { 
        std::string name;
        std::shared_ptr<const Type> type;
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
    struct Print {
        std::shared_ptr<ValExpr> print_expr;
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
        std::shared_ptr<std::unordered_set<std::string>> locks_dereferenced;
        std::vector<std::shared_ptr<Stmt>> body;
    };
    struct Return { std::shared_ptr<ValExpr> expr; };
    SourceSpan source_span;
    std::variant<VarDeclWithInit, MemberInitialize, BehaviourCall, Print, Expr, If, While, std::shared_ptr<Atomic>, Return> t;
};