#pragma once
// NOTE(srp): Still not final platform-independent code

#include "kaleidoscope.hpp"

#include "lexer/lexer.cpp"
#include "ast/ast.cpp"
#include "parser/parser.cpp"

internal void
HandleDefinition()
{
    if (ParseDefinition())
    {
        fprintf(stderr, "Parsed a function definition.\n");
    }
    else
    {
        // Skip token for error recovery.
        getNextToken();
    }

}

internal void
HandleExtern()
{
    if (ParseExtern())
    {
        fprintf(stderr, "Parsed an extern\n");
    }
    else
    {
        // Skip token for error recovery.
        getNextToken();
    }
}

internal void
HandleTopLevelExpression()
{
    // Evaluate a top-level expression into an anonymous function.
    if (ParseTopLevelExpr())
    {
        fprintf(stderr, "Parsed a top-level expr\n");
    }
    else
    {
        // Skip token for error recovery.
        getNextToken();
    }
}

internal void
MainLoop()
{
    while (1)
    {
        fprintf(stderr, "ready> ");
        switch (CurTok)
        {
            case tok_eof:
                return;
            case ';': // ignore top-level semicolons.
                getNextToken();
                break;
            case tok_def:
                HandleDefinition();
                break;
            case tok_extern:
                HandleExtern();
                break;
            default:
                HandleTopLevelExpression();
                break;
        }
    }
}

