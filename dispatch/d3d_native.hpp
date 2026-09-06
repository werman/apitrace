/*
 * Copyright © 2026 Valve Corporation.
 * SPDX-License-Identifier: MIT
 */

#pragma once


#if defined(APITRACE_DXVK_NATIVE)

// DXVK 2.7.1's native headers omit the Win32 path-length constant.
#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <dlfcn.h>

#include <windows.h>


#ifndef _snprintf
#define _snprintf snprintf
#endif

#ifndef MAKEINTRESOURCEA
#define MAKEINTRESOURCEA(i) ((LPCSTR)(uintptr_t)((WORD)(i)))
#endif

typedef void (*PROC)(void);

static inline void
Sleep(DWORD dwMilliseconds)
{
    usleep(dwMilliseconds * 1000);
}

static inline HMODULE
LoadLibraryA(const char *lpLibFileName)
{
    if (!lpLibFileName) {
        return NULL;
    }

    return reinterpret_cast<HMODULE>(dlopen(lpLibFileName, RTLD_NOW | RTLD_GLOBAL));
}

static inline HMODULE
GetModuleHandleA(const char *)
{
    return NULL;
}

static inline PROC
GetProcAddress(HMODULE hModule, LPCSTR lpProcName)
{
    if (reinterpret_cast<uintptr_t>(lpProcName) <= 0xffff) {
        return NULL;
    }

    if (hModule) {
        return reinterpret_cast<PROC>(dlsym(reinterpret_cast<void *>(hModule), lpProcName));
    }

    return reinterpret_cast<PROC>(dlsym(RTLD_DEFAULT, lpProcName));
}

static inline UINT
GetSystemDirectoryA(char *, UINT)
{
    return 0;
}

#endif /* APITRACE_DXVK_NATIVE */
