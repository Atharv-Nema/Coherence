/* ---------- HEADER SECTION (shared across .cc and .hh) ---------- */
%code requires {
    #include "top_level.hpp"
    #include <memory>
    #include <string>
    #include <vector>
    #include <unordered_set>
    #include <unordered_map>
    #include <iostream>
    #include <assert.h>

    using namespace std;

    // yyscan_t is defined in lex.yy.h, but we forward-declare it here
    typedef void* yyscan_t;
}

/* Enable conflict warnings and diagnostics */
%define parse.error detailed
%define lr.type ielr
%define parse.lac full
%expect 0

/* ---------- BISON CONFIGURATION ---------- */
%define api.pure full
%locations
%lex-param   { yyscan_t yyscanner }
%parse-param { yyscan_t yyscanner }

/* ---------- IMPLEMENTATION-ONLY SECTION (.cc only) ---------- */
%code {
    #include <iostream>
    #include "lex.yy.h"   // from Flex; gives yyget_text, yyscan_t, etc.

    Program* program_root = nullptr;

    void yyerror(YYLTYPE* loc, yyscan_t scanner, const char* msg) {
        std::cerr << "Parse error at line "
                  << loc->first_line << ", column " << loc->first_column
                  << ": " << msg
                  << " (near '" << yyget_text(scanner) << "')"
                  << std::endl;
    }
}

/* ---------- SEMANTIC VALUE UNION ---------- */
%union {
    int int_val;
    double float_val;
    string* str_val;
    shared_ptr<ValExpr>* val_expr;
    vector<shared_ptr<ValExpr>>* val_expr_list;
    shared_ptr<Stmt>* stmt;
    vector<shared_ptr<Stmt>>* stmt_list;
    BasicType* basic_type;
    FullType* full_type;
    Cap* cap;
    NameableType::Struct* struct_fields;
    ValExpr::VStruct* struct_instance;
    vector<TopLevelItem>* top_level_list;
    TopLevelItem* top_item;
    vector<TopLevelItem::VarDecl>* var_list;
    std::shared_ptr<TopLevelItem::Func>* func;
    std::shared_ptr<TopLevelItem::Actor>* actor;
    std::unordered_map<std::string, FullType>* actor_fields;
    std::shared_ptr<TopLevelItem::Constructor>* actor_constructor;
    std::shared_ptr<TopLevelItem::Behaviour>* actor_behaviour;
    Program* program;
    TopLevelItem::TypeDef* type_def;
}

/* ---------- TOKENS ---------- */
%token TOK_ACTOR TOK_NEW TOK_FUNC TOK_BE TOK_RETURN
%token TOK_ATOMIC TOK_IF TOK_ELSE TOK_WHILE TOK_DOT TOK_OUT
%token TOK_TYPE TOK_STRUCT TOK_INITIALIZE
%token TOK_INT TOK_BOOL
%token TOK_REF TOK_ISO TOK_VAL TOK_LOCKED
%token TOK_TRUE TOK_FALSE
%token TOK_SEND TOK_ARROW TOK_ASSIGN TOK_CONSUME
%token TOK_LEQ TOK_GEQ TOK_LESS TOK_GREATER TOK_EQ TOK_NEQ
%token TOK_LPAREN TOK_RPAREN TOK_LBRACE TOK_RBRACE TOK_LSQUARE TOK_RSQUARE
%token TOK_COLON TOK_SEMI TOK_COMMA
%token TOK_PLUS TOK_MINUS TOK_STAR TOK_SLASH
%token TOK_VAR

%token <int_val>   TOK_INT_LIT
%token <str_val>   TOK_IDENT

