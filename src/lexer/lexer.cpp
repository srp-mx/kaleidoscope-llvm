#pragma once
// NOTE(srp): Still not final platform-independent code

#include <string>
#include "../platform/typedefs/typedefs.hpp"

global_variable std::string IdentifierStr; // Filled in if tok_identifier
global_variable real64 NumVal;             // Filled in if tok_number

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

    // control
    tok_if = -6,
    tok_then = -7,
    tok_else = -8,
};

// gettok - Return the next token from standard input
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

        if (IdentifierStr == "if")
        {
            return tok_if;
        }

        if (IdentifierStr == "then")
        {
            return tok_then;
        }

        if (IdentifierStr == "else")
        {
            return tok_else;
        }

        return tok_identifier;
    }

    // NUMBERS
    if (isdigit(LastChar) || LastChar == '.')
    {
        // NumStr: [0-9.]+
        std::string NumStr;
        do
        {
            NumStr += LastChar;
            LastChar = getchar();
        } while (isdigit(LastChar) || LastChar == '.');

        NumVal = strtod(NumStr.c_str(), nullptr);
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

