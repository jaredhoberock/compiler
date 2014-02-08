#include <string>
#include <stdexcept>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Analysis/Passes.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/Transforms/Scalar.h>
#include "driver.hpp"

llvm::Module *init_native_target_and_create_module()
{
  llvm::InitializeNativeTarget();
  return new llvm::Module("my cool jit", llvm::getGlobalContext());
}

llvm::ExecutionEngine *create_execution_engine(llvm::Module *m)
{
  std::string error_string;
  llvm::ExecutionEngine *result = llvm::EngineBuilder(m).setErrorStr(&error_string).create();
  if(!result)
  {
    std::string what("Could not create ExecutionEngine: ");
    what += error_string;
    throw std::runtime_error(error_string);
  }

  return result;
}

driver::driver(const std::string &filename)
  : m_filename(filename),
    m_module(init_native_target_and_create_module()),
    m_fpm(m_module),
    m_execution_engine(create_execution_engine(m_module)),
    m_builder(llvm::getGlobalContext())
{
  // set up the optimizer pipeline. start with registering info about how the
  // target lays out data structures.
  m_fpm.add(new llvm::DataLayout(*m_execution_engine->getDataLayout()));

  // provide basic AliasAnalysis support for GVN
  m_fpm.add(llvm::createBasicAliasAnalysisPass());

  // do simple "peephole" optimizations and bit-twiddling optimizations
  m_fpm.add(llvm::createInstructionCombiningPass());

  // reassociate expressions
  m_fpm.add(llvm::createReassociatePass());

  // eliminate common subexpressions
  m_fpm.add(llvm::createGVNPass());

  // simplify the control flow graph (deleting unreachable blocks, etc)
  m_fpm.add(llvm::createCFGSimplificationPass());

  m_fpm.doInitialization();
}