/* ---------- TYPE DECLARATIONS ---------- */
%type <program>         program
%type <top_level_list>  top_level_items
%type <top_item>        top_level_item
%type <stmt>            stmt
%type <stmt_list>       stmt_list block
%type <val_expr>        val_expr
%type <val_expr>        assignment_expr comparison_expr additive_expr multiplicative_expr postfix_expr primary_expr
%type <val_expr_list>   val_expr_list nonempty_val_expr_list
%type <basic_type>      basic_type
%type <full_type>       full_type
%type <cap>             cap
%type <struct_fields>   struct_fields
%type <struct_instance> struct_instance
%type <var_list>        func_params 
%type <actor_fields>    actor_fields
%type <actor>           actor_def actor_members
%type <actor_constructor> actor_constructor
%type <actor_behaviour> actor_behaviour
%type <func>            func_def
%type <type_def>        type_def

/* ---------- OPERATOR PRECEDENCE ---------- */
// Not needed for now

/* ---------- HELPER CODE ---------- */
%code {
    inline SourceSpan span_from(const YYLTYPE& loc) {
        return { {loc.first_line, loc.first_column},
                 {loc.last_line,  loc.last_column} };
    }
}

%%

/* ---------- PROGRAM ---------- */
program
    : top_level_items {
        program_root = new Program();
        program_root->top_level_items = std::move(*$1);
        delete $1;
      }
    ;

/* ---------- TOP LEVEL ITEMS ---------- */
top_level_items
    : top_level_item {
        $$ = new vector<TopLevelItem>();
        $$->emplace_back(std::move(*$1));
        delete $1;
      }
    | top_level_items top_level_item {
        $$ = $1;
        $$->emplace_back(std::move(*$2));
        delete $2;
      }
    ;

top_level_item
    : actor_def { 
        $$ = new TopLevelItem(); 
        $$->t = std::move(*$1); 
        $$->source_span = span_from(@1); 
        delete $1;
      }
    | func_def  { 
        $$ = new TopLevelItem(); 
        $$->t = std::move(*$1); 
        $$->source_span = span_from(@1); 
        delete $1;
      }
    | type_def  { 
        $$ = new TopLevelItem(); 
        $$->t = std::move(*$1); 
        $$->source_span = span_from(@1); 
        delete $1;
      }
    ;

/* ---------- TYPE DEFINITIONS ---------- */
type_def
    : TOK_TYPE TOK_IDENT TOK_ASSIGN TOK_STRUCT TOK_LBRACE struct_fields TOK_RBRACE {
        const std::string& name = *$2;
        if (std::isupper(name[0])) {
            yyerror(&@2, yyscanner,
                    "User defined type names must not begin with a capital letter");
            YYERROR;
        }
        $$ = new TopLevelItem::TypeDef(
            std::move(*$2),
            make_shared<NameableType>(NameableType::Struct{ std::move($6->members) })
        );
        delete $2; delete $6;
      }
    | TOK_TYPE TOK_IDENT TOK_ASSIGN basic_type {
        const std::string& name = *$2;
        if (std::isupper(name[0])) {
            yyerror(&@2, yyscanner,
                    "User defined type names must not begin with a capital letter");
            YYERROR;
        }
        $$ = new TopLevelItem::TypeDef(
            std::move(*$2),
            make_shared<NameableType>(NameableType::Basic{ std::move(*$4) })
        );
        delete $2; delete $4;
      }
    ;

struct_fields
    : %empty { $$ = new NameableType::Struct(); }
    | struct_fields TOK_IDENT TOK_COLON basic_type TOK_SEMI {
        $$ = $1;
        $$->members.emplace_back(std::move(*$2), std::move(*$4));
        delete $2; delete $4;
      }
    ;

struct_instance
    : %empty { $$ = new ValExpr::VStruct(); }
    | struct_instance TOK_IDENT TOK_ASSIGN val_expr TOK_SEMI {
        $$ = $1;
        $$->fields.emplace_back(std::move(*$2), std::move(*$4));
        delete $2; delete $4;
      }
    ;

