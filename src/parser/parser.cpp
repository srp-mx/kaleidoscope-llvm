#pragma once
// NOTE(srp): Still not final platform-independent code

#include <map>
#include "../platform/typedefs/typedefs.hpp"

#include "../lexer/lexer.cpp"
#include "../logging/parser_err.cpp"

/// CurTok/getNextToken - Provide a simple token buffer. CurTok is the current
/// token the parser is looking at. genNextToken reads another token from the
/// lexer and updates CurTok with its results.
global_variable int32 CurTok;

internal int32
getNextToken()
{
    return CurTok = gettok();
}

/// BinopPrecedence - This holds precedence for each binary operator that is defined.
global_variable std::map<char, int32> BinopPrecedence;

// NOTE(srp): Recursive descent parsing here

internal std::unique_ptr<ExprAST> ParseExpression();

/// numberexpr ::= number
/// To be called when the current token is a tok_number token.
internal std::unique_ptr<ExprAST>
ParseNumberExpr()
{
    auto Result = std::make_unique<NumberExprAST>(NumVal);
    getNextToken(); // consume the number
    return std::move(Result);
}

/// parenexpr ::= '(' expression ')'
internal std::unique_ptr<ExprAST>
ParseParenExpr()
{
    // NOTE(srp): Parenthesis do not create AST nodes, they just guide the
    // parser to provide grouping. Once the AST is built, parenthesis are
    // not needed.

    getNextToken(); // eat '('
    auto V = ParseExpression(); // Here's some recursive descent parsing
    if (!V)
    {
        return nullptr;
    }

    if (CurTok != ')')
    {
        return LogError("expected ')'");
    }

    getNextToken(); // eat ')'
    return V;
}

/// identifierexpr
///     ::= identifier
///     ::= identifier '(' expression* ')'
internal std::unique_ptr<ExprAST>
ParseIdentifierExpr()
{
    /// To be called when the current token is a tok_identifier token.
    std::string IdName = IdentifierStr;

    getNextToken(); // eat identifier.

    if (CurTok != '(') // Simple variable ref.
    {
        return std::make_unique<VariableExprAST>(IdName);
    } // else: function call expression

    // Call.
    getNextToken(); // eat '('
    std::vector<std::unique_ptr<ExprAST>> Args;
    if (CurTok != ')')
    {
        while(true)
        {
            if (auto Arg = ParseExpression())
            {
                Args.push_back(std::move(Arg));
            }
            else
            {
                return nullptr;
            }

            if (CurTok == ')')
            {
                break;
            }

            if (CurTok != ',')
            {
                return LogError("Expected ')' or ',' in argument list");
            }

            getNextToken();
        }
    }

    // Eat the ')'
    getNextToken();

    return std::make_unique<CallExprAST>(IdName, std::move(Args));
}

/// ifexpr ::= 'if' expression 'then' expression 'else' expression
internal std::unique_ptr<ExprAST>
ParseIfExpr()
{
    getNextToken(); // eat the 'if'

    // condition
    auto Cond = ParseExpression();
    if (!Cond)
    {
        return nullptr;
    }

    if (CurTok != tok_then)
    {
        return LogError("expected 'then'");
    }

    getNextToken(); // eat 'then'

    auto Then = ParseExpression();

    if (!Then)
    {
        return nullptr;
    }

    if (CurTok != tok_else)
    {
        return LogError("expected 'else'");
    }

    getNextToken();

    auto Else = ParseExpression();
    if (!Else)
    {
        return nullptr;
    }

    return std::make_unique<IfExprAST>(std::move(Cond), std::move(Then), std::move(Else));
}

/// forexpr ::= 'for' identifier '=' expr ',' expr (',' expr)? 'in' expression
internal std::unique_ptr<ExprAST>
ParseForExpr()
{
    getNextToken(); // eat the 'for'

    if (CurTok != tok_identifier)
    {
        return LogError("expected identifier after 'for'");
    }

    std::string IdName = IdentifierStr;
    getNextToken(); // eat identifier

    if (CurTok != '=')
    {
        return LogError("expected '=' after 'for'");
    }

    getNextToken(); // eat '='

    auto Start = ParseExpression();
    if (!Start)
    {
        return nullptr;
    }

    if (CurTok != ',')
    {
        return LogError("expected ',' after for start value");
    }

    getNextToken(); // eat ','

    auto End = ParseExpression();
    if (!End)
    {
        return nullptr;
    }

    // The step value is optional
    std::unique_ptr<ExprAST> Step;
    if (CurTok == ',')
    {
        getNextToken(); // eat ','
        Step = ParseExpression();
        if (!Step)
        {
            return nullptr;
        }
    }

    if (CurTok != tok_in)
    {
        return LogError("expected 'in' after 'for'");
    }
    
    getNextToken(); // eat 'in'

    auto Body = ParseExpression();
    if (!Body)
    {
        return nullptr;
    }

    return std::make_unique<ForExprAST>(IdName, std::move(Start), std::move(End), std::move(Step), std::move(Body));
}

