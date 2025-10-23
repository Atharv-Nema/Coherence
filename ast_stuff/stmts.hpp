#pragma once
#include "expr.hpp"

struct Assignment {
    std::shared_ptr<AssignableExpr> target;
    std::shared_ptr<ValueExpr> rhs;
};

struct VarDeclWithInit {
    std::string name;
    FullType type;
    Value init;
};

struct Stmt;

struct IfStmt {
    std::shared_ptr<ValueExpr> cond;
    std::vector<std::shared_ptr<Stmt>> then_body;
    std::optional<std::vector<std::shared_ptr<Stmt>>> else_body;
};

struct WhileStmt {
    std::shared_ptr<ValueExpr> cond;
    std::vector<std::shared_ptr<Stmt>> body;
};

struct AtomicStmt {
    std::vector<std::string> locks;
    std::vector<std::shared_ptr<Stmt>> body;
};

// A single statement node
struct Stmt {
    struct DeclInit { VarDeclWithInit decl; };
    struct Assign { Assignment assign; };
    struct Expr { std::shared_ptr<ValueExpr> expr; };
    struct If { IfStmt ifs; };
    struct While { WhileStmt whiles; };
    struct Return { std::shared_ptr<ValueExpr> expr; };
    struct Atomic { AtomicStmt atomic; };

    std::variant<DeclInit, Assign, Expr, If, While, Return, Atomic> value;
};