/* ---------- ACTOR DEFINITION ---------- */
actor_def
    : TOK_ACTOR TOK_IDENT TOK_LBRACE actor_fields actor_members TOK_RBRACE {
        const std::string& name = *$2;
        if (!std::isupper(name[0])) {
            yyerror(&@2, yyscanner,
                    "Actor names must begin with a capital letter");
            YYERROR;
        }
        $$ = $5;
        (*$$)->name = std::move(*$2);
        (*$$)->member_vars = std::move(*$4);
        delete $2; delete $4;
      }
    ;

/* ---------- ACTOR FIELDS ---------- */
actor_fields
    : %empty { $$ = new unordered_map<std::string, FullType>(); }
    | actor_fields TOK_IDENT TOK_COLON full_type TOK_SEMI {
        $$ = $1;
        $$->emplace(std::move(*$2), std::move(*$4));
        delete $2; delete $4;
      }
    ;

/* ---------- ACTOR MEMBERS ---------- */
actor_members
    : %empty {
        $$ = new shared_ptr<TopLevelItem::Actor>(make_shared<TopLevelItem::Actor>());
      }
    | actor_members actor_constructor {
        $$ = $1;
        (*$$)->actor_members.emplace_back(std::move(*$2));
        delete $2;
      }
    | actor_members func_def {
        $$ = $1;
        (*$$)->actor_members.emplace_back(std::move(*$2));
        delete $2;
      }
    | actor_members actor_behaviour {
        $$ = $1;
        (*$$)->actor_members.emplace_back(std::move(*$2));
        delete $2;
      }
    ;

/* ---------- CONSTRUCTORS ---------- */
actor_constructor
    : TOK_NEW TOK_IDENT TOK_LPAREN func_params TOK_RPAREN block {
        $$ = new shared_ptr<TopLevelItem::Constructor>(
            make_shared<TopLevelItem::Constructor>()
        );
        (*$$)->name = std::move(*$2);
        (*$$)->params = std::move(*$4);
        (*$$)->body = std::move(*$6);
        delete $2; delete $4; delete $6;
      }
    ;

/* ---------- BEHAVIOURS ---------- */
actor_behaviour
    : TOK_BE TOK_IDENT TOK_LPAREN func_params TOK_RPAREN block {
        $$ = new shared_ptr<TopLevelItem::Behaviour>(make_shared<TopLevelItem::Behaviour>());
        (*$$)->name = std::move(*$2);
        (*$$)->params = std::move(*$4);
        (*$$)->body = std::move(*$6);
        delete $2; delete $4; delete $6;
      }
    ;

/* ---------- FUNCTIONS ---------- */
func_def
    : TOK_FUNC TOK_IDENT TOK_LPAREN func_params TOK_RPAREN TOK_ARROW full_type block {
        $$ = new shared_ptr<TopLevelItem::Func>(make_shared<TopLevelItem::Func>());
        (*$$)->name = std::move(*$2);
        (*$$)->return_type = std::move(*$7);
        (*$$)->params = std::move(*$4);
        (*$$)->body = std::move(*$8);
        // This is different from what is being done in atomic because of easy implementation
        // of kosaraju's algorithm in the ast validator
        (*$$)->locks_dereferenced = nullptr;
        delete $2; delete $4; delete $7; delete $8;
      }
    ;

func_params
    : %empty { $$ = new vector<TopLevelItem::VarDecl>(); }
    | func_params full_type TOK_IDENT opt_comma {
        $$ = $1;
        $$->push_back({ std::move(*$3), std::move(*$2) });
        delete $2; delete $3;
      }
    ;

opt_comma
    : %empty
    | TOK_COMMA
    ;

/* ---------- STATEMENTS ---------- */
block
    : TOK_LBRACE stmt_list TOK_RBRACE { $$ = $2; }
    ;

stmt_list
    : %empty { $$ = new vector<shared_ptr<Stmt>>(); }
    | stmt_list stmt {
        $$ = $1;
        $$->push_back(*$2);
        delete $2;
      }
    ;

