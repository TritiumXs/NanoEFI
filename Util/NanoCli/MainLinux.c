/*
 * examples/nano_cli/MainLinux.c — nano_cli Linux host
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 NanoEFI Contributors
 *
 * Usage: nano_cli [loader.nef]   (default: loader.nef in cwd)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "NefCliExt.h"
#include "NefLinux.h"
#include "NefLoader.h"

static NanoEfiServiceTable GSvc;

static EfiStatus ReadFile(const char* Path, char* Buf, UINT32 Max,
                          UINT32* Got) {
  FILE* F;
  size_t N;

  if (!Path || !Buf || Max == 0)
    return NEF_EINVAL;

  F = fopen(Path, "rb");
  if (!F)
    return NEF_ENOENT;

  N = fread(Buf, 1, (size_t)(Max - 1u), F);
  fclose(F);
  Buf[N] = '\0';
  if (Got)
    *Got = (UINT32)N;

  return NEF_OK;
}

static EfiStatus ExecNef(const char* Path, void* Arg) {
  FILE* F;
  void* Img;
  long Sz;
  EfiStatus S;

  if (!Path)
    return NEF_EINVAL;

  F = fopen(Path, "rb");
  if (!F)
    return NEF_ENOENT;

  fseek(F, 0, SEEK_END);
  Sz = ftell(F);
  rewind(F);
  if (Sz <= 0) {
    fclose(F);

    return NEF_EINVAL;
  }

  Img = mmap(NULL, (size_t)Sz, PROT_READ | PROT_WRITE | PROT_EXEC,
             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (Img == MAP_FAILED) {
    fclose(F);

    return NEF_ENOMEM;
  }

  fread(Img, 1, (size_t)Sz, F);
  fclose(F);

  S = NefLoad(Img, (NEF_SIZE)Sz, &GSvc, Arg, malloc, free, NanoEfiSecurityOpen);
  munmap(Img, (size_t)Sz);

  return S;
}

static EfiStatus ReadLine(char* Buf, UINT32 Max) {
  size_t N;

  if (!Buf || Max == 0)
    return NEF_EINVAL;

  if (!fgets(Buf, (int)Max, stdin))
    return NEF_EOF;

  N = strlen(Buf);
  if (N > 0 && Buf[N - 1] == '\n')
    Buf[N - 1] = '\0';

  return NEF_OK;
}

int main(int argc, char** argv) {
  static const NanoEfiCliExt CliExt = {
      .ReadFile = ReadFile,
      .ExecNef = ExecNef,
      .ReadLine = ReadLine,
  };
  const char* Loader = (argc > 1) ? argv[1] : "loader.nef";
  EfiStatus S;

  S = NanoEfiLinuxInit(&GSvc);
  if (EFI_ERROR(S)) {
    fprintf(stderr, "NanoEfiLinuxInit failed\n");

    return 1;
  }
  GSvc.Extension = (void*)&CliExt;

  S = ExecNef(Loader, NULL);

  return EFI_ERROR(S) ? 1 : 0;
}
