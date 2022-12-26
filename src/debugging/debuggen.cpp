#pragma once

#include <vector>
#include "../platform/typedefs/typedefs.hpp"
#include "../platform/llvm/llvm_include.hpp"
#include "./debuginfo.cpp"
#include "../ast/ast.cpp"

void
DebugInfo::emitLocation(ExprAST *AST)
{
    if (!AST)
    {
        return Builder->SetCurrentDebugLocation(llvm::DebugLoc());
    }

    llvm::DIScope *Scope;
    if (LexicalBlocks.empty())
    {
        Scope = TheCU;
    }
    else
    {
        Scope = LexicalBlocks.back();
    }

    Builder->SetCurrentDebugLocation(llvm::DILocation::get(Scope->getContext(), AST->getLine(), AST->getCol(), Scope));
}

internal llvm::DISubroutineType *
CreateFunctionType(unsigned NumArgs)
{
    llvm::SmallVector<llvm::Metadata *, 8> EltTys;
    llvm::DIType *DblTy = KSDbgInfo.getDoubleTy();

    // Add the result type
    EltTys.push_back(DblTy);

    for (unsigned i = 0, e = NumArgs; i != e; ++i)
    {
        EltTys.push_back(DblTy);
    }
    
    return DBuilder->createSubroutineType(DBuilder->getOrCreateTypeArray(EltTys));
}
