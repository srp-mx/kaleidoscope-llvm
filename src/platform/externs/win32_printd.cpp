#pragma once

#include "../typedefs/typedefs.hpp"

/// putchard - putchar that takes a double and returns 0.
extern "C" __declspec(dllexport) real64 
printd(real64 X)
{
    fprintf(stderr, "%f\n", X);
    return 0;
}


