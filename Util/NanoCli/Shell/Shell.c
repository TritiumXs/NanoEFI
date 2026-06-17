/*
 * examples/nano_cli/shell/Shell.c — fallback interactive shell (shell.nef)
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 NanoEFI Contributors
 *
 * Commands: load <path>  — execute a .nef driver
 *           help         — list commands
 *           exit         — return NEF_OK
 */

#include "NefCliExt.h"
#include "NefServices.h"
#include "NefString.h"

#define LINE_MAX 256U

static void CmdHelp(NanoEfiSvc* Svc) {
  Svc->Log("  load <path>  load and run a .nef driver\n");
  Svc->Log("  help         show this help\n");
  Svc->Log("  exit         exit the shell\n");
}

__attribute__((section(".text.entry"))) EfiStatus NEF_CALLCONV
NefDriverEntry(NanoEfiSvc* Svc, VOID* Arg) {
  NanoEfiCliExt* Io = (NanoEfiCliExt*)Svc->Extension;
  char Line[LINE_MAX];
  EfiStatus S;

  (void)Arg;

  if (!Io)
    return NEF_ENOSYS;

  Svc->Log("NanoEFI shell  (type 'help' for commands)\n");

  for (;;) {
    Svc->Log("nef> ");
    S = Io->ReadLine(Line, LINE_MAX);
    if (EFI_ERROR(S))
      break;

    if (NefStrcmp(Line, "exit") == 0)
      break;

    if (NefStrcmp(Line, "help") == 0) {
      CmdHelp(Svc);
      continue;
    }

    if (NefMemcmp(Line, "load ", 5) == 0) {
      S = Io->ExecNef(Line + 5, NULL);
      if (EFI_ERROR(S))
        Svc->Log("load: failed (0x%x)\n", (unsigned)S);
      continue;
    }

    if (Line[0] != '\0')
      Svc->Log("unknown: %s\n", Line);
  }

  return NEF_OK;
}
