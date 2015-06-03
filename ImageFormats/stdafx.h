#ifndef _WIN32_WINNT		// Allow use of features specific to Windows XP or later.                   
#define _WIN32_WINNT 0x0501	// Change this to the appropriate value to target other versions of Windows.
#endif						

#ifdef _DDK_

#include <bzshlp/stdafx.h>
#else
#define _DO_NOT_DECLARE_INTERLOCKED_INTRINSICS_IN_MEMORY
#include <winsock2.h>
#include <tchar.h>
#include <intrin.h>
#endif