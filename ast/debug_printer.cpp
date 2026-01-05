#include "top_level.hpp"
#include "../global_utils/pattern_matching_boilerplate.hpp"
#include <iostream>

// Types
void print_basic_type(const BasicType& b) {
    std::visit(Overload{
        [&](const BasicType::TUnit&) {
            std::cout << "TUnit";
        },
        [&](const BasicType::TInt&) {
            std::cout << "TInt";
        },
        [&](const BasicType::TBool&) {
            std::cout << "TBool";
        },
        [&](const BasicType::TNamed& x) {
            std::cout << "TNamed{" << x.name << "}";
        },
        [&](const BasicType::TActor& x) {
            std::cout << "TActor{" << x.name << "}";
        }
    }, b.t);
}

void print_cap(const Cap& c) {
    std::visit(Overload{
        [&](const Cap::Ref&) {
            std::cout << "Ref";
        },
        [&](const Cap::Val&) {
            std::cout << "Val";
        },
        [&](const Cap::Iso&) {
            std::cout << "Iso";
        },
        [&](const Cap::Iso_cap&) {
            std::cout << "Iso_cap";
        },
        [&](const Cap::Locked& x) {
            std::cout << "Locked(" << x.lock_name << ")";
        }
    }, c.t);
}

void print_full_type(const FullType& ft) {
    std::visit(Overload{
        [&](const BasicType& b) {
            std::cout << "Basic(";
            print_basic_type(b);
            std::cout << ")";
        },
        [&](const FullType::Pointer& p) {
            // Print capability first, then the base type, pointer-style
            std::cout << "Ptr(";
            print_basic_type(p.base);
            std::cout << ", ";
            print_cap(p.cap);
            std::cout << ")";
        }
    }, ft.t);
}

// Value expressions
void print_binop(BinOp op) {
    switch (op) {
        case BinOp::Add: std::cout << "Add"; break;
        case BinOp::Sub: std::cout << "Sub"; break;
        case BinOp::Mul: std::cout << "Mul"; break;
        case BinOp::Div: std::cout << "Div"; break;
        case BinOp::Geq: std::cout << "Geq"; break;
        case BinOp::Leq: std::cout << "Leq"; break;
        case BinOp::Eq:  std::cout << "Eq"; break;
        case BinOp::Neq: std::cout << "Neq"; break;
        case BinOp::Gt:  std::cout << "Gt"; break;
        case BinOp::Lt:  std::cout << "Lt"; break;
    }
}


void print_val_expr(const ValExpr& v) {
    std::visit(Overload{

        // --- Simple values ---
        [&](const ValExpr::VUnit&) {
            std::cout << "VUnit{}";
        },
        [&](const ValExpr::VInt& x) {
            std::cout << "VInt{v=" << x.v << "}";
        },
        [&](const ValExpr::VBool& x) {
            std::cout << "VBool{v=" << (x.v ? "true" : "false") << "}";
        },

        // --- Variable ---
        [&](const ValExpr::VVar& x) {
            std::cout << "VVar{name=" << x.name << "}";
        },

        // --- Struct literal ---
        [&](const ValExpr::VStruct& s) {
            std::cout << "VStruct{type=" << s.type_name << ", fields=[";
            bool first = true;
            for (const auto& [name, val] : s.fields) {
                if (!first) std::cout << ", ";
                std::cout << name << "=";
                print_val_expr(*val);
                first = false;
            }
            std::cout << "]}";
        },

        // --- Allocation ---
        [&](const ValExpr::NewInstance& n) {
            std::cout << "NewInstance{type=";
            print_basic_type(n.type);
            std::cout << ", cap=";
            print_cap(n.cap);
            std::cout << ", size=";
            print_val_expr(*n.size);
            std::cout << ", default=";
            print_val_expr(*n.default_value);
            std::cout << "}";
        },

        // --- Actor construction ---
        [&](const ValExpr::ActorConstruction& a) {
            std::cout << "ActorConstruction{actor=" << a.actor_name
                      << ", ctor=" << a.constructor_name
                      << ", args=[";
            for (size_t i = 0; i < a.args.size(); i++) {
                if (i > 0) std::cout << ", ";
                print_val_expr(*a.args[i]);
            }
            std::cout << "]}";
        },

        // --- Consume --- 
        [&](const ValExpr::Consume& c) {
            std::cout << "Conume{var=" << c.var_name << "}" << std::endl;
        },

        // --- Pointer access ---
        [&](const ValExpr::PointerAccess& p) {
            std::cout << "PointerAccess{value=";
            print_val_expr(*p.value);
            std::cout << ", index=";
            print_val_expr(*p.index);
            std::cout << "}";
        },

        // --- Struct field access ---
        [&](const ValExpr::Field& f) {
            std::cout << "Field{base=";
            print_val_expr(*f.base);
            std::cout << ", field=" << f.field << "}";
        },

        // --- Assignment ---
        [&](const ValExpr::Assignment& a) {
            std::cout << "Assignment{lhs=";
            print_val_expr(*a.lhs);
            std::cout << ", rhs=";
            print_val_expr(*a.rhs);
            std::cout << "}";
        },

        // --- Function call ---
        [&](const ValExpr::FuncCall& c) {
            std::cout << "FuncCall{func=" << c.func << ", args=[";
            for (size_t i = 0; i < c.args.size(); i++) {
                if (i > 0) std::cout << ", ";
                print_val_expr(*c.args[i]);
            }
            std::cout << "]}";
        },

        // --- Binary op ---
        [&](const ValExpr::BinOpExpr& b) {
            std::cout << "BinOpExpr{lhs=";
            print_val_expr(*b.lhs);
            std::cout << ", op=";
            print_binop(b.op);
            std::cout << ", rhs=";
            print_val_expr(*b.rhs);
            std::cout << "}";
        }
    }, v.t);
}



