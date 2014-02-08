// from http://www.gnu.org/software/bison/manual/html_node/Calc_002b_002b-Parser.html#Calc_002b_002b-Parser
%skeleton "lalr1.cc"
%require "3.0.2"
%defines
%define parser_class_name {parser}

%define api.token.constructor
%define api.value.type variant
%define parse.assert

%code requires
{
#include <string>
class driver;
#include "ast.hpp"
}

%param { driver &context }

%locations
%initial-action
{
  // initialize the initial location
  @$.begin.filename = @$.end.filename = &context.filename();
}

%define parse.trace
%define parse.error verbose

%code
{
#include "driver.hpp"
#include "yylex_decl.hpp"
}

%token
  END 0      "end of file"
  LPAREN     "("
  RPAREN     ")"
  PLUS       "+"
  MINUS      "-"
  MULTIPLIES "*"
  DIVIDES    "/"
  SEMICOLON  ";"
  DEF        "def"
  EXTERN     "extern"
;
%token <std::string> IDENTIFIER "identifier"
%token <double>      NUMBER     "number"

%type <number_expr_ast*>         numberexpr
%type <variable_expr_ast*>       identifierexpr
%type <expr_ast*>                parenexpr
%type <expr_ast*>                expr
%type <binary_expr_ast*>         arithexpr
%type <std::vector<std::string>> parameter_list
%type <prototype_ast*>           prototype
%type <function_ast*>            extern
%type <function_ast*>            definition

%left PLUS MINUS
%left MULTIPLIES DIVIDES
%precedence UNARY_PLUS UNARY_MINUS

//%printer { yyoutput << $$; } <*>;

%%
%start program;

program:
  statement program {}
| {}

statement:
  expr ";"       { $1->codegen(context); }
| definition ";" { $1->codegen(context); }
| extern ";"     { $1->codegen(context); }

numberexpr: "number" { $$ = new number_expr_ast($1); }

identifierexpr: "identifier" { $$ = new variable_expr_ast($1); }

expr:
  identifierexpr { $$ = $1; }
| numberexpr     { $$ = $1; }
| parenexpr      { $$ = $1; }
| arithexpr      { $$ = $1; }

parenexpr: "(" expr ")" { $$ = $2; }

arithexpr:
  expr "+" expr { $$ = new binary_expr_ast('+', $1, $3); }
| expr "-" expr { $$ = new binary_expr_ast('-', $1, $3); }
| expr "*" expr { $$ = new binary_expr_ast('*', $1, $3); }
| expr "/" expr { $$ = new binary_expr_ast('/', $1, $3); }
| "+" expr %prec UNARY_PLUS  {std::cout << "found arithexpr" << std::endl;}
| "-" expr %prec UNARY_MINUS {std::cout << "found arithexpr" << std::endl;}

definition: "def" prototype expr { $$ = new function_ast($2, $3); }

extern: "extern" prototype { $$ = new function_ast($2, 0); }

prototype: "identifier" "(" parameter_list ")" { $$ = new prototype_ast($1, $3); }

parameter_list:
  "identifier" parameter_list { $2.push_back($1); $$ = $2; }
|                             { $$ = std::vector<std::string>(); }
%%

void yy::parser::error(const location_type &l, const std::string &m)
{
  context.error(l, m);
}

