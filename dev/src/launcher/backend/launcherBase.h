#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <emmintrin.h>
#include <intrin.h>
#include <float.h>

#include <Windows.h>

#ifdef LOADER_PE_EXPORTS
	#define LAUNCHER_API __declspec(dllexport)
#else
	#define LAUNCHER_API __declspec(dllimport)
#endif

#pragma warning( disable:4251 ) // dll interface

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned __int64 uint64;

typedef char int8;
typedef short int16;
typedef int int32;
typedef __int64 int64;
