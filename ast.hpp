#pragma once

#include <string>
#include <vector>
#include "driver.hpp"

// base class for all expression nodes
class expr_ast
{
  public:
    inline virtual ~expr_ast(){};

    virtual llvm::Value *codegen(driver &context) = 0;
};

class number_expr_ast : public expr_ast
{
  public:
    inline number_expr_ast(double val) : m_val(val) {}

    virtual llvm::Value *codegen(driver &context);

  private:
    double m_val;
};

class variable_expr_ast : public expr_ast
{
  public:
    variable_expr_ast(const std::string &name) : m_name(name) {}

    virtual llvm::Value *codegen(driver &context);

  private:
    std::string m_name;
};

class binary_expr_ast : public expr_ast
{
  public:
    binary_expr_ast(char op, expr_ast *lhs, expr_ast *rhs)
      : m_op(op), m_lhs(lhs), m_rhs(rhs)
    {}

    virtual llvm::Value *codegen(driver &context);

  private:
    char m_op;
    expr_ast *m_lhs, *m_rhs;
};

class call_expr_ast : public expr_ast
{
  public:
    call_expr_ast(const std::string &callee, const std::vector<expr_ast*> &args)
      : m_callee(callee), m_args(args)
    {}

    virtual llvm::Value *codegen(driver &context);

  private:
    std::string m_callee;
    std::vector<expr_ast*> m_args;
};

class prototype_ast
{
  public:
    prototype_ast(const std::string &name, const std::vector<std::string> &args)
      : m_name(name), m_args(args)
    {}

    virtual llvm::Function *codegen(driver &context);

  private:
    std::string m_name;
    std::vector<std::string> m_args;
};

class function_ast
{
  public:
    function_ast(prototype_ast *proto, expr_ast *body)
      : m_proto(proto), m_body(body)
    {}

    virtual llvm::Function *codegen(driver &context);

  private:
    prototype_ast *m_proto;
    expr_ast *m_body;
};

class if_expr_ast : public expr_ast
{
  public:
    if_expr_ast(expr_ast *cond, expr_ast *then_expr, expr_ast *else_expr)
      : m_cond(cond),
        m_then_expr(then_expr),
        m_else_expr(else_expr)
    {}

    virtual llvm::Value *codegen(driver &context);

  private:
    expr_ast *m_cond, *m_then_expr, *m_else_expr;
};

class for_expr_ast : public expr_ast
{
  public:
    for_expr_ast(const std::string &variable_name,
                 expr_ast *start,
                 expr_ast *end,
                 expr_ast *step,
                 expr_ast *body)
      : m_variable_name(variable_name),
        m_start(start),
        m_end(end),
        m_step(step),
        m_body(body)
    {}

    virtual llvm::Value *codegen(driver &context);

  private:
    std::string m_variable_name;
    expr_ast *m_start, *m_end, *m_step, *m_body;
};

