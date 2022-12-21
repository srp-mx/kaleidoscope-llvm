#pragma once
// NOTE(srp): Still not final platform-independent code

#include "kaleidoscope.hpp"

#include "lexer/lexer.cpp"
#include "ast/ast.cpp"
#include "ast/ast_codegen.cpp"
#include "parser/parser.cpp"
#include <memory>

// NOTE(srp): Top-level parsing and JIT driver

internal void
InitializeModule()
{
    // Open a new context and module.
    TheContext = std::make_unique<llvm::LLVMContext>();
    TheModule = std::make_unique<llvm::Module>("my cool jit", *TheContext);

    // Create a new builder for the module.
    Builder = std::make_unique<llvm::IRBuilder<>>(*TheContext);
}

internal void 
InitializePassManager()
{
    // Create a new pass manager attached to it.
    TheFPM = std::make_unique<llvml::FunctionPassManager>(TheModule.get());

    // Do simple "peephole" and bit-twiddling optimizations.
    TheFPM->add(llvm::createInstructionCombiningPass());
    // Reassociate expressions
    TheFPM->add(llvm::createReassociatePass());
    // Eliminate Common SubExpressions
    TheFPM->add(llvm::createGVNPass());
    // Simplify the control flow graph (deleting unreachable blocks, etc.)
    TheFPM->add(llvm::createCFGSimplificationPass());

    TheFPM->doInitialization();
    // TODO(srp): See https://llvm.org/docs/Passes.html
}

internal void
InitializeLLVM()
{
    InitializeModule();
    InitializePassManager();
}

internal void
HandleDefinition()
{
    if (auto FnAST = ParseDefinition())
    {
        if (auto *FnIR = FnAST->codegen())
        {
            fprintf(stderr, "Read function definition:");
            FnIR->print(llvm::errs());
            fprintf(stderr, "\n");
        }
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
    if (auto ProtoAST = ParseExtern())
    {
        if (auto *FnIR = ProtoAST->codegen())
        {
            fprintf(stderr, "Read extern: ");
            FnIR->print(llvm::errs());
            fprintf(stderr, "\n");
        }
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
    if (auto FnAST = ParseTopLevelExpr())
    {
        if (auto *FnIR = FnAST->codegen())
        {
            fprintf(stderr, "Read top-level expression: ");
            FnIR->print(llvm::errs());
            fprintf(stderr, "\n");

            // Remove the anonymous expression
            FnIR->eraseFromParent();
        }
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
    while (true)
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

