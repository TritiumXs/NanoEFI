/*
 * nef_format.h — NanoEFI Built-in Format Library
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 NanoEFI Contributors
 *
 * FREESTANDING: no libc.  va_list uses compiler builtins (GCC/Clang).
 *
 * Supported format specifiers:
 *   %d  %i   — signed decimal
 *   %u       — unsigned decimal
 *   %x  %X   — unsigned hex (lower/upper)
 *   %s       — null-terminated string
 *   %c       — character
 *   %p       — pointer (0x-prefixed hex)
 *   %%       — literal '%'
 *
 * Width and zero-padding modifiers supported (e.g. %08x, %-10s).
 * Floating-point specifiers (%f %e %g) are NOT implemented.
 */

#ifndef NEF_FORMAT_H
#define NEF_FORMAT_H

#include "NefErrno.h"

/*──────────────────────────────────────────────────────────────────────
 * Freestanding va_list  (GCC / Clang builtins; no <stdarg.h>)
 *──────────────────────────────────────────────────────────────────────*/

#if defined(__GNUC__) || defined(__clang__)
typedef __builtin_va_list NefVaList;
#define NEF_VA_START(ap, last) __builtin_va_start(ap, last)
#define NEF_VA_END(ap) __builtin_va_end(ap)
#define NEF_VA_ARG(ap, type) __builtin_va_arg(ap, type)
#else
/* Fallback: include <stdarg.h> for other compilers */
#include <stdarg.h>
typedef va_list NefVaList;
#define NEF_VA_START va_start
#define NEF_VA_END va_end
#define NEF_VA_ARG va_arg
#endif

/*──────────────────────────────────────────────────────────────────────
 * API
 *──────────────────────────────────────────────────────────────────────*/

INT32 NefVsnprintf(char* Buf, NEF_SIZE Size, const char* Fmt, NefVaList Ap);
INT32 NefSnprintf(char* Buf, NEF_SIZE Size, const char* Fmt, ...);

#endif /* NEF_FORMAT_H */
