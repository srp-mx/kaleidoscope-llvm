#pragma once
// NOTE(srp): Still not final platform-independent code

#include <memory>
#include <vector>
#include <string>
#include "../platform/llvm/llvm_include.hpp"
#include "../platform/typedefs/typedefs.hpp"
#include "../debugging/debuginfo.cpp"

llvm::raw_ostream &
indent(llvm::raw_ostream &O, int32 size)
{
    return O << std::string(size, ' ');
}

/// ExprAST - Base class for all expression nodes.
class ExprAST
{
    SourceLocation Loc;

    // TODO(srp): Add a type field
    public:
        ExprAST(SourceLocation Loc = CurLoc) : Loc(Loc) {}
        virtual ~ExprAST() {}
        virtual llvm::Value *codegen() = 0;

        int32 getLine() const { return Loc.Line; }
        int32 getCol() const { return Loc.Col; }

        virtual llvm::raw_ostream &
        dump(llvm::raw_ostream &out, int ind)
        {
            return out << ':' << getLine() << ':' << getCol() << '\n';
        }
};

/// NumberExprAST - Expression class for numeric literals like "1.0"
class NumberExprAST : public ExprAST
{
    real64 Val;

    public:
        NumberExprAST(real64 Val) : Val(Val) {}
        llvm::Value *codegen() override;

        llvm::raw_ostream &
        dump(llvm::raw_ostream &out, int32 ind) override
        {
            return ExprAST::dump(out << Val, ind);
        }
};

/// VariableExprAST - Expression class for referencing a variable, like "a".
class VariableExprAST : public ExprAST
{
    std::string Name;

    public:
        VariableExprAST(SourceLocation Loc, const std::string &Name) 
            : ExprAST(Loc), Name(Name) {}
        llvm::Value *codegen() override;
        const std::string &getName() const { return Name; }

        llvm::raw_ostream &
        dump(llvm::raw_ostream &out, int32 ind) override
        {
            return ExprAST::dump(out << Name, ind);
        }
};

/// VarExprAST - Expression class for var/in (variable creation)
class VarExprAST : public ExprAST
{
    std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames; // Local mutable variable list
    std::unique_ptr<ExprAST> Body; // Scope of the variable list

    public:
        VarExprAST(std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames,
                std::unique_ptr<ExprAST> Body)
            : VarNames(std::move(VarNames)), Body(std::move(Body)) {}

        llvm::Value *codegen() override;

        llvm::raw_ostream &
        dump(llvm::raw_ostream &out, int32 ind) override
        {
            ExprAST::dump(out << "var", ind);
            for (const auto &NamedVar : VarNames)
            {
                NamedVar.second->dump(indent(out, ind) << NamedVar.first << ':', ind + 1);
            }
            Body->dump(indent(out, ind) << "Body:", ind + 1);
            return out;
        }
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

        llvm::raw_ostream &
        dump(llvm::raw_ostream &out, int32 ind) override
        {
            ExprAST::dump(out << "{unary" << Opcode << "}", ind);
            Operand->dump(out, ind+1);
            return out;
        }
};

/// BinaryExprAST - Expression class for a binary operator.
class BinaryExprAST : public ExprAST
{
    char Op;
    std::unique_ptr<ExprAST> LHS, RHS;

    public:
        BinaryExprAST(SourceLocation Loc, char Op, std::unique_ptr<ExprAST> LHS, std::unique_ptr<ExprAST> RHS)
            : ExprAST(Loc), Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
        llvm::Value *codegen() override;

        llvm::raw_ostream &
        dump(llvm::raw_ostream &out, int32 ind) override
        {
            ExprAST::dump(out << "{binary" << Op << "}", ind);
            LHS->dump(indent(out, ind) << "LHS:", ind + 1);
            RHS->dump(indent(out, ind) << "RHS:", ind + 1);
            return out;
        }
};

/// CallExprAST - Expression class for function calls.
class CallExprAST : public ExprAST
{
    std::string Callee;
    std::vector<std::unique_ptr<ExprAST>> Args;

    public:
        CallExprAST(SourceLocation Loc, const std::string &Callee, std::vector<std::unique_ptr<ExprAST>> Args)
            : ExprAST(Loc), Callee(Callee), Args(std::move(Args)) {}
        llvm::Value *codegen() override;

        llvm::raw_ostream &
        dump(llvm::raw_ostream &out, int32 ind) override
        {
            ExprAST::dump(out << "call " << Callee, ind);
            for (const auto &Arg : Args)
            {
                Arg->dump(indent(out, ind + 1), ind + 1);
            }
            return out;
        }
};

/// IfExprAST - Expression class for if/then/else
class IfExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> Cond, Then, Else;

    public:
        IfExprAST(SourceLocation Loc, std::unique_ptr<ExprAST> Cond, std::unique_ptr<ExprAST> Then, std::unique_ptr<ExprAST> Else)
            : ExprAST(Loc), Cond(std::move(Cond)), Then(std::move(Then)), Else(std::move(Else)) {}

        llvm::Value *codegen() override;

        llvm::raw_ostream &
        dump(llvm::raw_ostream &out, int32 ind) override
        {
            ExprAST::dump(out << "if", ind);
            Cond->dump(indent(out, ind) << "Cond:", ind + 1);
            Then->dump(indent(out, ind) << "Then:", ind + 1);
            Else->dump(indent(out, ind) << "Else:", ind + 1);
            return out;
        }
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

        llvm::raw_ostream &
        dump(llvm::raw_ostream &out, int32 ind) override
        {
            ExprAST::dump(out << "for", ind);
            Start->dump(indent(out, ind) << "Cond:", ind + 1);
            End->dump(indent(out, ind) << "End:", ind + 1);
            Step->dump(indent(out, ind) << "Step:", ind + 1);
            Body->dump(indent(out, ind) << "Body:", ind + 1);
            return out;
        }
};

/// PrototypeAST - This class represents the "prototype" for a function,
/// which captures its name and its argument names (thus, the number of args too)
/// as well as if it is an operator.
class PrototypeAST
{
    std::string Name;
    std::vector<std::string> Args;

    bool32 IsOperator;
    unsigned Precedence; // Precedence if a binop
    int32 Line;

    public:
        PrototypeAST(SourceLocation Loc, const std::string &Name, std::vector<std::string> Args,
                bool32 IsOperator = false, unsigned Prec = 0)
            : Name(Name), Args(std::move(Args)), IsOperator(IsOperator), Precedence(Prec), Line(Loc.Line) {}

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
        int32 getLine() const { return Line; }
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

        llvm::raw_ostream &
        dump(llvm::raw_ostream &out, int32 ind)
        {
            indent(out, ind) << "FunctionAST\n";
            ++ind;
            indent(out, ind) << "Body:";
            return Body ? Body->dump(out, ind) : out << "null\n";
        }
};


