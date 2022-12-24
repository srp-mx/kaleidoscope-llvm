#pragma once

#include "../platform/llvm/llvm_include.hpp"
#include "../platform/typedefs/typedefs.hpp"

#include "./ast.cpp"
#include "../logging/ast_err.cpp"

#include <string>

llvm::Function *
getFunction(std::string Name)
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

/// CreateEntryBlockAlloca - Create an alloca instruction in the entry block of
/// the function. This is used for mutable variables etc.
internal llvm::AllocaInst *
CreateEntryBlockAlloca(llvm::Function *TheFunction, llvm::StringRef VarName)
{
    llvm::IRBuilder<> TmpB(&TheFunction->getEntryBlock(), TheFunction->getEntryBlock().begin());
    return TmpB.CreateAlloca(llvm::Type::getDoubleTy(*TheContext), 0, VarName);
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
    llvm::AllocaInst *A = NamedValues[Name];
    if (!A)
    {
        return LogErrorV("Unknown variable name");
    }

    // Load the value
    return Builder->CreateLoad(A->getAllocatedType(), A, Name.c_str());
}

llvm::Value*
VarExprAST::codegen()
{
    // Variables getting shadowed
    std::vector<llvm::AllocaInst*> OldBindings;

    llvm::Function *TheFunction = Builder->GetInsertBlock()->getParent();

    // Register all variables and emit their initializer
    for (unsigned i = 0, e = VarNames.size(); i != e; ++i)
    {
        const std::string &VarName = VarNames[i].first;
        ExprAST *Init = VarNames[i].second.get();

        // Emit the initializer before adding the variable to scope, this prevents
        // the initializer from referencing the variable itself, and permits stuff
        // like this:
        // var a = 1 in
        //   var a = a in ...  # refers to outer 'a'
        llvm::Value *InitVal;
        if (Init)
        {
            InitVal = Init->codegen();
            if (!InitVal)
            {
                return nullptr;
            }
        }
        else
        {
            // If no initial value specifier, initialize to zero
            InitVal = llvm::ConstantFP::get(*TheContext, llvm::APFloat(0.0));
        }

        llvm::AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName);
        Builder->CreateStore(InitVal, Alloca);

        // Remember the old variable binding so that we can restore the binding when
        // we unrecurse.
        OldBindings.push_back(NamedValues[VarName]);

        // Remember this binding
        NamedValues[VarName] = Alloca;
    }

    // Codegen the body, now that all vars are in scope
    llvm::Value *BodyVal = Body->codegen();
    if (!BodyVal)
    {
        return nullptr;
    }

    // Pop all our variables from scope
    for (unsigned i = 0, e = VarNames.size(); i !=e; ++i)
    {
        NamedValues[VarNames[i].first] = OldBindings[i];
    }

    // Return the body computation
    return BodyVal;
}

llvm::Value *
UnaryExprAST::codegen()
{
    llvm::Value *OperandV = Operand->codegen();
    if (!OperandV)
    {
        return nullptr;
    }

    std::string unaryFuncName = std::string("{unary");
    unaryFuncName += Opcode;
    unaryFuncName += '}';
    llvm::Function *F = getFunction(unaryFuncName);
    if (!F)
    {
        return LogErrorV("Unknown unary operator");
    }

    return Builder->CreateCall(F, OperandV, "unop");
}

llvm::Value*
BinaryExprAST::codegen()
{
    // Special case '=' because we don't want to emit the LHS as an expression
    if (Op == '=')
    {
        // Assignment requires the LHS to be an identifier
        // This assumes we're building without RTTI because LLVM builds that way by
        // default. If you build LLVM with RTTI this can be changed to a
        // dynamic_cast for automatic error checking.
        VariableExprAST *LHSE = static_cast<VariableExprAST*>(LHS.get());
        if (!LHSE)
        {
            return LogErrorV("destination of '=' must be a variable");
        }

        // Codegen the RHS
        llvm::Value *Val = RHS->codegen();
        if (!Val)
        {
            return nullptr;
        }

        // Look up the name
        llvm::Value *Variable = NamedValues[LHSE->getName()];
        if (!Variable)
        {
            return LogErrorV("Unknown variable name");
        }

        Builder->CreateStore(Val, Variable);
        return Val;
    }

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
            // Convert bool 0/1 to double 0.0 or 1.0
            return Builder->CreateUIToFP(L, llvm::Type::getDoubleTy(*TheContext), "booltmp");
        default:
            break;
    }

    // If it wasn't a builtin binary operator, it must be a user defined one. Emit
    // a call to it.
    llvm::Function *F = getFunction((std::string("{binary") + Op) + "}");
    assert(F && "binary operator not found!");

    llvm::Value *Ops[] = {L, R};
    return Builder->CreateCall(F, Ops, "binop");
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

