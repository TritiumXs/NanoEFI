/*
 * examples/nano_cli/main.c — nano_cli host (platform-agnostic)
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 NanoEFI Contributors
 *
 * No platform headers here. cmake/NanoEFI.cmake selects the HAL at
 * link time; NefBackend.h is the only backend surface exposed.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "NefBackend.h"
#include "NefCliExt.h"
#include "NefLoader.h"

static NanoEfiServiceTable GSvc;

static EfiStatus ExecNef(const char* Path, void* Arg) {
  FILE* F;
  long Sz;
  void* Img;
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

  Img = NefAllocExec((NEF_SIZE)Sz);
  if (!Img) {
    fclose(F);

    return NEF_ENOMEM;
  }
  fread(Img, 1, (size_t)Sz, F);
  fclose(F);

  S = NefLoad(Img, (NEF_SIZE)Sz, &GSvc, Arg, malloc, free, NanoEfiSecurityOpen);
  NefFreeExec(Img, (NEF_SIZE)Sz);

  return S;
}

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

  if (EFI_ERROR(NanoEfiBackendInit(&GSvc))) {
    fprintf(stderr, "NanoEfiBackendInit failed\n");

    return 1;
  }
  GSvc.Extension = (void*)&CliExt;

  return EFI_ERROR(ExecNef(Loader, NULL)) ? 1 : 0;
}
