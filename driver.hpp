#pragma once

#include <llvm/Analysis/Passes.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/JIT.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/PassManager.h>
#include <string>
#include "location.hh"

class driver
{
  public:
    inline explicit driver()
      : driver(std::string())
    {}

    inline ~driver()
    {
      m_module->dump();
      delete m_execution_engine;
    }

    driver(const std::string &filename);

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

    inline std::string &filename()
    {
      return m_filename;
    }

    inline llvm::Module &module()
    {
      return *m_module;
    }

    inline llvm::FunctionPassManager &fpm()
    {
      return m_fpm;
    }

    inline llvm::ExecutionEngine &execution_engine()
    {
      return *m_execution_engine;
    }

    inline std::map<std::string,llvm::Value*> &named_values()
    {
      return m_named_values;
    }

    inline llvm::IRBuilder<> &builder()
    {
      return m_builder;
    }

  private:
    std::string m_filename;
    llvm::Module *m_module;
    llvm::FunctionPassManager m_fpm;
    llvm::ExecutionEngine *m_execution_engine;
    llvm::IRBuilder<> m_builder;
    std::map<std::string,llvm::Value*> m_named_values;
};

