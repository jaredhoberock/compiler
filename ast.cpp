#include "ast.hpp"
#include "error.hpp"
#include <llvm/Analysis/Passes.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/JIT.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/PassManager.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Transforms/Scalar.h>
#include <iostream>
#include <map>
#include <vector>

llvm::Module *the_module;
llvm::FunctionPassManager *the_fpm;
llvm::ExecutionEngine *the_execution_engine;
static llvm::IRBuilder<> builder(llvm::getGlobalContext());
static std::map<std::string, llvm::Value*> named_values;

llvm::Value *number_expr_ast::codegen()
{
  return llvm::ConstantFP::get(llvm::getGlobalContext(), llvm::APFloat(m_val));
}

llvm::Value *variable_expr_ast::codegen()
{
  // look this variable up in the function
  llvm::Value *result = named_values[m_name];
  return result ? result : error_value("Unknown variable name");
}

llvm::Value *binary_expr_ast::codegen()
{
  auto *lhs = m_lhs->codegen();
  auto *rhs = m_rhs->codegen();
  if(lhs == 0 || rhs == 0) return 0;

  switch(m_op)
  {
    case '+' : return builder.CreateFAdd(lhs, rhs, "addtmp");
    case '-' : return builder.CreateFSub(lhs, rhs, "subtmp");
    case '*' : return builder.CreateFMul(lhs, rhs, "multmp");
    case '<' :
      lhs = builder.CreateFCmpULT(lhs, rhs, "cmptmp");

      // convert bool 0/1 to double 0.0 or 1.0
      return builder.CreateUIToFP(lhs, llvm::Type::getDoubleTy(llvm::getGlobalContext()), "booltmp");
    default: return error_value("invalid binary operator");
  }
}

llvm::Value *call_expr_ast::codegen()
{
  // look up the name in the global module table
  llvm::Function *callee_f = the_module->getFunction(m_callee);

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
    args_vector.push_back(a->codegen());
    if(args_vector.back() == 0) return 0;
  }

  return builder.CreateCall(callee_f, args_vector, "calltmp");
}

llvm::Function *prototype_ast::codegen()
{
  // make the function type: double(double, double) etc.
  std::vector<llvm::Type*> doubles(m_args.size(), llvm::Type::getDoubleTy(llvm::getGlobalContext()));

  llvm::FunctionType *fun_type = llvm::FunctionType::get(llvm::Type::getDoubleTy(llvm::getGlobalContext()), doubles, false);

  llvm::Function *f = llvm::Function::Create(fun_type, llvm::Function::ExternalLinkage, m_name, the_module);

  // if f conflicted, there was already something named m_name. if it has a
  // body, don't allow redefinition or reextern
  if(f->getName() != m_name)
  {
    // delete the one we just made and get the existing one
    f->eraseFromParent();
    f = the_module->getFunction(m_name);

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
    named_values[m_args[index]] = arg_iter;
  }

  return f;
}

llvm::Function *function_ast::codegen()
{
  named_values.clear();

  llvm::Function *f = m_proto->codegen();
  if(f == 0) return 0;

  // create a new basic block to start insertion into
  llvm::BasicBlock *bb = llvm::BasicBlock::Create(llvm::getGlobalContext(), "entry", f);
  builder.SetInsertPoint(bb);

  if(llvm::Value *return_value = m_body->codegen())
  {
    // finish off the function
    builder.CreateRet(return_value);

    // validate the generated code, checking for consistency
    llvm::verifyFunction(*f);

    // optimize the function
    the_fpm->run(*f);

    return f;
  }

  // error reading body, remove function
  f->eraseFromParent();
  return 0;
}

