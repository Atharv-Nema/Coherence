%option noyywrap
%{
#include "parser.tab.hpp"
#include <string>
using namespace std;
#define YY_DECL int yylex()
%}

%%

"actor"     return TOK_ACTOR;
"new"       return TOK_NEW;
"func"      return TOK_FUNC;
"be"        return TOK_BE;
"return"    return TOK_RETURN;
"type"      return TOK_TYPE;
"struct"    return TOK_STRUCT;
"atomic"    return TOK_ATOMIC;
"if"        return TOK_IF;
"else"      return TOK_ELSE;
"while"     return TOK_WHILE;

"int"       return TOK_INT;
"float"     return TOK_FLOAT;
"bool"      return TOK_BOOL;

"ref"       return TOK_REF;
"iso"       return TOK_ISO;
"val"       return TOK_VAL;
"locked"    return TOK_LOCKED;

"true"      return TOK_TRUE;
"false"     return TOK_FALSE;

":="        return TOK_INITIALIZE;
"=>"        return TOK_ARROW;
"="         return TOK_ASSIGN;
"<="        return TOK_LEQ;
">="        return TOK_GEQ;
"<"         return TOK_LESS;
">"         return TOK_GREATER;
":"         return TOK_COLON;
"("         return TOK_LPAREN;
")"         return TOK_RPAREN;
"["         return TOK_LSQUARE;
"]"         return TOK_RSQUARE;
"{"         return TOK_LBRACE;
"}"         return TOK_RBRACE;
";"         return TOK_SEMI;
","         return TOK_COMMA;
"+"         return TOK_PLUS;
"-"         return TOK_MINUS;
"*"         return TOK_STAR;
"/"         return TOK_SLASH;

"//".*                                 { /* ignore line comments */ }
"/*"([^*]|\n|\*+[^*/])*\*+"/"         { /* ignore block comments */ }

"-"?[0-9]+"."[0-9]+       { yylval.float_val = stod(yytext); return TOK_FLOAT_LIT; }
"-"?[0-9]+                { yylval.int_val = stoi(yytext); return TOK_INT_LIT; }

[a-zA-Z_][a-zA-Z0-9_']*   { yylval.str_val = new string(yytext); return TOK_IDENT; }

[ \t\r\n]+                { /* ignore */ }

.                         { /* ignore */ }

%%
