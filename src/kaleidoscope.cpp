#pragma once
// NOTE(srp): Still not final platform-independent code

#include "kaleidoscope.hpp"

#include "lexer/lexer.cpp"
#include "ast/ast.cpp"
#include "parser/parser.cpp"
#include "ast/ast_codegen.cpp"
#include <memory>
#include <system_error>

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

internal int32
CompileObjectCode()
{
    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmParsers();
    llvm::InitializeAllAsmPrinters();

    auto TargetTriple = llvm::sys::getDefaultTargetTriple();
    TheModule->setTargetTriple(TargetTriple);

    std::string Error;
    auto Target = llvm::TargetRegistry::lookupTarget(TargetTriple, Error);

    // Print an error and exit if we couldn't find the requested target.
    // This generally occurs if we've forgotten to initialise the
    // TargetRegistry or we have a bogus target triple
    if (!Target)
    {
        llvm::errs() << Error;
        return 1; // NOTE(srp): main should return this.
    }

    auto CPU = "generic";
    auto Features = "";

    llvm::TargetOptions opt;
    auto RM = llvm::Optional<llvm::Reloc::Model>();
    auto TheTargetMachine = Target->createTargetMachine(TargetTriple, CPU, Features, opt, RM);

    TheModule->setDataLayout(TheTargetMachine->createDataLayout());

    auto Filename = "output.o";
    std::error_code EC;
    llvm::raw_fd_ostream dest(Filename, EC, llvm::sys::fs::OF_None);

    if (EC)
    {
        llvm::errs() << "Could not open file: " << EC.message();
        return 1; // NOTE(srp): main should return this.
    }

    llvml::PassManager pass;
    auto FileType = llvm::CGFT_ObjectFile;

    if (TheTargetMachine->addPassesToEmitFile(pass, dest, nullptr,  FileType))
    {
        llvm::errs() << "TheTargetMachine can't emit a file of this type";
        return 1; // NOTE(srp): main should return this.
    }

    pass.run(*TheModule);
    dest.flush();

    llvm::outs() << "Wrote " << Filename << "\n";

    return 0; // NOTE(srp): no errors
}

internal void
InitializeLLVM()
{
    InitializeModule();
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
        FnAST->codegen();
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