stmt
    /* var <name> : <type> = <expr>; */
    : TOK_VAR TOK_IDENT TOK_COLON full_type TOK_ASSIGN val_expr TOK_SEMI {
        $$ = new shared_ptr<Stmt>(make_shared<Stmt>(
            Stmt{
                span_from(@$),
                Stmt::VarDeclWithInit{
                    std::move(*$2),   // name
                    std::move(*$4),   // type
                    std::move(*$6)    // init expr
                }
            }
        ));
        delete $2; delete $4; delete $6;
      }
    /* member initialization: field := expr; */
    | TOK_IDENT TOK_INITIALIZE val_expr TOK_SEMI {
        $$ = new shared_ptr<Stmt>(make_shared<Stmt>(
            Stmt{
                span_from(@$),
                Stmt::MemberInitialize{
                    std::move(*$1),
                    std::move(*$3)
                }
            }
        ));
        delete $1; delete $3;
      }
    /* behaviour call: obj->behaviour(args...); */
    | val_expr TOK_SEND TOK_IDENT TOK_LPAREN val_expr_list TOK_RPAREN TOK_SEMI {
        $$ = new shared_ptr<Stmt>(make_shared<Stmt>(
            Stmt{
                span_from(@$),
                Stmt::BehaviourCall{
                    *$1,
                    *$3,
                    std::move(*$5)
                }
            }
        ));
        delete $1; delete $3; delete $5;
      }
    /* expression statement */
    | val_expr TOK_SEMI {
        $$ = new shared_ptr<Stmt>(make_shared<Stmt>(
            Stmt{
                span_from(@$),
                Stmt::Expr{ std::move(*$1) }
            }
        ));
        delete $1;
      }
    
    /* print statement */
    | TOK_OUT val_expr TOK_SEMI {
        $$ = new shared_ptr<Stmt>(make_shared<Stmt>(
            Stmt{
                span_from(@$),
                Stmt::Print{ std::move(*$2) }
            }
        ));
        delete $2;
      }

    /* return */
    | TOK_RETURN val_expr TOK_SEMI {
        $$ = new shared_ptr<Stmt>(make_shared<Stmt>(
            Stmt{
                span_from(@$),
                Stmt::Return{ std::move(*$2) }
            }
        ));
        delete $2;
      }
    /* atomic block */
    | TOK_ATOMIC block {
        $$ = new shared_ptr<Stmt>(make_shared<Stmt>(
            Stmt{
                span_from(@$),
                make_shared<Stmt::Atomic>(
                    std::make_shared<std::unordered_set<std::string>>(), 
                    std::move(*$2))
            }
        ));
        delete $2;
      }
    /* if / if-else */
    | TOK_IF TOK_LPAREN val_expr TOK_RPAREN block {
        $$ = new shared_ptr<Stmt>(make_shared<Stmt>(
            Stmt{
                span_from(@$),
                Stmt::If{
                    std::move(*$3),
                    std::move(*$5),
                    std::nullopt
                }
            }
        ));
        delete $3; delete $5;
      }
    | TOK_IF TOK_LPAREN val_expr TOK_RPAREN block TOK_ELSE block {
        $$ = new shared_ptr<Stmt>(make_shared<Stmt>(
            Stmt{
                span_from(@$),
                Stmt::If{
                    std::move(*$3),
                    std::move(*$5),
                    std::move(*$7)
                }
            }
        ));
        delete $3; delete $5; delete $7;
      }
    /* while */
    | TOK_WHILE TOK_LPAREN val_expr TOK_RPAREN block {
        $$ = new shared_ptr<Stmt>(make_shared<Stmt>(
            Stmt{
                span_from(@$),
                Stmt::While{
                    std::move(*$3),
                    std::move(*$5)
                }
            }
        ));
        delete $3; delete $5;
      }
    ;

