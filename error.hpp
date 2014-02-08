#pragma once

#include <iostream>
#include <llvm/IR/Module.h>

class expr_ast;
class prototype_ast;
class function_ast;

inline expr_ast *error(const char *str)
{
  std::cerr << "Error: " << str << std::endl;
  return 0;
}

inline prototype_ast *error_prototype(const char *str)
{
  error(str);
  return 0;
}

inline function_ast *error_function(const char *str)
{
  error(str);
  return 0;
}

static llvm::Value *error_value(const char *str)
{
  std::cerr << "Error: " << str << std::endl;
  return 0;
}

