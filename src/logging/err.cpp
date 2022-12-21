#pragma once
// NOTE(srp): Still not final platform-independent code

#include <memory>
#include "../platform/typedefs/typedefs.hpp"

#include "../ast/ast.cpp"

/// LogError* - These are little helper functions for error handling.
internal std::unique_ptr<ExprAST>
LogError(const char *Str)
{
    fprintf(stderr, "Error: %s\n", Str);
    return nullptr;
}

