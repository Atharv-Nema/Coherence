/* ---------- HEADER SECTION (shared across .cc and .hh) ---------- */
%code requires {
    // Includes needed by both the parser implementation (.cc)
    // and the generated header (.hh) that defines YYSTYPE.
    #include "top_level.hpp"
    #include <memory>
    #include <string>
    #include <vector>
    #include <unordered_set>
    #include <unordered_map>

    using namespace std;
}

/* ---------- IMPLEMENTATION-ONLY HEADER SECTION (.cc only) ---------- */
%code {
    // These only go into parser.tab.cc (not the header).
    #include <iostream>

    // Forward declaration of lexer (defined in lex.yy.cc)
    int yylex(void);

    // Global root node for the parsed program
    Program* program_root = nullptr;
}

/* ---------- BISON CONFIGURATION ---------- */
%define parse.error verbose
%locations

/* ---------- ERROR HANDLERS (go at bottom of parser.tab.cc) ---------- */
%code {
    void yyerror(YYLTYPE* loc, const char* s) {
        cerr << "Parse error at line "
             << loc->first_line << ", column " << loc->first_column
             << ": " << s << endl;
    }

    void yyerror(const char* s) {
        cerr << "Parse error: " << s << endl;
    }
}


/* ---------- TOKENS ---------- */
%token TOK_ACTOR TOK_NEW TOK_FUNC TOK_BE TOK_RETURN
%token TOK_ATOMIC TOK_IF TOK_ELSE TOK_WHILE TOK_DOT
%token TOK_TYPE TOK_STRUCT TOK_INITIALIZE
%token TOK_INT TOK_FLOAT TOK_BOOL
%token TOK_REF TOK_ISO TOK_VAL TOK_LOCKED
%token TOK_TRUE TOK_FALSE
%token TOK_ARROW TOK_ASSIGN
%token TOK_LEQ TOK_GEQ TOK_LESS TOK_GREATER
%token TOK_LPAREN TOK_RPAREN TOK_LBRACE TOK_RBRACE TOK_LSQUARE TOK_RSQUARE
%token TOK_COLON TOK_SEMI TOK_COMMA
%token TOK_PLUS TOK_MINUS TOK_STAR TOK_SLASH

%token <int_val> TOK_INT_LIT
%token <float_val> TOK_FLOAT_LIT
%token <str_val> TOK_IDENT

/* ---------- UNION (Semantic Values) ---------- */
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

/* ---------- TYPE DECLARATIONS ---------- */
%type <program> program
%type <top_level_list> top_level_items
%type <top_item> top_level_item
%type <stmt> stmt
%type <stmt_list> stmt_list block
%type <val_expr> val_expr
%type <val_expr_list> val_expr_list
%type <basic_type> basic_type
%type <full_type> full_type
%type <cap> cap
%type <struct_fields> struct_fields
%type <struct_instance> struct_instance
%type <var_list> func_params 
%type <actor_fields> actor_fields
%type <actor> actor_def actor_members
%type <actor_constructor> actor_constructor
%type <actor_behaviour> actor_behaviour
%type <func> func_def
%type <type_def> type_def

/* ---------- OPERATOR PRECEDENCE ---------- */
%left TOK_PLUS TOK_MINUS
%left TOK_STAR TOK_SLASH
%left TOK_LEQ TOK_GEQ TOK_LESS TOK_GREATER

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
        delete $1; }
    | func_def  { 
        $$ = new TopLevelItem(); 
        $$->t = std::move(*$1); 
        $$->source_span = span_from(@1); 
        delete $1; }
    | type_def  { 
        $$ = new TopLevelItem(); 
        $$->t = std::move(*$1); 
        $$->source_span = span_from(@1); 
        delete $1; }
    ;

/* ---------- TYPE DEFINITIONS ---------- */
type_def
    : TOK_TYPE TOK_IDENT TOK_ASSIGN TOK_STRUCT TOK_LBRACE struct_fields TOK_RBRACE TOK_SEMI {
        $$ = new TopLevelItem::TypeDef(
            std::move(*$2),
            make_shared<NameableType>(NameableType::Struct{ std::move($6->members) })
        );
        delete $2; delete $6;
      }
    | TOK_TYPE TOK_IDENT TOK_ASSIGN basic_type TOK_SEMI {
        $$ = new TopLevelItem::TypeDef(
            std::move(*$2),
            make_shared<NameableType>(NameableType::Basic{ std::move(*$4) })
        );
        delete $2; delete $4;
      }
    ;

