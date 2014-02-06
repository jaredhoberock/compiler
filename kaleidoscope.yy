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
}

%param { driver &context }

%locations
%initial-action
{
  // initialize the initial location
  @$.begin.filename = @$.end.filename = &context.filename;
}

%define parse.trace
%define parse.error verbose

%code
{
#include "driver.hpp"
}

%token
  END 0      "end of file"
  LPAREN     "("
  RPAREN     ")"
  PLUS       "+"
  MINUS      "-"
  MULTIPLIES "*"
  DIVIDES    "/"
;
%token <std::string> IDENTIFIER "identifier"
%token <double>      NUMBER     "number"

%type <double> numberexpr
%type <std::string> identifierexpr

%left PLUS MINUS
%left MULTIPLIES DIVIDES
%precedence UNARY_PLUS UNARY_MINUS

%printer { yyoutput << $$; } <*>;

%%
%start primary;

primary:
  identifierexpr {std::cout << "parser: found primary. " << std::endl;}
| numberexpr     {std::cout << "parser: found primary. " << std::endl;}
| parenexpr      {std::cout << "parser: found primary. " << std::endl;}

numberexpr: "number" { $$ = $1; std::cout << "parser: found number: " << $1 << std::endl;}

identifierexpr: "identifier" { $$ = $1; std::cout << "parser: found identifier: " << $1 << std::endl;}

expr:
  identifierexpr {std::cout << "parser: found expression." << std::endl;}
| numberexpr     {std::cout << "parser: found expression." << std::endl;}
| parenexpr      {std::cout << "parser: found expression." << std::endl;}
| arithexpr      {std::cout << "parser: found expression." << std::endl;}

parenexpr: "(" expr ")" {std::cout << "found parenexpr" << std::endl;}

arithexpr:
  expr "+" expr {std::cout << "found arithexpr" << std::endl;}
| expr "-" expr {std::cout << "found arithexpr" << std::endl;}
| expr "*" expr {std::cout << "found arithexpr" << std::endl;}
| expr "/" expr {std::cout << "found arithexpr" << std::endl;}
| "+" expr %prec UNARY_PLUS  {std::cout << "found arithexpr" << std::endl;}
| "-" expr %prec UNARY_MINUS {std::cout << "found arithexpr" << std::endl;}
%%

void yy::parser::error(const location_type &l, const std::string &m)
{
  context.error(l, m);
}

