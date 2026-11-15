// intrin.h (Linux compatibility layer): MSVC intrinsics used by the engine.
#pragma once
#include <windows.h>
#if !__has_builtin( __emulu )	// clang -fms-extensions provides __cpuid and __emulu as builtins
#include <cpuid.h>
#undef __cpuid	// GCC's 5-argument macro; MSVC's is a function

inline void __cpuid(int info[ 4 ], int function)
{
	unsigned a = 0, b = 0, c = 0, d = 0;
	__cpuid_count( function, 0, a, b, c, d );
	info[ 0 ] = (int)a; info[ 1 ] = (int)b; info[ 2 ] = (int)c; info[ 3 ] = (int)d;
}

inline unsigned __int64 __emulu(unsigned a, unsigned b) { return (unsigned __int64)a * b; }
#endif