/* ---------- EXPRESSIONS ---------- */

/* List of val_exprs (used for parameters sent to function calls) */
val_expr_list
    : %empty {
        $$ = new vector<shared_ptr<ValExpr>>();
      }
    | nonempty_val_expr_list {
        $$ = $1;
      }
    ;

nonempty_val_expr_list
    : val_expr {
        $$ = new vector<shared_ptr<ValExpr>>();
        $$->push_back(*$1);
        delete $1;
      }
    | nonempty_val_expr_list TOK_COMMA val_expr {
        $$ = $1;
        $$->push_back(*$3);
        delete $3;
      }
    ;

/* Top-level expression nonterminal */
val_expr
    : assignment_expr { $$ = $1; }
    ;

/* Assignment: right-associative, lhs is a postfix_expr */
assignment_expr
    : comparison_expr { $$ = $1; }
    | postfix_expr TOK_ASSIGN assignment_expr {
        $$ = new shared_ptr<ValExpr>(make_shared<ValExpr>(
            ValExpr{
                span_from(@$),
                FullType(),
                ValExpr::Assignment{
                    std::move(*$1),
                    std::move(*$3)
                }
            }
        ));
        delete $1; delete $3;
      }
    ;

/* Boolean comparisons: <, <=, >, >=, ==, != */
comparison_expr
    : additive_expr { $$ = $1; }
    | additive_expr TOK_LESS additive_expr {
        $$ = new shared_ptr<ValExpr>(make_shared<ValExpr>(
            ValExpr{ 
                span_from(@$),
                FullType(),
                ValExpr::BinOpExpr{ std::move(*$1), BinOp::Lt, std::move(*$3) }
            }
        ));
        delete $1; delete $3;
      }
    | additive_expr TOK_LEQ additive_expr {
        $$ = new shared_ptr<ValExpr>(make_shared<ValExpr>(
            ValExpr{ 
                span_from(@$),
                FullType(),
                ValExpr::BinOpExpr{ std::move(*$1), BinOp::Leq, std::move(*$3) }
            }
        ));
        delete $1; delete $3;
      }
    | additive_expr TOK_GREATER additive_expr {
        $$ = new shared_ptr<ValExpr>(make_shared<ValExpr>(
            ValExpr{ 
                span_from(@$),
                FullType(),
                ValExpr::BinOpExpr{ std::move(*$1), BinOp::Gt, std::move(*$3) }
            }
        ));
        delete $1; delete $3;
      }
    | additive_expr TOK_GEQ additive_expr {
        $$ = new shared_ptr<ValExpr>(make_shared<ValExpr>(
            ValExpr{ 
                span_from(@$),
                FullType(),
                ValExpr::BinOpExpr{ std::move(*$1), BinOp::Geq, std::move(*$3) }
            }
        ));
        delete $1; delete $3;
      }
    | additive_expr TOK_EQ additive_expr {
        $$ = new shared_ptr<ValExpr>(make_shared<ValExpr>(
            ValExpr{ 
                span_from(@$),
                FullType(),
                ValExpr::BinOpExpr{ std::move(*$1), BinOp::Eq, std::move(*$3) }
            }
        ));
        delete $1; delete $3;
      }
    | additive_expr TOK_NEQ additive_expr {
        $$ = new shared_ptr<ValExpr>(make_shared<ValExpr>(
            ValExpr{ 
                span_from(@$),
                FullType(),
                ValExpr::BinOpExpr{ std::move(*$1), BinOp::Neq, std::move(*$3) }
            }
        ));
        delete $1; delete $3;
      }
    ;


