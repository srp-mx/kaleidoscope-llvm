#pragma once

#include "llvm/IR/Function.h"

llvm::Function::iterator llvm_Function_insert(llvm::Function::iterator Position, llvm::BasicBlock *BB);

#if MESSED_UP_FUNCTION_H
// For some reason this function is missing from /usr/include/llvm/IR/Function.h
llvm::Function::iterator llvm_Function_insert(llvm::Function *thisf, llvm::Function::iterator Position, llvm::BasicBlock *BB)
{
    return thisf->getBasicBlockList().insert(Position, BB);
}
#else
llvm::Function::iterator llvm_Function_insert(llvm::Function *thisf, llvm::Function::iterator Position, llvm::BasicBlock *BB)
{
    return thisf->insert(Position, BB);
}
#endif
