#include "ast.hpp"
#include "error.hpp"
#include <llvm/Analysis/Verifier.h>
#include <iostream>
#include <map>
#include <vector>

llvm::Value *number_expr_ast::codegen(driver &context)
{
  return llvm::ConstantFP::get(llvm::getGlobalContext(), llvm::APFloat(m_val));
}

llvm::Value *variable_expr_ast::codegen(driver &context)
{
  // look this variable up in the function
  llvm::Value *result = context.named_values()[m_name];
  return result ? result : error_value("Unknown variable name");
}

llvm::Value *binary_expr_ast::codegen(driver &context)
{
  auto *lhs = m_lhs->codegen(context);
  auto *rhs = m_rhs->codegen(context);
  if(lhs == 0 || rhs == 0) return 0;

  switch(m_op)
  {
    case '+' : return context.builder().CreateFAdd(lhs, rhs, "addtmp");
    case '-' : return context.builder().CreateFSub(lhs, rhs, "subtmp");
    case '*' : return context.builder().CreateFMul(lhs, rhs, "multmp");
    case '<' :
      lhs = context.builder().CreateFCmpULT(lhs, rhs, "cmptmp");

      // convert bool 0/1 to double 0.0 or 1.0
      return context.builder().CreateUIToFP(lhs, llvm::Type::getDoubleTy(llvm::getGlobalContext()), "booltmp");
    default: return error_value("invalid binary operator");
  }
}

llvm::Value *call_expr_ast::codegen(driver &context)
{
  // look up the name in the global module table
  llvm::Function *callee_f = context.module().getFunction(m_callee);

  if(callee_f == 0)
  {
    return error_value("Unknown function referenced");
  }

  // if argument mismatch error
  if(callee_f->arg_size() != m_args.size())
  {
    return error_value("Incorrect # of arguments passed");
  }

  std::vector<llvm::Value*> args_vector;
  for(auto a : m_args)
  {
    args_vector.push_back(a->codegen(context));
    if(args_vector.back() == 0) return 0;
  }

  return context.builder().CreateCall(callee_f, args_vector, "calltmp");
}

llvm::Function *prototype_ast::codegen(driver &context)
{
  // make the function type: double(double, double) etc.
  std::vector<llvm::Type*> doubles(m_args.size(), llvm::Type::getDoubleTy(llvm::getGlobalContext()));

  llvm::FunctionType *fun_type = llvm::FunctionType::get(llvm::Type::getDoubleTy(llvm::getGlobalContext()), doubles, false);

  llvm::Function *f = llvm::Function::Create(fun_type, llvm::Function::ExternalLinkage, m_name, &context.module());

  // if f conflicted, there was already something named m_name. if it has a
  // body, don't allow redefinition or reextern
  if(f->getName() != m_name)
  {
    // delete the one we just made and get the existing one
    f->eraseFromParent();
    f = context.module().getFunction(m_name);

    // if f already has a body, reject this
    if(!f->empty())
    {
      error_function("redefinition of function");
      return 0;
    }

    // if f took a different number of args, reject
    if(f->arg_size() != m_args.size())
    {
      error_function("redefinition of function with different # of args");
      return 0;
    }
  }

  unsigned int index = 0;
  for(llvm::Function::arg_iterator arg_iter = f->arg_begin();
      index != m_args.size();
      ++arg_iter, ++index)
  {
    arg_iter->setName(m_args[index]);

    // add arguments to variable symbol table
    context.named_values()[m_args[index]] = arg_iter;
  }

  return f;
}

llvm::Function *function_ast::codegen(driver &context)
{
  context.named_values().clear();

  llvm::Function *f = m_proto->codegen(context);
  if(f == 0) return 0;

  // create a new basic block to start insertion into
  llvm::BasicBlock *bb = llvm::BasicBlock::Create(llvm::getGlobalContext(), "entry", f);
  context.builder().SetInsertPoint(bb);

  if(llvm::Value *return_value = m_body->codegen(context))
  {
    // finish off the function
    context.builder().CreateRet(return_value);

    // validate the generated code, checking for consistency
    llvm::verifyFunction(*f);

    // optimize the function
    context.fpm().run(*f);

    return f;
  }

  // error reading body, remove function
  f->eraseFromParent();
  return 0;
}

