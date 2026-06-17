/*
 * examples/nano_cli/loader/Loader.c — boot loader driver (loader.nef)
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 NanoEFI Contributors
 *
 * Reads nanoefi.cfg, launches boot= target; falls back to shell.nef on error.
 */

#include "NefCliExt.h"
#include "NefServices.h"
#include "NefString.h"

#define CFG_PATH "nanoefi.cfg"
#define SHELL_PATH "shell.nef"
#define CFG_MAX 512U

/* Find the value after "boot=" on any non-comment line; else NULL */
static const char* FindBoot(const char* Cfg) {
  const char* P = Cfg;

  while (*P) {
    if (*P == '#') {
      while (*P && *P != '\n')
        P++;
      continue;
    }
    if (NefMemcmp(P, "boot=", 5) == 0)
      return P + 5;
    while (*P && *P != '\n')
      P++;
  }

  return NULL;
}

__attribute__((section(".text.entry"))) EfiStatus NEF_CALLCONV
NefDriverEntry(NanoEfiSvc* Svc, VOID* Arg) {
  NanoEfiCliExt* Io = (NanoEfiCliExt*)Svc->Extension;
  char Cfg[CFG_MAX];
  char Path[256];
  const char* Val;
  UINT32 I;
  EfiStatus S;

  if (!Io)
    return NEF_ENOSYS;

  S = Io->ReadFile(CFG_PATH, Cfg, CFG_MAX, NULL);
  if (EFI_ERROR(S)) {
    Svc->Log("loader: %s not found, using shell\n", CFG_PATH);
    goto fallback;
  }

  Val = FindBoot(Cfg);
  if (!Val) {
    Svc->Log("loader: no boot= in %s, using shell\n", CFG_PATH);
    goto fallback;
  }

  for (I = 0;
       I < sizeof(Path) - 1u && Val[I] && Val[I] != '\n' && Val[I] != '\r'; I++)
    Path[I] = Val[I];
  Path[I] = '\0';

  Svc->Log("loader: booting %s\n", Path);
  S = Io->ExecNef(Path, Arg);
  if (!EFI_ERROR(S))
    return S;

  Svc->Log("loader: boot failed (0x%x), using shell\n", (unsigned)S);
fallback:
  return Io->ExecNef(SHELL_PATH, Arg);
}
