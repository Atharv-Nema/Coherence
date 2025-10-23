#pragma once
#include "types.hpp"
#include <memory>

struct BasicValue {
    struct VUnit {};
    struct VInt { int v; };
    struct VFloat { double v; };
    struct VBool { bool v; };
    struct Struct { std::vector<std::pair<std::string, std::shared_ptr<Value>>> fields; }; 
    struct VVar { std::string name; };
    std::variant<VUnit, VInt, VFloat, VBool, Struct, VVar> value;
};

struct NewInstance {
    BasicType type;
    Cap cap;
    BasicValue default_val;
};

struct Value {
    struct Basic { BasicValue value; };
    struct New { NewInstance inst; };
    struct Array { int size; NewInstance elem_type; };
    std::variant<Basic, New, Array> value;
};