struct_fields
    : /* empty */ { $$ = new NameableType::Struct(); }
    | struct_fields TOK_IDENT TOK_COLON basic_type TOK_SEMI {
        $$ = $1;
        $$->members.emplace_back(std::move(*$2), std::move(*$4));
        delete $2; delete $4;
      }
    ;

struct_instance
    : /* empty */ { $$ = new ValExpr::VStruct(); }
    | struct_instance TOK_IDENT TOK_ASSIGN val_expr TOK_SEMI {
        $$ = $1;
        $$->fields.emplace_back(std::move(*$2), std::move(*$4));
        delete $2; delete $4;
      }

/* ---------- ACTOR DEFINITION ---------- */
actor_def
    : TOK_ACTOR TOK_IDENT TOK_LBRACE actor_fields actor_members TOK_RBRACE {
        $$ = $5;
        (*$$)->name = std::move(*$2);
        (*$$)->member_vars = std::move(*$4);
        delete $2; delete $4;
      }
    ;

/* ---------- ACTOR FIELDS ---------- */
actor_fields
    : /* empty */ { $$ = new unordered_map<std::string, FullType>(); }
    | actor_fields TOK_IDENT TOK_COLON full_type TOK_SEMI {
        $$ = $1;
        $$->emplace(std::move(*$2), std::move(*$4));
        delete $2; delete $4;
      }
    ;

/* ---------- ACTOR MEMBERS ---------- */
actor_members
    : /* empty */ { 
        $$ = new shared_ptr<TopLevelItem::Actor>(make_shared<TopLevelItem::Actor>());
        }
    | actor_members actor_constructor { $$ = $1; (*$$)->constructors.emplace_back(std::move(*$2)); delete $2; }
    | actor_members func_def          { $$ = $1; (*$$)->member_funcs.emplace_back(std::move(*$2)); delete $2; }
    | actor_members actor_behaviour   { $$ = $1; (*$$)->member_behaviours.emplace_back(std::move(*$2)); delete $2; }
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
        (*$$)->locks_dereferenced = std::unordered_set<std::string>();
        delete $2; delete $4; delete $7; delete $8;
      }
    ;

func_params
    : /* empty */ { $$ = new vector<TopLevelItem::VarDecl>(); }
    | func_params full_type TOK_IDENT opt_comma {
        $$ = $1;
        $$->push_back({ std::move(*$3), std::move(*$2) });
        delete $2; delete $3;
      }
    ;

opt_comma : /* empty */ | TOK_COMMA ;

/* ---------- STATEMENTS ---------- */
block
    : TOK_LBRACE stmt_list TOK_RBRACE { $$ = $2; }
    ;

stmt_list
    : /* empty */ { $$ = new vector<shared_ptr<Stmt>>(); }
    | stmt_list stmt  { $$ = $1; $$->push_back(*$2); delete $2; }
    ;