llvm::Value *if_expr_ast::codegen()
{
  llvm::Value *cond_value = m_cond->codegen();
  if(cond_value == 0) return 0;

  // convert condition to a bool by comparing equal to 0
  cond_value =
    builder.CreateFCmpONE(cond_value,
                          llvm::ConstantFP::get(llvm::getGlobalContext(), llvm::APFloat(0.0)),
                          "ifcond");

  llvm::Function *fun = builder.GetInsertBlock()->getParent();

  // create blocks for the then and else cases. insert the "then" block at the
  // end of the function
  llvm::BasicBlock *then_bb  = llvm::BasicBlock::Create(llvm::getGlobalContext(), "then", fun);
  llvm::BasicBlock *else_bb  = llvm::BasicBlock::Create(llvm::getGlobalContext(), "else", fun);
  llvm::BasicBlock *merge_bb = llvm::BasicBlock::Create(llvm::getGlobalContext(), "ifcont", fun);

  builder.CreateCondBr(cond_value, then_bb, else_bb);

  // emit then value
  builder.SetInsertPoint(then_bb);

  llvm::Value *then_value = m_then_expr->codegen();
  if(then_value == 0) return 0;

  builder.CreateBr(merge_bb);

  // codegen of the then can change the current block, update then_bb for the phi
  then_bb = builder.GetInsertBlock();

  // emit else block
  fun->getBasicBlockList().push_back(else_bb);
  builder.SetInsertPoint(else_bb);

  llvm::Value *else_value = m_else_expr->codegen();
  if(else_value == 0) return 0;

  builder.CreateBr(merge_bb);

  // codegen of the else can change the current block, update else_bb for the phi
  else_bb = builder.GetInsertBlock();

  // emit merge block
  fun->getBasicBlockList().push_back(merge_bb);
  builder.SetInsertPoint(merge_bb);
  llvm::PHINode *phi_node = builder.CreatePHI(llvm::Type::getDoubleTy(llvm::getGlobalContext()), 2, "iftmp");

  phi_node->addIncoming(then_value, then_bb);
  phi_node->addIncoming(else_value, else_bb);

  return phi_node;
}

llvm::Value *for_expr_ast::codegen()
{
  // emit the start code first, without "variable" in scope
  llvm::Value *start_value = m_start->codegen();
  if(start_value == 0) return 0;
  
  // make the new basic block for the loop header, inserting after current block
  llvm::Function *f = builder.GetInsertBlock()->getParent();
  llvm::BasicBlock *preheader_bb = builder.GetInsertBlock();
  llvm::BasicBlock *loop_bb = llvm::BasicBlock::Create(llvm::getGlobalContext(), "loop", f);

  // insert an explicit fall through from the current block to the loop_bb
  builder.CreateBr(loop_bb);

  // start insertion in loop_bb
  builder.SetInsertPoint(loop_bb);

  // start with the PHI node with an entry for start
  llvm::PHINode *variable = builder.CreatePHI(llvm::Type::getDoubleTy(llvm::getGlobalContext()), 2, m_variable_name.c_str());
  variable->addIncoming(start_value, preheader_bb);

  // within the loop, the variableis defined equal to the PHI node. if it
  // shadows an existing variable, we have to restore it, so save it now.
  llvm::Value *old_value = named_values[m_variable_name];
  named_values[m_variable_name] = variable;

  // emit the body of the loop. this, like any other expr, can change the
  // current basic block. note that we ignore the value computed by the body,
  // but don't allow an error
  if(m_body->codegen() == 0)
  {
    return 0;
  }

  // emit the step value.
  llvm::Value *step_value = 0;
  if(m_step)
  {
    step_value = m_step->codegen();
    if(step_value == 0) return 0;
  }
  else
  {
    // if not specified, use 1.0
    step_value = llvm::ConstantFP::get(llvm::getGlobalContext(), llvm::APFloat(1.0));
  }

  llvm::Value *next_variable = builder.CreateFAdd(variable, step_value, "nextvar");

  // compute the end condition
  llvm::Value *end_condition = m_end->codegen();
  if(end_condition == 0) return 0;

  // convert condition to a bool by comparing equal to 0.0
  end_condition =
    builder.CreateFCmpONE(end_condition,
                          llvm::ConstantFP::get(llvm::getGlobalContext(), llvm::APFloat(0.0)),
                          "loopcond");

  // create the "after loop" block and insert it
  llvm::BasicBlock *loop_end_bb = builder.GetInsertBlock();
  llvm::BasicBlock *after_bb = llvm::BasicBlock::Create(llvm::getGlobalContext(), "afterloop", f);

  // insert the conditional branch into the end of loop_end_bb
  builder.CreateCondBr(end_condition, loop_bb, after_bb);

  // any new code will be inserted in after_bb
  builder.SetInsertPoint(after_bb);

  // add a new entry to the PHI node for the backedge
  variable->addIncoming(next_variable, loop_end_bb);

  // restore the unshadowed variable
  if(old_value)
  {
    named_values[m_variable_name] = old_value;
  }
  else
  {
    named_values.erase(m_variable_name);
  }

  // for expr always returns 0.0
  return llvm::Constant::getNullValue(llvm::Type::getDoubleTy(llvm::getGlobalContext()));
}

extern "C" double putchard(double x)
{
  std::putchar(static_cast<char>(x));
  return 0;
}

