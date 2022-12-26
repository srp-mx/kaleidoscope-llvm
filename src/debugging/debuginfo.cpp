#pragma once
// NOTE(srp): Not final platform-independent layer

#include <vector>
#include "../platform/typedefs/typedefs.hpp"
#include "../platform/llvm/llvm_include.hpp"

class PrototypeAST;
class ExprAST;

struct DebugInfo
{
    llvm::DICompileUnit *TheCU;
    llvm::DIType *DblTy;
    std::vector<llvm::DIScope*> LexicalBlocks;

    void emitLocation(ExprAST *AST);
    llvm::DIType *getDoubleTy();
} KSDbgInfo;

llvm::DIType *
DebugInfo::getDoubleTy()
{
    if (DblTy)
    {
        return DblTy;
    }

    DblTy = DBuilder->createBasicType("double", 64, llvm::dwarf::DW_ATE_float);
    return DblTy;
}

struct SourceLocation
{
    int32 Line;
    int32 Col;
};

global_variable SourceLocation CurLoc;
global_variable SourceLocation LexLoc = {1, 0};

internal int32
advance()
{
    int32 LastChar = getchar();
    
    // TODO(srp): Test possible bug in CRLF drifting an extra line
    if (LastChar == '\n' || LastChar == '\r')
    {
        LexLoc.Line++;
        LexLoc.Col = 0;
    }
    else
    {
        LexLoc.Col++;
    }
    return LastChar;
}