stmt
    : full_type TOK_IDENT TOK_ASSIGN val_expr TOK_SEMI {
        $$ = new shared_ptr<Stmt>(make_shared<Stmt>(
            Stmt {
                span_from(@$),
                Stmt::VarDeclWithInit{
                    std::move(*$2),
                    std::move(*$1),
                    std::move(*$4)
                }
            }
        ));
        delete $1; delete $2; delete $4;
      }
    | TOK_IDENT TOK_INITIALIZE val_expr TOK_SEMI {
        $$ = new shared_ptr<Stmt>(make_shared<Stmt>(
            Stmt {
                span_from(@$),
                Stmt::MemberInitialize {
                    std::move(*$1),
                    std::move(*$3)
                }
            }
        ));
        delete $1; delete $3;
      }
    | val_expr TOK_SEMI {
        $$ = new shared_ptr<Stmt>(make_shared<Stmt>(
            Stmt {
                span_from(@$),
                Stmt::Expr{ std::move(*$1) }
            }
        ));
        delete $1;
      }

    | TOK_RETURN val_expr TOK_SEMI {
        $$ = new shared_ptr<Stmt>(make_shared<Stmt>(
            Stmt {
                span_from(@$),
                Stmt::Return{ std::move(*$2) }
            }
        ));
        delete $2;
      }

    | TOK_ATOMIC block {
        $$ = new shared_ptr<Stmt>(make_shared<Stmt>(
            Stmt {
                span_from(@$),
                make_shared<Stmt::Atomic>(std::unordered_set<std::string>(), std::move(*$2))
            }
        ));
        delete $2;
      }

    | TOK_IF TOK_LPAREN val_expr TOK_RPAREN block {
        $$ = new shared_ptr<Stmt>(make_shared<Stmt>(
            Stmt {
                span_from(@$),
                Stmt::If{
                    std::move(*$3),
                    std::move(*$5),
                    std::nullopt
                },
            }
        ));
        delete $3; delete $5;
      }
    | TOK_IF TOK_LPAREN val_expr TOK_RPAREN block TOK_ELSE block {
        $$ = new shared_ptr<Stmt>(make_shared<Stmt>(
            Stmt {
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

    | TOK_WHILE TOK_LPAREN val_expr TOK_RPAREN block {
        $$ = new shared_ptr<Stmt>(make_shared<Stmt>(
            Stmt {
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


// List of val_exprs (used for parameters sent to function calls)
val_expr_list
    : /* empty */ { $$ = new vector<shared_ptr<ValExpr>>(); }
    | val_expr_list val_expr  { $$ = $1; $$->push_back(*$2); delete $2; }
    ;



// Need to add FuncCall, BeCall, Assignment, FieldAccess, PointerAccess, ActorConstruction, Unit
val_expr
    : TOK_LPAREN TOK_RPAREN {
        $$ = new shared_ptr<ValExpr>(make_shared<ValExpr>(
            ValExpr { span_from(@1), ValExpr::VUnit{} }
        ));
      }
    
    | TOK_INT_LIT {
        $$ = new shared_ptr<ValExpr>(make_shared<ValExpr>(
            ValExpr { span_from(@1), ValExpr::VInt{ $1 } }
        ));
      }
    | TOK_FLOAT_LIT {
        $$ = new shared_ptr<ValExpr>(make_shared<ValExpr>(
            ValExpr { span_from(@1), ValExpr::VFloat{ $1 } }
        ));
      }
    // Booleans
    | TOK_TRUE {
        $$ = new shared_ptr<ValExpr>(make_shared<ValExpr>(
             ValExpr { span_from(@1), ValExpr::VBool{ true } }
        ));
      }
    | TOK_FALSE {
        $$ = new shared_ptr<ValExpr>(make_shared<ValExpr>(
             ValExpr { span_from(@1), ValExpr::VBool{ false } }
        ));
      }
    // Variables
    | TOK_IDENT {
        $$ = new shared_ptr<ValExpr>(make_shared<ValExpr>(
              ValExpr { span_from(@1), ValExpr::VVar{ *$1 } }
        ));
        delete $1;
      }

    // Struct instance
    | TOK_LBRACE struct_instance TOK_RBRACE TOK_COLON TOK_IDENT {
        $2->type_name = std::move(*$5);
        $$ = new shared_ptr<ValExpr>(make_shared<ValExpr>(
            ValExpr {
                span_from(@$),
                std::move(*$2)
            }
        ));
        delete $2; delete $5;
      }
    
    // Allocations
    | TOK_NEW cap TOK_LSQUARE val_expr TOK_RSQUARE basic_type TOK_LPAREN val_expr TOK_RPAREN {
        $$ = new shared_ptr<ValExpr>(make_shared<ValExpr>(
            ValExpr {
                span_from(@$),
                ValExpr::NewInstance{ std::move(*$6), std::move(*$2), std::move(*$8), std::move(*$4) }
            }
        ));
        delete $2; delete $4; delete $6; delete $8;
      }
    
    | TOK_NEW TOK_IDENT TOK_DOT TOK_IDENT TOK_LPAREN val_expr_list TOK_RPAREN {
        $$ = new shared_ptr<ValExpr>(make_shared<ValExpr>(
            ValExpr {
                span_from(@$),
                ValExpr::ActorConstruction{
                    std::move(*$2), std::move(*$4), std::move(*$6)
                }
            }
        ));
        delete $2; delete $4; delete $6;
      }
    
    // Accesses
    | val_expr TOK_LSQUARE val_expr TOK_RSQUARE {
        $$ = new shared_ptr<ValExpr>(make_shared<ValExpr>(
            ValExpr {
                span_from(@$),
                ValExpr::PointerAccess{
                    std::move(*$3), std::move(*$1)
                }
            }
        ));
        delete $1; delete $3;
      }

    | val_expr TOK_IDENT {
        $$ = new shared_ptr<ValExpr>(make_shared<ValExpr>(
            ValExpr {
                span_from(@$),
                ValExpr::Field{
                    std::move(*$1), std::move(*$2)
                }
            }
        ));
        delete $1; delete $2;
      }
    
    | val_expr TOK_ASSIGN val_expr {
        $$ = new shared_ptr<ValExpr>(make_shared<ValExpr>(
            ValExpr {
                span_from(@$),
                ValExpr::Assignment{ std::move(*$1), std::move(*$3) }
            }
        ));
        delete $1; delete $3;  
      }


    // Callable stuff
    | TOK_IDENT TOK_LPAREN val_expr_list TOK_RPAREN {
        $$ = new shared_ptr<ValExpr>(make_shared<ValExpr>(
            ValExpr {
                span_from(@$),
                ValExpr::FuncCall{ std::move(*$1), std::move(*$3) }
            }
        ));
        delete $1; delete $3;
      }
    
    | val_expr TOK_DOT TOK_IDENT TOK_LPAREN val_expr_list TOK_RPAREN {
        $$ = new shared_ptr<ValExpr>(make_shared<ValExpr>(
            ValExpr {
                span_from(@$),
                ValExpr::BeCall{ *$1, *$3, std::move(*$5) }
            }
        ));
        delete $1; delete $3; delete $5;
  }



    // Binary operations
    | val_expr TOK_PLUS val_expr {
        $$ = new shared_ptr<ValExpr>(make_shared<ValExpr>(
             ValExpr {  span_from(@$), ValExpr::BinOpExpr{ *$1, BinOp::Add, *$3 } }
        ));
        delete $1; delete $3;
      }
    | val_expr TOK_MINUS val_expr {
        $$ = new shared_ptr<ValExpr>(make_shared<ValExpr>(
             ValExpr { span_from(@$), ValExpr::BinOpExpr{ *$1, BinOp::Sub, *$3 } }
        ));
        delete $1; delete $3;
      }
    | val_expr TOK_STAR val_expr {
        $$ = new shared_ptr<ValExpr>(make_shared<ValExpr>(
            ValExpr { span_from(@$), ValExpr::BinOpExpr{ *$1, BinOp::Mul, *$3 } }
        ));
        delete $1; delete $3;
      }
    | val_expr TOK_SLASH val_expr {
        $$ = new shared_ptr<ValExpr>(make_shared<ValExpr>(
            ValExpr { span_from(@$), ValExpr::BinOpExpr{ *$1, BinOp::Div, *$3 } }
        ));
        delete $1; delete $3;
      }
    ;

/* ---------- TYPES ---------- */
basic_type
    : TOK_INT   { $$ = new BasicType(); $$->t = BasicType::TInt{}; }
    | TOK_FLOAT { $$ = new BasicType(); $$->t = BasicType::TFloat{}; }
    | TOK_BOOL  { $$ = new BasicType(); $$->t = BasicType::TBool{}; }
    | TOK_IDENT { $$ = new BasicType(); $$->t = BasicType::TNamed{ *$1 }; delete $1; }
    ;

full_type
    : basic_type { $$ = new FullType(); $$->t = *$1; delete $1; }
    | TOK_LPAREN basic_type cap TOK_RPAREN {
        $$ = new FullType();
        $$->t = FullType::Pointer{ *$2, *$3 };
        delete $2; delete $3;
      }
    ;

cap
    : TOK_REF              { $$ = new Cap(); $$->t = Cap::Ref{}; }
    | TOK_VAL              { $$ = new Cap(); $$->t = Cap::Val{}; }
    | TOK_ISO              { $$ = new Cap(); $$->t = Cap::Iso{}; }
    | TOK_LOCKED TOK_IDENT { $$ = new Cap(); $$->t = Cap::Locked{ *$2 }; delete $2; }
    ;

%%
