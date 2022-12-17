// NOTE(srp): THIS IS NOT A FINAL PLATFORM LAYER
// We're first making it work and then making it nice.

// TODO(srp): Move platform independent stuff

#include <stdint.h>
#include <string>
#include <cctype>
#include <memory>
#include <vector>
#include <map>

#define local_persist static
#define global_variable static
#define internal static
#define inline_variable constexpr

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef int32 bool32;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef float real32;
typedef double real64;

typedef std::string str;

#include "kaleidoscope.cpp"


// Tokens [0-255] if it's an unknown character, otherwise one of 
// the following for known things
enum Token {
    tok_eof = -1,

    // commands
    tok_def = -2,
    tok_extern = -3,

    // primary
    tok_identifier = -4,
    tok_number = -5,
};

global_variable str IdentifierStr; // Filled in if tok_identifier
global_variable real64 NumVal;     // Filled in if tok_number

// gettok - Return the next token from standard input
// Heart of the LEXER.
internal int32
gettok()
{
    local_persist int32 LastChar = ' ';

    // Skip whitespace
    while (isspace(LastChar)) 
    {
        LastChar = getchar();
    }

    // IDENTIFIERS
    if (isalpha(LastChar))
    {
        // IdentifierStr: [a-zA-Z][a-zA-Z0-9]*
        IdentifierStr = LastChar;
        while (isalnum((LastChar = getchar())))
        {
            IdentifierStr += LastChar;
        }

        if (IdentifierStr == "def")
        {
            return tok_def;
        }

        if (IdentifierStr == "extern")
        {
            return tok_extern;
        }

        return tok_identifier;
    }

    // NUMBERS
    if (isdigit(LastChar) || LastChar == '.')
    {
        // NumStr: [0-9.]+
        str NumStr;
        do
        {
            NumStr += LastChar;
            LastChar = getchar();
        } while (isdigit(LastChar) || LastChar == '.');

        NumVal = strtod(NumStr.c_str(), 0);
        return tok_number;
        
        // TODO(srp): This will incorrectly read "1.23.45.67" as "1.23"
    }

    // COMMENTS
    if (LastChar == '#')
    {
        // Comment until end of line
        do
        {
            LastChar = getchar();
        } while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

        if (LastChar != EOF)
        {
            return gettok();
        }
    }

    // Check for end of file. Don't eat the EOF.
    if (LastChar == EOF)
    {
        return tok_eof;
    }

    // Otherwise, just return the character as its ascii value
    int32 ThisChar = LastChar;
    LastChar = getchar();
    return ThisChar;
}

// ABSTRACT SYNTAX TREE (AST)

/// ExprAST - Base class for all expression nodes.
class ExprAST
{
    // TODO(srp): Add a type field
    public:
        virtual ~ExprAST() {}
};

/// NumberExprAST - Expression class for numeric literals like "1.0"
class NumberExprAST : public ExprAST
{
    real64 Val;

    public:
        NumberExprAST(double Val) : Val(Val) {}
};

/// VariableExprAST - Expression class for referencing a variable, like "a".
class VariableExprAST : public ExprAST
{
    str Name;

    public:
        VariableExprAST(const str &Name) : Name(Name) {}
};

/// BinaryExprAST - Expression class for a binary operator.
class BinaryExprAST : public ExprAST
{
    char Op;
    std::unique_ptr<ExprAST> LHS, RHS;

    public:
        BinaryExprAST(char op, std::unique_ptr<ExprAST> LHS, std::unique_ptr<ExprAST> RHS)
            : Op(op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
};

/// CallExprAST - Expression class for function calls.
class CallExprAST : public ExprAST
{
    str Callee;
    std::vector<std::unique_ptr<ExprAST>> Args;

    public:
        CallExprAST(const str & Callee, std::vector<std::unique_ptr<ExprAST>> Args)
            : Callee(Callee), Args(std::move(Args)) {}
};

/// PrototypeAST - This class represents the "prototype" for a function,
/// which captures its name and its argument names (thus, the number of args too)
class PrototypeAST
{
    str Name;
    std::vector<str> Args;

    public:
        PrototypeAST(const str &name, std::vector<str> Args)
            : Name(name), Args(std::move(Args)) {}

        const str &getName() const { return Name; }
};

/// FunctionAST - This class represents a function definition itself.
class FunctionAST
{
    std::unique_ptr<PrototypeAST> Proto;
    std::unique_ptr<ExprAST> Body;

    public:
        FunctionAST(std::unique_ptr<PrototypeAST> Proto, std::unique_ptr<ExprAST> Body)
            : Proto(std::move(Proto)), Body(std::move(Body)) {}
};

// PARSER ERROR HANDLING

/// CurTok/getNextToken - Provide a simple token buffer. CurTok is the current
/// token the parser is looking at. genNextToken reads another token from the
/// lexer and updates CurTok with its results.
global_variable int32 CurTok;

internal int32
getNextToken()
{
    return CurTok = gettok();
}

/// LogError* - These are little helper functions for error handling.
std::unique_ptr<ExprAST>
LogError(const char *Str)
{
    fprintf(stderr, "LogError: %s\n", Str);
    return nullptr;
}

std::unique_ptr<PrototypeAST>
LogErrorP(const char *Str)
{
    LogError(Str);
    return nullptr;
}

// PARSER (builds the AST)

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

    getNextToken(); // ear ')'
    return V;
}

/// identifierexpr
///     ::= identifier
///     ::= identifier '(' expression* ')'
internal std::unique_ptr<ExprAST>
ParseIdentifierExpr()
{
    /// To be called when the current token is a tok_identifier token.
    str IdName = IdentifierStr;

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
        while(1)
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

/// primary
///     ::= identifierexpr
///     ::= numberexpr
///     ::= parenexpr
/// Works as entry point for "primary" expressions
internal std::unique_ptr<ExprAST>
ParsePrimary()
{
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
    }
}

// NOTE(srp): Operator-Precedence Parsing here

/// BinopPrecedence - This holds precedence for each binary operator that is defined.
global_variable std::map<char, int32> BinopPrecedence;

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
    while (1)
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

    str FnName = IdentifierStr;
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
        auto Proto = std::make_unique<PrototypeAST>("", std::vector<std::string>());
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

// NOTE(srp): Top-level parsing here

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

int main()
{
    // Install standard binary operators.
    InstallStandardBinaryOperators();

    // Prime the first token
    fprintf(stderr, "ready> ");
    getNextToken();

    // Run the main "interpreter loop" now
    MainLoop();

    return 0;
}










