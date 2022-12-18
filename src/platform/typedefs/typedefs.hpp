#pragma once

#define local_persist static
#define global_variable static
#define internal static
#define inline_variable constexpr

#ifdef PLATFORM_LINUX
    #include "linux_typedefs.hpp"
#else
    #include "linux_typedefs.hpp" // Default to Linux typedefs
#endif
