#pragma once


#ifdef PLATFORM_LINUX
    #include "linux_llvm_include.hpp"
#else
    #include "linux_llvm_include.hpp" // Default to Linux llvm .h files
#endif

#include "../typedefs/typedefs.hpp"
#include <map>
#include <string>

namespace llvml = llvm::legacy;

global_variable std::unique_ptr<llvm::LLVMContext> TheContext;
global_variable std::unique_ptr<llvm::Module> TheModule;
global_variable std::unique_ptr<llvm::IRBuilder<>> Builder;
global_variable std::map<std::string, llvm::Value*> NamedValues;
global_variable std::unique_ptr<llvml::FunctionPassManager> TheFPM;