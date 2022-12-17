// NOTE(srp): THIS IS NOT A FINAL PLATFORM LAYER
// We're first making it work and then making it nice.

// TODO(srp): Move platform independent stuff

#include <stdint.h>
#include <string>
#include <cctype>
#include <memory>
#include <vector>

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

// ABSTRACT SYNTAX TREE

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