additive_expr
    : multiplicative_expr { $$ = $1; }
    | additive_expr TOK_PLUS multiplicative_expr {
        $$ = new shared_ptr<ValExpr>(make_shared<ValExpr>(
            ValExpr{
                span_from(@$),
                FullType(),
                ValExpr::BinOpExpr{
                    std::move(*$1),
                    BinOp::Add,
                    std::move(*$3)
                }
            }
        ));
        delete $1; delete $3;
      }
    | additive_expr TOK_MINUS multiplicative_expr {
        $$ = new shared_ptr<ValExpr>(make_shared<ValExpr>(
            ValExpr{
                span_from(@$),
                FullType(),
                ValExpr::BinOpExpr{
                    std::move(*$1),
                    BinOp::Sub,
                    std::move(*$3)
                }
            }
        ));
        delete $1; delete $3;
      }
    ;

/* Multiplicative level: * and / */
multiplicative_expr
    : postfix_expr { $$ = $1; }
    | multiplicative_expr TOK_STAR postfix_expr {
        $$ = new shared_ptr<ValExpr>(make_shared<ValExpr>(
            ValExpr{
                span_from(@$),
                FullType(),
                ValExpr::BinOpExpr{
                    std::move(*$1),
                    BinOp::Mul,
                    std::move(*$3)
                }
            }
        ));
        delete $1; delete $3;
      }
    | multiplicative_expr TOK_SLASH postfix_expr {
        $$ = new shared_ptr<ValExpr>(make_shared<ValExpr>(
            ValExpr{
                span_from(@$),
                FullType(),
                ValExpr::BinOpExpr{
                    std::move(*$1),
                    BinOp::Div,
                    std::move(*$3)
                }
            }
        ));
        delete $1; delete $3;
      }
    ;

/* Postfix level: field access and pointer/index access */
postfix_expr
    : primary_expr { $$ = $1; }
    | postfix_expr TOK_LSQUARE val_expr TOK_RSQUARE {
        $$ = new shared_ptr<ValExpr>(make_shared<ValExpr>(
            ValExpr{
                span_from(@$),
                FullType(),
                ValExpr::PointerAccess{
                    std::move(*$3),
                    std::move(*$1)
                }
            }
        ));
        delete $1; delete $3;
      }
    | postfix_expr TOK_DOT TOK_IDENT {
        $$ = new shared_ptr<ValExpr>(make_shared<ValExpr>(
            ValExpr{
                span_from(@$),
                FullType(),
                ValExpr::Field{
                    std::move(*$1),
                    std::move(*$3)
                }
            }
        ));
        delete $1; delete $3;
      }
    ;

