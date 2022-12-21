#pragma once
// NOTE(srp): Still not final platform-independent code

#include <memory>
#include "../platform/typedefs/typedefs.hpp"

#include "err.cpp"

internal std::unique_ptr<PrototypeAST>
LogErrorP(const char *Str)
{
    LogError(Str);
    return nullptr;
}
