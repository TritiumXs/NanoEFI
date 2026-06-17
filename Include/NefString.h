/*
 * nef_string.h — NanoEFI Built-in String Library
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 NanoEFI Contributors
 *
 * FREESTANDING: no libc.  All symbols use the nef_ prefix to avoid
 * collisions with platform libc when both are linked.
 *
 * These functions are for driver code only.  The NanoEFI core itself
 * calls none of them.
 */

#ifndef NEF_STRING_H
#define NEF_STRING_H

#include "NefErrno.h"

VOID* NefMemcpy(VOID* Dst, const VOID* Src, NEF_SIZE N);
VOID* NefMemset(VOID* Dst, INT32 C, NEF_SIZE N);
INT32 NefMemcmp(const VOID* A, const VOID* B, NEF_SIZE N);
NEF_SIZE NefStrlen(const char* S);
INT32 NefStrcmp(const char* A, const char* B);
char* NefStrcpy(char* Dst, const char* Src);

#endif /* NEF_STRING_H */
