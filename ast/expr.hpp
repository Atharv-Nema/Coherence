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
        std::shared_ptr<ValExpr> size;
    };
    struct ActorConstruction {
        std::string actor_name;
        std::string constructor_name;
        std::vector<std::shared_ptr<ValExpr>> args; 
    };

    // Accesses
    struct PointerAccess {
        std::shared_ptr<ValExpr> index;
        std::shared_ptr<ValExpr> value;
    };
    struct Field { std::shared_ptr<ValExpr> base;
                   std::string field; };
    
    // Assignments
    struct Assignment{ 
        std::shared_ptr<ValExpr> lhs; 
        std::shared_ptr<ValExpr> rhs; 
    };

    // Callable calls
    struct FuncCall { 
        std::string func; 
        std::vector<std::shared_ptr<ValExpr>> args; 
    };
    
    // Operations
    struct BinOpExpr { std::shared_ptr<ValExpr> lhs; BinOp op; std::shared_ptr<ValExpr> rhs; };

    SourceSpan source_span;
    FullType expr_type;
    std::variant<VUnit, VInt, VBool, VVar, VStruct, NewInstance, ActorConstruction, 
    PointerAccess, Field, Assignment, FuncCall, BinOpExpr> t;
};


