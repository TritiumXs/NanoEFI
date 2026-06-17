/*
 * examples/sensor_logger/MainLinux.c — Linux Launcher
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 NanoEFI Contributors
 */

#include <stdio.h>

#include "../../backends/linux/NefLinux.h"

/* Driver entry — defined in SensorLogger.c */
extern EfiStatus SensorLoggerEntry(NanoEfiServiceTable* Svc, VOID* Arg);

int main(void) {
  NanoEfiServiceTable Svc;
  if (EFI_ERROR(NanoEfiLinuxInit(&Svc))) {
    fprintf(stderr, "NanoEfiLinuxInit failed\n");

    return 1;
  }
  Svc.Log("[NanoEFI] v%u.%u — linux backend\n",
          (NANO_EFI_VERSION_CURRENT >> 16) & 0xFFFFu,
          NANO_EFI_VERSION_CURRENT & 0xFFFFu);
  EfiStatus s = SensorLoggerEntry(&Svc, NULL);

  return EFI_ERROR(s) ? 1 : 0;
}