/// primary
///     ::= identifierexpr
///     ::= numberexpr
///     ::= parenexpr
///     ::= ifexpr
/// Works as entry point for "primary" expressions
internal std::unique_ptr<ExprAST>
ParsePrimary()
{
    // TODO(srp): Parse unary -

    // That's why in the following functions we can assume CurTok's state
    // when they're called.
    switch (CurTok)
    {
        default:
            return LogError("unknown token when expecting an expression");
        case tok_identifier:
            return ParseIdentifierExpr();
        case tok_number:
            return ParseNumberExpr();
        case '(':
            return ParseParenExpr();
        case tok_if:
            return ParseIfExpr();
        case tok_for:
            return ParseForExpr();
    }
}

// NOTE(srp): Operator-Precedence Parsing here

/// GetTokPrecedence - Get the precedence of the pending binary operator token.
internal int32
GetTokPrecedence()
{
    if (!isascii(CurTok))
    {
        return -1;
    }

    // Make sure it's a declared binop
    int32 TokPrec = BinopPrecedence[CurTok];
    if (TokPrec <= 0)
    {
        return -1;
    }
    return TokPrec;
}

/// binoprhs
///     ::= ('+' primary)*
internal std::unique_ptr<ExprAST>
ParseBinOpRHS(int32 ExprPrec, std::unique_ptr<ExprAST> LHS)
{
    // If this is a binop, find its precedence
    while (true)
    {
        int32 TokPrec = GetTokPrecedence();

        // If this is a binop that binds at least as tightly as the current binop,
        // conume it, otherwise we are done.
        if (TokPrec < ExprPrec)
        {
            return LHS;
        }

        // Okay, we know this is a binop.
        int32 BinOp = CurTok;
        getNextToken(); // eat binop

        // Parse the primary expression after the binary operator
        auto RHS = ParsePrimary();
        if (!RHS)
        {
            return nullptr;
        }

        // If BinOp binds less tightly with RHS than the operator after RHS, let
        // the pending operator take RHS as its LHS.
        int32 NextPrec = GetTokPrecedence();
        if (TokPrec < NextPrec)
        {
            // That way we'll join higher precedence RHS'es and 'collapse' them
            // when we find a lower precedence one.
            // e.g: a+b*c+d -> (+: (+: a, (*: b, c)), d)
            RHS = ParseBinOpRHS(TokPrec + 1, std::move(RHS));
            if (!RHS)
            {
                return nullptr;
            }
        }

        // Merge LHS/RGS.
        LHS = std::make_unique<BinaryExprAST>(BinOp, std::move(LHS), std::move(RHS));
    }
}

/// expression
///     ::= primary binoprhs
/// NOTE(srp): binoprhs is allowed to be empty
internal std::unique_ptr<ExprAST>
ParseExpression()
{
    auto LHS = ParsePrimary();
    if (!LHS)
    {
        return nullptr;
    }

    return ParseBinOpRHS(0, std::move(LHS));
}

// NOTE(srp): Less interesting parsing here

/// prototype
///     ::= id '(' id* ')'
internal std::unique_ptr<PrototypeAST>
ParsePrototype()
{
    if (CurTok != tok_identifier)
    {
        return LogErrorP("Expected function name in prototype");
    }

    std::string FnName = IdentifierStr;
    getNextToken();

    if (CurTok != '(')
    {
        return LogErrorP("Expected '(' in prototype");
    }

    // Read the list of argument names
    std::vector<std::string> ArgNames;
    while (getNextToken() == tok_identifier)
    {
        ArgNames.push_back(IdentifierStr);
    }
    if (CurTok != ')')
    {
        return LogErrorP("Expected ')' in prototype");
    }

    // success
    getNextToken(); // eat ')'

    return std::make_unique<PrototypeAST>(FnName, std::move(ArgNames));
}

/// definition ::= 'def' prototype expression
internal std::unique_ptr<FunctionAST>
ParseDefinition()
{
    getNextToken(); // eat 'def'
    auto Proto = ParsePrototype();
    if (!Proto)
    {
        return nullptr;
    }
    
    if (auto E = ParseExpression())
    {
        return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
    }

    return nullptr;
}

/// external ::= 'extern' prototype
internal std::unique_ptr<PrototypeAST>
ParseExtern()
{
    getNextToken(); // eat 'extern'.
    return ParsePrototype();
}

/// toplevelexpr ::= expression
/// Anonymous nullary functions to allow arbitrary top-level expressions
internal std::unique_ptr<FunctionAST>
ParseTopLevelExpr()
{
    if (auto E = ParseExpression())
    {
        // Make an anonymous proto.
        auto Proto = std::make_unique<PrototypeAST>("{__anon_expr}", std::vector<std::string>());
        return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
    }
    return nullptr;
}

inline void
InstallStandardBinaryOperators()
{
    // 1 is lowest precedence
    BinopPrecedence['<'] = 10;
    BinopPrecedence['+'] = 20;
    BinopPrecedence['-'] = 20;
    BinopPrecedence['*'] = 40; // highest
}



