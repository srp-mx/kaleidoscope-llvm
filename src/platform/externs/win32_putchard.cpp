#pragma once

#include "../typedefs/typedefs.hpp"

/// putchard - putchar that takes a double and returns 0.
extern "C" __declspec(dllexport) real64 
putchard(real64 X)
{
    fputc((char)X, stderr);
    return 0;
}

