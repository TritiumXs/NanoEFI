/*
 * lib/nef_string.c — NanoEFI Built-in String Library
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 NanoEFI Contributors
 *
 * Freestanding: no libc.
 */

#include "NefString.h"

VOID* NefMemcpy(VOID* Dst, const VOID* Src, NEF_SIZE N) {
  UINT8* d = (UINT8*)Dst;
  const UINT8* s = (const UINT8*)Src;
  while (N--)
    *d++ = *s++;

  return Dst;
}

VOID* NefMemset(VOID* Dst, INT32 C, NEF_SIZE N) {
  UINT8* d = (UINT8*)Dst;
  while (N--)
    *d++ = (UINT8)C;

  return Dst;
}

INT32 NefMemcmp(const VOID* A, const VOID* B, NEF_SIZE N) {
  const UINT8* a = (const UINT8*)A;
  const UINT8* b = (const UINT8*)B;
  while (N--) {
    if (*a != *b)
      return (INT32)*a - (INT32)*b;
    a++;
    b++;
  }

  return 0;
}

NEF_SIZE NefStrlen(const char* S) {
  const char* p = S;
  while (*p)
    p++;

  return (NEF_SIZE)(p - S);
}

INT32 NefStrcmp(const char* A, const char* B) {
  while (*A && *A == *B) {
    A++;
    B++;
  }

  return (INT32)(UINT8)*A - (INT32)(UINT8)*B;
}

char* NefStrcpy(char* Dst, const char* Src) {
  char* d = Dst;
  while ((*d++ = *Src++))
    ;

  return Dst;
}
