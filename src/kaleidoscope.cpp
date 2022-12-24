#pragma once
// NOTE(srp): Still not final platform-independent code

#include "kaleidoscope.hpp"

#include "lexer/lexer.cpp"
#include "ast/ast.cpp"
#include "parser/parser.cpp"
#include "ast/ast_codegen.cpp"
#include <memory>

// NOTE(srp): Top-level parsing and JIT driver

internal void
InitializeJIT()
{
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    TheJIT = ExitOnErr(llvmo::KaleidoscopeJIT::Create());
}

internal void
InitializeModule()
{
    // Open a new context and module.
    TheContext = std::make_unique<llvm::LLVMContext>();
    TheModule = std::make_unique<llvm::Module>("my cool jit", *TheContext);
    TheModule->setDataLayout(TheJIT->getDataLayout());

    // Create a new builder for the module.
    Builder = std::make_unique<llvm::IRBuilder<>>(*TheContext);
}

internal void 
InitializePassManager()
{
    // Create a new pass manager attached to it.
    TheFPM = std::make_unique<llvml::FunctionPassManager>(TheModule.get());
    
    //Promote allocas to registers.
    TheFPM->add(llvm::createPromoteMemoryToRegisterPass());
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
    InitializeJIT();
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
            fprintf(stderr, "Read function definition:\n");
            FnIR->print(llvm::errs());
            fprintf(stderr, "\n");
            ExitOnErr(TheJIT->addModule(llvmo::ThreadSafeModule(std::move(TheModule), std::move(TheContext))));
            InitializeModule();
            InitializePassManager();
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
            fprintf(stderr, "Read extern: \n");
            FnIR->print(llvm::errs());
            fprintf(stderr, "\n");
            FunctionProtos[ProtoAST->getName()] = std::move(ProtoAST);
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
            // Prints IR
            fprintf(stderr, "Read top level expression: \n");
            FnIR->print(llvm::errs());
            fprintf(stderr, "\n");

            // Create a ResourceTracker to track JIT'd memory allocated to our
            // anonymous expression -- that way we can free it after executing.
            auto RT = TheJIT->getMainJITDylib().createResourceTracker();

            auto TSM = llvmo::ThreadSafeModule(std::move(TheModule), std::move(TheContext));
            ExitOnErr(TheJIT->addModule(std::move(TSM), RT));
            InitializeModule();
            InitializePassManager();

            // Search the JIT for the __anon_expr symbol.
            auto ExprSymbol = ExitOnErr(TheJIT->lookup("{__anon_expr}"));
            assert(ExprSymbol && "Function not found\n");

            // Get the symbol's address and cast it to the right type (takes no
            // arguments, returns a real64) so we can call it as a native function.
            real64 (*FP)() = (real64 (*)())(intptr_t)ExprSymbol.getAddress();
            fprintf(stderr, "\nEvaluated to %f\n", FP());

            // Delete the anonymous expression module from the JIT.
            ExitOnErr(RT->remove());
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

