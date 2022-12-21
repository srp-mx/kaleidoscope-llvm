#pragma once

#include "../platform/llvm/llvm_include.hpp"
#include "../platform/typedefs/typedefs.hpp"

#include "./ast.cpp"
#include "../logging/ast_err.cpp"

#include <string>

llvm::Function *getFunction(std::string Name)
{
    // First, see if the function has already been added to the current module.
    if (auto *F = TheModule->getFunction(Name))
    {
        return F;
    }

    // If not, check whether we can codegen the declaration from some existing
    // prototype.
    auto FI = FunctionProtos.find(Name);
    if (FI != FunctionProtos.end())
    {
        return FI->second->codegen();
    }

    // If no existing prototype exists, return null.
    return nullptr;
}

llvm::Value*
NumberExprAST::codegen()
{
    return llvm::ConstantFP::get(*TheContext, llvm::APFloat(Val));
}

llvm::Value*
VariableExprAST::codegen()
{
    // Look this variable up in the function
    llvm::Value *V = NamedValues[Name];
    if (!V)
    {
        return LogErrorV("Unknown variable name");
    }
    return V;
}

llvm::Value*
BinaryExprAST::codegen()
{
    llvm::Value *L = LHS->codegen();
    llvm::Value *R = RHS->codegen();
    if (!L || !R)
    {
        return nullptr;
    }

    switch (Op)
    {
        case '+':
            return Builder->CreateFAdd(L, R, "addtmp");
        case '-':
            return Builder->CreateFSub(L, R, "subtmp");
        case '*':
            return Builder->CreateFMul(L, R, "multmp");
        case '<':
            L = Builder->CreateFCmpULT(L, R, "cmptmp");
            return Builder->CreateUIToFP(L, llvm::Type::getDoubleTy(*TheContext), "booltmp");
        default:
            return LogErrorV("invalid binary operator");
    }
}

llvm::Value *
CallExprAST::codegen()
{
    // Look up the name in the global module table.
    llvm::Function *CalleeF = getFunction(Callee);
    if (!CalleeF)
    {
        return LogErrorV("Unknown function referenced");
    }

    // If argument mismatch error.
    if (CalleeF->arg_size() != Args.size())
    {
        return LogErrorV("Incorrect # arguments passed");
    }

    std::vector<llvm::Value*> ArgsV;
    for (unsigned i = 0, e = Args.size(); i != e; ++i)
    {
        ArgsV.push_back(Args[i]->codegen());
        if (!ArgsV.back())
        {
            return nullptr;
        }
    }

    return Builder->CreateCall(CalleeF, ArgsV, "calltmp");
}

llvm::Value *
IfExprAST::codegen()
{
    llvm::Value *CondV = Cond->codegen();
    if (!CondV)
    {
        return nullptr;
    }

    // Convert condition to a bool by comparing non-equal to 0.0
    CondV = Builder->CreateFCmpONE(CondV, llvm::ConstantFP::get(*TheContext, llvm::APFloat(0.0)), "ifcond");

    llvm::Function *TheFunction = Builder->GetInsertBlock()->getParent();

    // Create blocks for the then and else cases. Insert the 'then' block at the
    // end of the function.
    llvm::BasicBlock *ThenBB = llvm::BasicBlock::Create(*TheContext, "then", TheFunction);
    llvm::BasicBlock *ElseBB = llvm::BasicBlock::Create(*TheContext, "else");
    llvm::BasicBlock *MergeBB = llvm::BasicBlock::Create(*TheContext, "ifcont");

    Builder->CreateCondBr(CondV, ThenBB, ElseBB);

    // Emit then value
    Builder->SetInsertPoint(ThenBB);

    llvm::Value *ThenV = Then->codegen();
    if (!ThenV)
    {
        return nullptr;
    }

    Builder->CreateBr(MergeBB);
    // Codegen of 'Then' can change the current block, update ThenBB for the PHI
    ThenBB = Builder->GetInsertBlock();

    // Emit else block
    llvm_Function_insert(TheFunction, TheFunction->end(), ElseBB);
    Builder->SetInsertPoint(ElseBB);

    llvm::Value *ElseV = Else->codegen();
    if (!ElseV)
    {
        return nullptr;
    }

    Builder->CreateBr(MergeBB);
    // codegen of 'Else' can change the current block, update ElseBB for the PHI
    ElseBB = Builder->GetInsertBlock();

    // Emit merge block
    llvm_Function_insert(TheFunction, TheFunction->end(), MergeBB);
    Builder->SetInsertPoint(MergeBB);
    llvm::PHINode *PN = Builder->CreatePHI(llvm::Type::getDoubleTy(*TheContext), 2, "iftmp");

    PN->addIncoming(ThenV, ThenBB);
    PN->addIncoming(ElseV, ElseBB);
    return PN;
}

llvm::Function *
PrototypeAST::codegen()
{
    // Make the function type: double(double, double) etc.
    std::vector<llvm::Type*> Doubles(Args.size(), llvm::Type::getDoubleTy(*TheContext));

    llvm::FunctionType *FT = llvm::FunctionType::get(llvm::Type::getDoubleTy(*TheContext), Doubles, false);

    llvm::Function *F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, Name, TheModule.get());

    unsigned Idx = 0;
    for (auto &Arg : F->args())
    {
        Arg.setName(Args[Idx++]);
    }

    return F;
}

llvm::Function *
FunctionAST::codegen()
{
    // Transfer ownership of the prototype to the FunctionProtos map, but keep a
    // reference to it for use below.
    auto &P = *Proto;
    FunctionProtos[Proto->getName()] = std::move(Proto);
    llvm::Function *TheFunction = getFunction(P.getName());
    if (!TheFunction)
    {
        return nullptr;
    }

    // Create a new basic block to start insertion into
    llvm::BasicBlock *BB = llvm::BasicBlock::Create(*TheContext, "entry", TheFunction);
    Builder->SetInsertPoint(BB);

    // Record the function arguments in the NamedValues map.
    NamedValues.clear();
    for (auto &Arg : TheFunction->args())
    {
        NamedValues[std::string(Arg.getName())] = &Arg;
    }

    if (llvm::Value *RetVal = Body->codegen())
    {
        // Finish off the function.
        Builder->CreateRet(RetVal);

        // Validate the generated code, checking for consistency
        llvm::verifyFunction(*TheFunction);

        // Optimize the function
        TheFPM->run(*TheFunction);

        return TheFunction;
    }

    // Error reading body, remove function.
    TheFunction->eraseFromParent();
    return nullptr;
    // TODO(srp): bug: If the FunctionAST::codegen() method finds an existing 
    // IR Function, it does not validate its signature against the definition’s 
    // own prototype. This means that an earlier ‘extern’ declaration will take 
    // precedence over the function definition’s signature, which can cause codegen 
    // to fail, for instance if the function arguments are named differently.
    // TESTCASE:
    // extern foo(a);
    // def foo(b) b;
}

// NOTE(srp): Interesting: https://llvm.org/docs/LangRef.html