llvm::Value *
ForExprAST::codegen()
{
    llvm::Function *TheFunction = Builder->GetInsertBlock()->getParent();

    // Create an alloca for the variable in the entry block
    llvm::AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName);

    // Emit the start code first, without 'variable' in scope
    llvm::Value *StartVal = Start->codegen();
    if (!StartVal)
    {
        return nullptr;
    }

    // Store the value into the alloca
    Builder->CreateStore(StartVal, Alloca);

    // Make the new basic block for the loop header, inserting after current
    // block.
    llvm::BasicBlock *LoopBB = llvm::BasicBlock::Create(*TheContext, "loop", TheFunction);

    // Insert an explicit fall through from the current block to the LoopBB
    Builder->CreateBr(LoopBB);

    // Start insertion in LoopBB
    Builder->SetInsertPoint(LoopBB);

    // If it shadows an existing variable, we have to restore it, so save the
    // shadowed value now.
    llvm::AllocaInst *OldVal = NamedValues[VarName];
    NamedValues[VarName] = Alloca;

    // Emit the body of the loop. This, like any other expr, can change the
    // current BB. Note that we ignore the value computed by the body, but don't
    // allow an error.
    if (!Body->codegen())
    {
        return nullptr;
    }

    // Emit the step value
    llvm::Value *StepVal = nullptr;
    if (Step)
    {
        StepVal = Step->codegen();
        if (!StepVal)
        {
            return nullptr;
        }
    }
    else
    {
        // If not specified, use 1.0.
        StepVal = llvm::ConstantFP::get(*TheContext, llvm::APFloat(1.0));
    }

    // Compute the end condition.
    llvm::Value *EndCond = End->codegen();
    if (!EndCond)
    {
        return nullptr;
    }

    // Reload, increment, and restore the alloca. This handles the case where
    // the body of the loop mutates the variable
    llvm::Value *CurVar = Builder->CreateLoad(Alloca->getAllocatedType(), Alloca, VarName.c_str());
    llvm::Value *NextVar = Builder->CreateFAdd(CurVar, StepVal, "nextvar");
    Builder->CreateStore(NextVar, Alloca);

    // Convert condition to a bool by comparing non-equal to 0.0
    EndCond = Builder->CreateFCmpONE(EndCond, llvm::ConstantFP::get(*TheContext, llvm::APFloat(0.0)), "loopcond");

    // Create the "after loop" block and insert it
    llvm::BasicBlock *AfterBB = llvm::BasicBlock::Create(*TheContext, "afterloop", TheFunction);

    // Insert the conditional branch into the end of LoopEndBB
    Builder->CreateCondBr(EndCond, LoopBB, AfterBB);

    // Any new code will be inserted in AfterBB
    Builder->SetInsertPoint(AfterBB);
    
    // Restore the unshadowed variable
    if (OldVal)
    {
        NamedValues[VarName] = OldVal;
    }
    else
    {
        NamedValues.erase(VarName);
    }

    // for expr always returns 0.0
    return llvm::Constant::getNullValue(llvm::Type::getDoubleTy(*TheContext));
} // TODO(srp): ```for n=1, n<0, 1 in putchard(48+n);``` should print nothing, but it prints ```1```.
  // loopcond should also be checked at entry BB

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

    // If this is an operator, install it
    if (P.isBinaryOp())
    {
        BinopPrecedence[P.getOperatorName()] = P.getBinaryPrecedence();
    }

    // Create a new basic block to start insertion into
    llvm::BasicBlock *BB = llvm::BasicBlock::Create(*TheContext, "entry", TheFunction);
    Builder->SetInsertPoint(BB);

    // Record the function arguments in the NamedValues map.
    NamedValues.clear();
    for (auto &Arg : TheFunction->args())
    {
        // Create an alloca for this variable
        llvm::AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, Arg.getName());

        // Store the initial value into the alloca
        Builder->CreateStore(&Arg, Alloca);

        // Add arguments to variable symbol table
        NamedValues[std::string(Arg.getName())] = Alloca;
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

    if (P.isBinaryOp())
    {
        BinopPrecedence.erase(P.getOperatorName());
    }

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
