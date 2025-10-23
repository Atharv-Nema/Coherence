#pragma once
#include "values.hpp"

enum class BinOp { Add, Sub, Mul, Div, Geq, Leq, Eq, Neq, Gt, Lt };

struct AssignableExpr {
    struct Var { std::string name; };
    struct Deref { std::shared_ptr<AssignableExpr> inner; };
    struct Field { std::shared_ptr<AssignableExpr> base;
                   std::vector<std::string> fields; };
    std::variant<Var, Deref, Field> value;
};

struct ValueExpr {
    struct ValueE { std::shared_ptr<Value> val; };
    struct BinOpE { std::shared_ptr<ValueExpr> lhs; BinOp op; std::shared_ptr<ValueExpr> rhs; };
    struct NotE { std::shared_ptr<ValueExpr> inner; };
    struct ConsumeE { std::string name; };
    struct CallE { std::string func; std::vector<std::shared_ptr<ValueExpr>> args; };
    struct AssignableE { std::shared_ptr<AssignableExpr> expr; };
    std::variant<ValueE, BinOpE, NotE, ConsumeE, CallE, AssignableE> value;
};