#pragma once
// NOTE(srp): Still not final platform-independent code

#include <memory>
#include <vector>
#include <string>
#include "../platform/llvm/llvm_include.hpp"
#include "../platform/typedefs/typedefs.hpp"

/// ExprAST - Base class for all expression nodes.
class ExprAST
{
    // TODO(srp): Add a type field
    public:
        virtual ~ExprAST() = default;
        virtual llvm::Value *codegen() = 0;
};

/// NumberExprAST - Expression class for numeric literals like "1.0"
class NumberExprAST : public ExprAST
{
    real64 Val;

    public:
        NumberExprAST(real64 Val) : Val(Val) {}
        llvm::Value *codegen() override;
};

/// VariableExprAST - Expression class for referencing a variable, like "a".
class VariableExprAST : public ExprAST
{
    std::string Name;

    public:
        VariableExprAST(const std::string &Name) : Name(Name) {}
        llvm::Value *codegen() override;
};

/// UnaryExprAST - Expression class for a unary operator.
class UnaryExprAST : public ExprAST
{
    char Opcode;
    std::unique_ptr<ExprAST> Operand;

    public:
        UnaryExprAST(char Opcode, std::unique_ptr<ExprAST> Operand)
            : Opcode(Opcode), Operand(std::move(Operand)) {}

        llvm::Value *codegen() override;
};

/// BinaryExprAST - Expression class for a binary operator.
class BinaryExprAST : public ExprAST
{
    char Op;
    std::unique_ptr<ExprAST> LHS, RHS;

    public:
        BinaryExprAST(char op, std::unique_ptr<ExprAST> LHS, std::unique_ptr<ExprAST> RHS)
            : Op(op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
        llvm::Value *codegen() override;
};

/// CallExprAST - Expression class for function calls.
class CallExprAST : public ExprAST
{
    std::string Callee;
    std::vector<std::unique_ptr<ExprAST>> Args;

    public:
        CallExprAST(const std::string &Callee, std::vector<std::unique_ptr<ExprAST>> Args)
            : Callee(Callee), Args(std::move(Args)) {}
        llvm::Value *codegen() override;
};

/// IfExprAST - Expression class for if/then/else
class IfExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> Cond, Then, Else;

    public:
        IfExprAST(std::unique_ptr<ExprAST> Cond, std::unique_ptr<ExprAST> Then, std::unique_ptr<ExprAST> Else)
            :Cond(std::move(Cond)), Then(std::move(Then)), Else(std::move(Else)) {}

        llvm::Value *codegen() override;
};

/// ForExprAST - Expression class for for/in
class ForExprAST : public ExprAST
{
    std::string VarName;
    std::unique_ptr<ExprAST> Start, End, Step, Body;

    public:
        ForExprAST(const std::string &VarName, std::unique_ptr<ExprAST> Start,
                std::unique_ptr<ExprAST> End, std::unique_ptr<ExprAST> Step,
                std::unique_ptr<ExprAST> Body)
            : VarName(VarName), Start(std::move(Start)), End(std::move(End)), Step(std::move(Step)), 
                Body(std::move(Body)) {}

        llvm::Value *codegen() override;
};

/// PrototypeAST - This class represents the "prototype" for a function,
/// which captures its name and its argument names (thus, the number of args too)
class PrototypeAST
{
    std::string Name;
    std::vector<std::string> Args;

    bool32 IsOperator;
    unsigned Precedence; // Precedence if a binop

    public:
        PrototypeAST(const std::string &Name, std::vector<std::string> Args,
                bool32 IsOperator = false, unsigned Prec = 0)
            : Name(Name), Args(std::move(Args)), IsOperator(IsOperator), Precedence(Prec) {}

        llvm::Function *codegen();
        const std::string &getName() const { return Name; }

        bool32 isUnaryOp() const { return IsOperator && Args.size() == 1; }
        bool32 isBinaryOp() const { return IsOperator && Args.size() == 2; }

        char getOperatorName() const
        {
            assert(isUnaryOp() || isBinaryOp());
            return Name[Name.size() - 2]; // since the names are '{unary:}' we
                                          // want the second to last char
        }

        unsigned getBinaryPrecedence() const { return Precedence; }
};

/// FunctionAST - This class represents a function definition itself.
class FunctionAST
{
    std::unique_ptr<PrototypeAST> Proto;
    std::unique_ptr<ExprAST> Body;

    public:
        FunctionAST(std::unique_ptr<PrototypeAST> Proto, std::unique_ptr<ExprAST> Body)
            : Proto(std::move(Proto)), Body(std::move(Body)) {}
        llvm::Function *codegen();
};