/* Primaries: literals, vars, calls, struct literals, new allocations, actor construction */
primary_expr
    : TOK_LPAREN val_expr TOK_RPAREN { $$ = $2; }
    | TOK_LPAREN TOK_RPAREN {
        $$ = new shared_ptr<ValExpr>(make_shared<ValExpr>(
            ValExpr{ span_from(@1), FullType(), ValExpr::VUnit{} }
        ));
      }
    | TOK_INT_LIT {
        $$ = new shared_ptr<ValExpr>(make_shared<ValExpr>(
            ValExpr{ span_from(@1), FullType(), ValExpr::VInt{ $1 } }
        ));
      }
    | TOK_TRUE {
        $$ = new shared_ptr<ValExpr>(make_shared<ValExpr>(
            ValExpr{ span_from(@1), FullType(), ValExpr::VBool{ true } }
        ));
      }
    | TOK_FALSE {
        $$ = new shared_ptr<ValExpr>(make_shared<ValExpr>(
            ValExpr{ span_from(@1), FullType(), ValExpr::VBool{ false } }
        ));
      }
    | TOK_IDENT {
        $$ = new shared_ptr<ValExpr>(make_shared<ValExpr>(
            ValExpr{ span_from(@1), FullType(), ValExpr::VVar{ *$1 } }
        ));
        delete $1;
      }
    /* Struct instance: { ... } : TypeName */
    | TOK_LBRACE struct_instance TOK_RBRACE TOK_COLON TOK_IDENT {
        $2->type_name = std::move(*$5);
        $$ = new shared_ptr<ValExpr>(make_shared<ValExpr>(
            ValExpr{
                span_from(@$),
                FullType(),
                std::move(*$2)
            }
        ));
        delete $2; delete $5;
      }
    /* Allocation: new <cap>[size] <type>(default_value) */
    | TOK_NEW cap TOK_LSQUARE val_expr TOK_RSQUARE basic_type TOK_LPAREN val_expr TOK_RPAREN {
        $$ = new shared_ptr<ValExpr>(make_shared<ValExpr>(
            ValExpr{
                span_from(@$),
                FullType(),
                ValExpr::NewInstance{
                    std::move(*$6),  // BasicType
                    std::move(*$2),  // Cap
                    std::move(*$8),  // default_value
                    std::move(*$4)   // size
                }
            }
        ));
        delete $2; delete $4; delete $6; delete $8;
      }
    /* Actor construction: new Actor.ctor(args...) */
    | TOK_NEW TOK_IDENT TOK_DOT TOK_IDENT TOK_LPAREN val_expr_list TOK_RPAREN {
        $$ = new shared_ptr<ValExpr>(make_shared<ValExpr>(
            ValExpr{
                span_from(@$),
                FullType(),
                ValExpr::ActorConstruction{
                    std::move(*$2),  // actor_name
                    std::move(*$4),  // constructor_name
                    std::move(*$6)   // args
                }
            }
        ));
        delete $2; delete $4; delete $6;
      }
    /* Function call: f(args...) */
    | TOK_IDENT TOK_LPAREN val_expr_list TOK_RPAREN {
        $$ = new shared_ptr<ValExpr>(make_shared<ValExpr>(
            ValExpr{
                span_from(@$),
                FullType(),
                ValExpr::FuncCall{
                    std::move(*$1),
                    std::move(*$3)
                }
            }
        ));
        delete $1; delete $3;
      }
    /* Consume expression */
    | TOK_CONSUME TOK_LPAREN TOK_IDENT TOK_RPAREN {
        $$ = new shared_ptr<ValExpr>(make_shared<ValExpr>(
            ValExpr{
                span_from(@$),
                FullType(),
                ValExpr::Consume{ *$3 }
            }
        ));
        delete $3;
      }
    ;

/* ---------- TYPES ---------- */
basic_type
    : TOK_LPAREN TOK_RPAREN {
        $$ = new BasicType();
        $$->t = BasicType::TUnit{};
      }
    | TOK_INT {
        $$ = new BasicType();
        $$->t = BasicType::TInt{};
      }
    | TOK_BOOL {
        $$ = new BasicType();
        $$->t = BasicType::TBool{};
      }
    | TOK_IDENT {
        $$ = new BasicType();
        std::string &name = *$1;
        assert(name.size() > 0);
        if (std::isupper(name[0])) {
            // Actor type
            $$->t = BasicType::TActor{ std::move(*$1) };
        } else {
            // Regular named type
            $$->t = BasicType::TNamed{ std::move(*$1) };
        }

        delete $1;
      }
    ;

full_type
    : TOK_LPAREN full_type TOK_RPAREN {
        $$ = $2;
      }
    | basic_type {
        $$ = new FullType();
        $$->t = *$1;
        delete $1;
      }
    | basic_type cap {
        $$ = new FullType();
        $$->t = FullType::Pointer{ *$1, *$2 };
        delete $1; delete $2;
      }
    ;

cap
    : TOK_REF {
        $$ = new Cap();
        $$->t = Cap::Ref{};
      }
    | TOK_VAL {
        $$ = new Cap();
        $$->t = Cap::Val{};
      }
    | TOK_ISO {
        $$ = new Cap();
        $$->t = Cap::Iso{};
      }
    | TOK_LOCKED TOK_LESS TOK_IDENT TOK_GREATER {
        $$ = new Cap();
        $$->t = Cap::Locked{ *$3 };
        delete $3;
      }
    ;

%%