llvm::Value *if_expr_ast::codegen(driver &context)
{
  llvm::Value *cond_value = m_cond->codegen(context);
  if(cond_value == 0) return 0;

  // convert condition to a bool by comparing equal to 0
  cond_value =
    context.builder().CreateFCmpONE(cond_value,
                                    llvm::ConstantFP::get(llvm::getGlobalContext(), llvm::APFloat(0.0)),
                                    "ifcond");

  llvm::Function *fun = context.builder().GetInsertBlock()->getParent();

  // create blocks for the then and else cases. insert the "then" block at the
  // end of the function
  llvm::BasicBlock *then_bb  = llvm::BasicBlock::Create(llvm::getGlobalContext(), "then", fun);
  llvm::BasicBlock *else_bb  = llvm::BasicBlock::Create(llvm::getGlobalContext(), "else", fun);
  llvm::BasicBlock *merge_bb = llvm::BasicBlock::Create(llvm::getGlobalContext(), "ifcont", fun);

  context.builder().CreateCondBr(cond_value, then_bb, else_bb);

  // emit then value
  context.builder().SetInsertPoint(then_bb);

  llvm::Value *then_value = m_then_expr->codegen(context);
  if(then_value == 0) return 0;

  context.builder().CreateBr(merge_bb);

  // codegen of the then can change the current block, update then_bb for the phi
  then_bb = context.builder().GetInsertBlock();

  // emit else block
  fun->getBasicBlockList().push_back(else_bb);
  context.builder().SetInsertPoint(else_bb);

  llvm::Value *else_value = m_else_expr->codegen(context);
  if(else_value == 0) return 0;

  context.builder().CreateBr(merge_bb);

  // codegen of the else can change the current block, update else_bb for the phi
  else_bb = context.builder().GetInsertBlock();

  // emit merge block
  fun->getBasicBlockList().push_back(merge_bb);
  context.builder().SetInsertPoint(merge_bb);
  llvm::PHINode *phi_node = context.builder().CreatePHI(llvm::Type::getDoubleTy(llvm::getGlobalContext()), 2, "iftmp");

  phi_node->addIncoming(then_value, then_bb);
  phi_node->addIncoming(else_value, else_bb);

  return phi_node;
}

llvm::Value *for_expr_ast::codegen(driver &context)
{
  // emit the start code first, without "variable" in scope
  llvm::Value *start_value = m_start->codegen(context);
  if(start_value == 0) return 0;
  
  // make the new basic block for the loop header, inserting after current block
  llvm::Function *f = context.builder().GetInsertBlock()->getParent();
  llvm::BasicBlock *preheader_bb = context.builder().GetInsertBlock();
  llvm::BasicBlock *loop_bb = llvm::BasicBlock::Create(llvm::getGlobalContext(), "loop", f);

  // insert an explicit fall through from the current block to the loop_bb
  context.builder().CreateBr(loop_bb);

  // start insertion in loop_bb
  context.builder().SetInsertPoint(loop_bb);

  // start with the PHI node with an entry for start
  llvm::PHINode *variable = context.builder().CreatePHI(llvm::Type::getDoubleTy(llvm::getGlobalContext()), 2, m_variable_name.c_str());
  variable->addIncoming(start_value, preheader_bb);

  // within the loop, the variableis defined equal to the PHI node. if it
  // shadows an existing variable, we have to restore it, so save it now.
  llvm::Value *old_value = context.named_values()[m_variable_name];
  context.named_values()[m_variable_name] = variable;

  // emit the body of the loop. this, like any other expr, can change the
  // current basic block. note that we ignore the value computed by the body,
  // but don't allow an error
  if(m_body->codegen(context) == 0)
  {
    return 0;
  }

  // emit the step value.
  llvm::Value *step_value = 0;
  if(m_step)
  {
    step_value = m_step->codegen(context);
    if(step_value == 0) return 0;
  }
  else
  {
    // if not specified, use 1.0
    step_value = llvm::ConstantFP::get(llvm::getGlobalContext(), llvm::APFloat(1.0));
  }

  llvm::Value *next_variable = context.builder().CreateFAdd(variable, step_value, "nextvar");

  // compute the end condition
  llvm::Value *end_condition = m_end->codegen(context);
  if(end_condition == 0) return 0;

  // convert condition to a bool by comparing equal to 0.0
  end_condition =
    context.builder().CreateFCmpONE(end_condition,
                          llvm::ConstantFP::get(llvm::getGlobalContext(), llvm::APFloat(0.0)),
                          "loopcond");

  // create the "after loop" block and insert it
  llvm::BasicBlock *loop_end_bb = context.builder().GetInsertBlock();
  llvm::BasicBlock *after_bb = llvm::BasicBlock::Create(llvm::getGlobalContext(), "afterloop", f);

  // insert the conditional branch into the end of loop_end_bb
  context.builder().CreateCondBr(end_condition, loop_bb, after_bb);

  // any new code will be inserted in after_bb
  context.builder().SetInsertPoint(after_bb);

  // add a new entry to the PHI node for the backedge
  variable->addIncoming(next_variable, loop_end_bb);

  // restore the unshadowed variable
  if(old_value)
  {
    context.named_values()[m_variable_name] = old_value;
  }
  else
  {
    context.named_values().erase(m_variable_name);
  }

  // for expr always returns 0.0
  return llvm::Constant::getNullValue(llvm::Type::getDoubleTy(llvm::getGlobalContext()));
}

extern "C" double putchard(double x)
{
  std::putchar(static_cast<char>(x));
  return 0;
}

