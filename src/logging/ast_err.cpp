#pragma once
// NOTE(srp): Still not final platform-independent code

#include "../platform/typedefs/typedefs.hpp"

#include "err.cpp"

internal llvm::Value*
LogErrorV(const char *Str)
{
    LogError(Str);
    return nullptr;
}

internal llvm::Function*
LogErrorF(const char* Str)
{
    LogError(Str);
    return nullptr;
}
