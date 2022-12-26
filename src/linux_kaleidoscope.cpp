// NOTE(srp): THIS IS NOT A FINAL PLATFORM LAYER
// We're first making it work and then making it nice.

// TODO(srp): Move platform independent stuff

#include <string>
#include <cctype>
#include <memory>
#include <vector>
#include <map>

#define PLATFORM_LINUX 1
#define MESSED_UP_FUNCTION_H 1

#include "kaleidoscope.cpp"

#include "platform/externs/linux_extern_table.hpp"

int main()
{
    // Initialize the compile target
    InitializeTarget();

    // Install standard binary operators.
    InstallStandardBinaryOperators();

    // Prime the first token
    getNextToken();

    // Make the module, which holds all the code.
    InitializeLLVM();

    // Run the main "interpreter loop" now
    MainLoop();

    FinalizeLLVM();

    return 0;
}










