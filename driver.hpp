#pragma once

#include <string>

struct driver
{
  std::string filename;

  void scan_begin();
  void scan_end();

  inline void error(const yy::location &l, const std::string &m)
  {
    std::cerr << l << ": " << m << std::endl;
  }

  inline void error(const std::string &m)
  {
    std::cerr << m << std::endl;
  }
};

#define YY_DECL yy::parser::symbol_type yylex(driver &d)
YY_DECL;

