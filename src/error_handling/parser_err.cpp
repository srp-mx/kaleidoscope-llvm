#pragma once
// NOTE(srp): Still not final platform-independent code

#include <memory>
#include "../platform/typedefs/typedefs.hpp"

#include "../ast/ast.cpp"

/// LogError* - These are little helper functions for error handling.
std::unique_ptr<ExprAST>
LogError(const char *Str)
{
    fprintf(stderr, "LogError: %s\n", Str);
    return nullptr;
}

std::unique_ptr<PrototypeAST>
LogErrorP(const char *Str)
{
    LogError(Str);
    return nullptr;
}

