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

%token END 0 "end of file"
%token <std::string> IDENTIFIER "identifier"
%token <double>      NUMBER     "number"

%printer { yyoutput << $$; } <*>;

%%
%start primary;

primary:
  "identifier" {std::cout << "parser: found identifier: " << $1 << std::endl;}
| "number"     {std::cout << "parser: found number: " << $1 << std::endl;}
%%

void yy::parser::error(const location_type &l, const std::string &m)
{
  context.error(l, m);
}

