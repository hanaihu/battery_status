#pragma once
// Single place for the Windows version target and the common Win32 includes.
// Keeping this in one header means the build files never need to define it.

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
