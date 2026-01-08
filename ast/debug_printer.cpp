#pragma once
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

void print_stmt(const Stmt& s) {
    std::visit(Overload{

        [&](const Stmt::VarDeclWithInit& v) {
            std::cout << "VarDeclWithInit{name=" << v.name << ", type=";
            print_full_type(v.type);
            std::cout << ", init=";
            print_val_expr(*v.init);
            std::cout << "}\n";
        },

        [&](const Stmt::MemberInitialize& m) {
            std::cout << "MemberInitialize{member=" << m.member_name << ", init=";
            print_val_expr(*m.init);
            std::cout << "}\n";
        },

        [&](const Stmt::BehaviourCall& b) {
            std::cout << "BehaviourCall{actor=";
            print_val_expr(*b.actor);
            std::cout << ", behaviour=" << b.behaviour_name << ", args=[";
            for (size_t i = 0; i < b.args.size(); ++i) {
                if (i > 0) std::cout << ", ";
                print_val_expr(*b.args[i]);
            }
            std::cout << "]}\n";
        },

        [&](const Stmt::Print& p) {
            std::cout << "Print{expr=";
            print_val_expr(*p.print_expr);
            std::cout << "}\n";
        },

        [&](const Stmt::Expr& e) {
            std::cout << "Expr{";
            print_val_expr(*e.expr);
            std::cout << "}\n";
        },

        [&](const Stmt::If& i) {
            std::cout << "If{cond=";
            print_val_expr(*i.cond);
            std::cout << "}\n";

            std::cout << "Then:\n";
            for (const auto& stmt : i.then_body) {
                print_stmt(*stmt);
            }

            if (i.else_body) {
                std::cout << "Else:\n";
                for (const auto& stmt : *i.else_body) {
                    print_stmt(*stmt);
                }
            }
        },

        [&](const Stmt::While& w) {
            std::cout << "While{cond=";
            print_val_expr(*w.cond);
            std::cout << "}\n";

            std::cout << "Body:\n";
            for (const auto& stmt : w.body) {
                print_stmt(*stmt);
            }
        },

        [&](const std::shared_ptr<Stmt::Atomic>& a) {
            std::cout << "Atomic{locks=[";
            if (a->locks_dereferenced) {
                bool first = true;
                for (const auto& l : *a->locks_dereferenced) {
                    if (!first) std::cout << ", ";
                    std::cout << l;
                    first = false;
                }
            }
            std::cout << "]}\n";

            std::cout << "Body:\n";
            for (const auto& stmt : a->body) {
                print_stmt(*stmt);
            }
        },

        [&](const Stmt::Return& r) {
            std::cout << "Return{expr=";
            print_val_expr(*r.expr);
            std::cout << "}\n";
        }

    }, s.t);
}

void print_func(const TopLevelItem::Func& f) {
    std::cout << "Func{name=" << f.name << ", return_type=";
    print_full_type(f.return_type);
    std::cout << "}\n";

    std::cout << "Params:\n";
    for(const TopLevelItem::VarDecl& var_decl: f.params) {
        std::cout << "VarDecl{name=" << var_decl.name << ", type=";
        print_full_type(var_decl.type);
        std::cout << "}";
        std::cout << "\n";
    }

    std::cout << "Body:\n";
    for(const auto& stmt : f.body) {
        print_stmt(*stmt);
    }
}

void print_constructor(const TopLevelItem::Constructor& c) {
    std::cout << "Constructor{name=" << c.name << "}\n";

    std::cout << "Params:\n";
    for(const TopLevelItem::VarDecl& var_decl: c.params) {
        std::cout << "VarDecl{name=" << var_decl.name << ", type=";
        print_full_type(var_decl.type);
        std::cout << "}";
        std::cout << "\n";
    }

    std::cout << "Body:\n";
    for (const auto& stmt : c.body) {
        print_stmt(*stmt);
    }
}

void print_behaviour(const TopLevelItem::Behaviour& b) {
    std::cout << "Behaviour{name=" << b.name << "}\n";

    std::cout << "Params:\n";
    for(const TopLevelItem::VarDecl& var_decl: b.params) {
        std::cout << "VarDecl{name=" << var_decl.name << ", type=";
        print_full_type(var_decl.type);
        std::cout << "}";
        std::cout << "\n";
    }

    std::cout << "Body:\n";
    for (const auto& stmt : b.body) {
        print_stmt(*stmt);
    }
}

void print_actor(const TopLevelItem::Actor& a) {
    std::cout << "Actor{name=" << a.name << "}\n";

    std::cout << "MemberVars:\n";
    for (const auto& [name, type] : a.member_vars) {
        std::cout << name << ": ";
        print_full_type(type);
        std::cout << "\n";
    }

    std::cout << "ActorMembers:\n";
    for (const auto& mem : a.actor_members) {
        std::visit(Overload{
            [&](const std::shared_ptr<TopLevelItem::Func>& f) {
                print_func(*f);
            },
            [&](const std::shared_ptr<TopLevelItem::Constructor>& c) {
                print_constructor(*c);
            },
            [&](const std::shared_ptr<TopLevelItem::Behaviour>& b) {
                print_behaviour(*b);
            }
        }, mem);
    }
}

void print_typedef(const TopLevelItem::TypeDef& t) {
    std::cout << "TypeDef{name=" << t.type_name << "}\n";
}


void print_top_level_item(const TopLevelItem& item) {
    std::visit(Overload{
        [&](const TopLevelItem::TypeDef& t) {
            print_typedef(t);
        },
        [&](const std::shared_ptr<TopLevelItem::Func>& f) {
            print_func(*f);
        },
        [&](const std::shared_ptr<TopLevelItem::Actor>& a) {
            print_actor(*a);
        }
    }, item.t);
}

void print_program(const Program& program) {
    std::cout << "Program\n";
    for (const auto& item : program.top_level_items) {
        print_top_level_item(item);
    }
}




