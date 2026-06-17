/*
 * lib/nef_format.c — NanoEFI Built-in Format Library
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 NanoEFI Contributors
 *
 * Freestanding: no libc.  Supports: %d %i %u %x %X %s %c %p %%.
 * Width and zero-padding modifiers supported.  No floating point.
 */

#include "NefFormat.h"

#include "NefString.h"

static NEF_SIZE PutChar(char* Buf, NEF_SIZE Pos, NEF_SIZE Size, char C) {
  if (Pos + 1u < Size)
    Buf[Pos] = C;

  return 1u;
}

static NEF_SIZE PutUint(char* Buf, NEF_SIZE Pos, NEF_SIZE Size, UINT64 Val,
                        UINT32 Base, int UpperCase, int Width, int ZeroPad) {
  char Tmp[20];
  NEF_SIZE Len = 0u;
  const char* Digits = UpperCase ? "0123456789ABCDEF" : "0123456789abcdef";

  if (Val == 0u) {
    Tmp[Len++] = '0';
  } else {
    while (Val) {
      Tmp[Len++] = Digits[Val % (UINT64)Base];
      Val /= (UINT64)Base;
    }
  }

  char Pad = ZeroPad ? '0' : ' ';
  NEF_SIZE Total = 0u;
  while ((int)Len < Width) {
    Total += PutChar(Buf, Pos + Total, Size, Pad);
    Width--;
  }

  while (Len--) {
    Total += PutChar(Buf, Pos + Total, Size, Tmp[Len]);
  }

  return Total;
}

INT32 NefVsnprintf(char* Buf, NEF_SIZE Size, const char* Fmt, NefVaList Ap) {
  NEF_SIZE Pos = 0u;

  if (!Buf || Size == 0u)
    return 0;

  while (*Fmt) {
    if (*Fmt != '%') {
      Pos += PutChar(Buf, Pos, Size, *Fmt++);
      continue;
    }
    Fmt++;

    int ZeroPad = 0;
    if (*Fmt == '0') {
      ZeroPad = 1;
      Fmt++;
    }

    int Width = 0;
    while (*Fmt >= '0' && *Fmt <= '9') {
      Width = Width * 10 + (*Fmt++ - '0');
    }

    switch (*Fmt++) {
      case 'd':
      case 'i': {
        INT32 V = NEF_VA_ARG(Ap, INT32);
        if (V < 0) {
          Pos += PutChar(Buf, Pos, Size, '-');
          V = -V;
          if (Width > 0)
            Width--;
        }
        Pos +=
            PutUint(Buf, Pos, Size, (UINT64)(UINT32)V, 10, 0, Width, ZeroPad);
        break;
      }
      case 'u':
        Pos += PutUint(Buf, Pos, Size, (UINT64)NEF_VA_ARG(Ap, UINT32), 10, 0,
                       Width, ZeroPad);
        break;
      case 'x':
        Pos += PutUint(Buf, Pos, Size, (UINT64)NEF_VA_ARG(Ap, UINT32), 16, 0,
                       Width, ZeroPad);
        break;
      case 'X':
        Pos += PutUint(Buf, Pos, Size, (UINT64)NEF_VA_ARG(Ap, UINT32), 16, 1,
                       Width, ZeroPad);
        break;
      case 'p':
        Pos += PutChar(Buf, Pos, Size, '0');
        Pos += PutChar(Buf, Pos, Size, 'x');
        Pos += PutUint(Buf, Pos, Size, (UINT64)(UINTN)NEF_VA_ARG(Ap, VOID*), 16,
                       0, (int)(sizeof(VOID*) * 2u), 1);
        break;
      case 's': {
        const char* S = NEF_VA_ARG(Ap, const char*);
        if (!S)
          S = "(null)";
        int Slen = (int)NefStrlen(S);
        while (Slen < Width) {
          Pos += PutChar(Buf, Pos, Size, ' ');
          Width--;
        }
        while (*S)
          Pos += PutChar(Buf, Pos, Size, *S++);
        break;
      }
      case 'c':
        Pos += PutChar(Buf, Pos, Size, (char)NEF_VA_ARG(Ap, INT32));
        break;
      case '%':
        Pos += PutChar(Buf, Pos, Size, '%');
        break;
      default:
        Pos += PutChar(Buf, Pos, Size, '?');
        break;
    }
  }

  Buf[Pos < Size ? Pos : Size - 1u] = '\0';

  return (INT32)Pos;
}

INT32 NefSnprintf(char* Buf, NEF_SIZE Size, const char* Fmt, ...) {
  NefVaList Ap;
  NEF_VA_START(Ap, Fmt);
  INT32 R = NefVsnprintf(Buf, Size, Fmt, Ap);
  NEF_VA_END(Ap);

  return R;
}
