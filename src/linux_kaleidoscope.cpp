// NOTE(srp): THIS IS NOT A FINAL PLATFORM LAYER
// We're first making it work and then making it nice.

// TODO(srp): Move platform independent stuff

#include <string>
#include <cctype>
#include <memory>
#include <vector>
#include <map>

#define PLATFORM_LINUX 1

#include "kaleidoscope.cpp"

int main()
{
    // Install standard binary operators.
    InstallStandardBinaryOperators();

    // Prime the first token
    fprintf(stderr, "ready> ");
    getNextToken();

    // Make the module, which holds all the code.
    InitializeLLVM();

    // Run the main "interpreter loop" now
    MainLoop();

    // Print out all of the generated code.
    TheModule->print(llvm::errs(), nullptr);

    return 0;
}










