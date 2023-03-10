#pragma once


#if PLATFORM_LINUX
    #include "linux_llvm_include.hpp"
#else
    #include "linux_llvm_include.hpp" // Default to Linux llvm .h files
#endif

#include "../typedefs/typedefs.hpp"
#include "../../jit/KaleidoscopeJIT.h"
#include <map>
#include <string>

namespace llvml = llvm::legacy;
namespace llvmo = llvm::orc;

class PrototypeAST;

global_variable std::unique_ptr<llvm::LLVMContext> TheContext;
global_variable std::unique_ptr<llvm::Module> TheModule;
global_variable std::unique_ptr<llvm::IRBuilder<>> Builder;
global_variable std::unique_ptr<llvm::DIBuilder> DBuilder;
global_variable llvm::ExitOnError ExitOnErr;

global_variable std::map<std::string, llvm::AllocaInst*> NamedValues;
global_variable std::unique_ptr<llvmo::KaleidoscopeJIT> TheJIT;
global_variable std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;
