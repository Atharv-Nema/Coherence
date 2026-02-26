#pragma once
#include "top_level.hpp"

void print_cap(const Cap& c);
void print_type(std::shared_ptr<const Type> type);
void print_binop(BinOp op);
void print_val_expr(const ValExpr& v);
void print_stmt(const Stmt& s);
void print_func(const TopLevelItem::Func& f);
void print_constructor(const TopLevelItem::Constructor& c);
void print_behaviour(const TopLevelItem::Behaviour& b);
void print_actor(const TopLevelItem::Actor& a);
void print_typedef(const TopLevelItem::TypeDef& t);
void print_top_level_item(const TopLevelItem& item);
void print_program(const Program& program);