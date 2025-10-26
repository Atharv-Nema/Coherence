#pragma once
#include "types.hpp"
#include <memory>


// Struct for storing additional information for error messages
struct SourceLoc {
    int line;
    int char_no;
};

struct SourceSpan {
    SourceLoc start;
    SourceLoc end;
};

// Exclusively for statements that represent computation and not control flow (ie, result in a value)

enum class BinOp { Add, Sub, Mul, Div, Geq, Leq, Eq, Neq, Gt, Lt };

struct ValExpr {
    // Simple values
    struct VUnit {};
    struct VInt { int v; };
    struct VFloat { double v; };
    struct VBool { bool v; };

    // Named values (variables)
    struct VVar { std::string name; };

    // Structs
    struct VStruct { 
        std::vector<std::pair<std::string, std::shared_ptr<ValExpr>>> fields;
        std::string type_name;
    };
    
    // Allocations
    struct NewInstance {
        BasicType type;
        Cap cap;
        std::shared_ptr<ValExpr> default_value;
    };
    struct Array {
        int size;
        NewInstance element_info;
    };
    struct ActorConstruction {
        std::string actor_name;
        std::string constructor_name;
        std::vector<std::shared_ptr<ValExpr>> args; 
    };

    // Accesses
    struct ArrayAccess {
        int index;
        std::shared_ptr<ValExpr> value;
    };
    struct Deref { std::shared_ptr<ValExpr> inner; };
    struct Field { std::shared_ptr<ValExpr> base;
                   std::vector<std::string> fields; };
    
    // Assignments
    struct Assignment{ 
        std::shared_ptr<ValExpr> lhs; 
        std::shared_ptr<ValExpr> rhs; 
    };

    // Callable calls
    struct FuncCall { 
        std::string func; std::vector<std::shared_ptr<ValExpr>> args; 
    };

    struct BeCall {
        std::shared_ptr<ValExpr> actor;
        std::string behaviour_name;
        std::vector<std::shared_ptr<ValExpr>> args; 
    };
    
    // Operations
    struct BinOpExpr { std::shared_ptr<ValExpr> lhs; BinOp op; std::shared_ptr<ValExpr> rhs; };
    struct ConsumeE { std::string name; };

    SourceSpan source_span;
    std::variant<VUnit, VInt, VFloat, VBool, VVar, VStruct, NewInstance, Array, ActorConstruction, 
    ArrayAccess, Deref, Field, Assignment, FuncCall, BeCall, BinOpExpr, ConsumeE> t;
};

// struct AssignableExpr {
//     struct Var { std::string name; };
//     struct Deref { std::shared_ptr<AssignableExpr> inner; };
//     struct Field { std::shared_ptr<AssignableExpr> base;
//                    std::vector<std::string> fields; };
//     std::variant<Var, Deref, Field> value;
// };
