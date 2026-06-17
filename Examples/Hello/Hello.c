/*
 * examples/hello/Hello.c — Minimal NanoEFI driver image
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 NanoEFI Contributors
 *
 * Platform-agnostic: only NefServices.h.  Compiled with -fPIC -nostdlib.
 * The .text.entry section is placed first by driver.ld so EntryOffset == 28.
 */

#include "NefServices.h"

__attribute__((section(".text.entry"))) NEF_CALLCONV EfiStatus
HelloEntry(NanoEfiServiceTable* Svc, VOID* Arg) {
  (VOID) Arg;
  Svc->Log("Hello from NanoEFI dynamic loader!\n");

  return NEF_OK;
}
