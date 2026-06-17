/*
 * examples/sensor_logger/MainWin32.c — Win32 Launcher
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 NanoEFI Contributors
 */

#include "../../backends/win32/NefWin32.h"

extern EfiStatus SensorLoggerEntry(NanoEfiServiceTable* Svc, VOID* Arg);

int main(void) {
  NanoEfiServiceTable Svc;
  if (EFI_ERROR(NanoEfiWin32Init(&Svc))) {
    fprintf(stderr, "NanoEfiWin32Init failed\n");

    return 1;
  }
  Svc.Log("[NanoEFI] v%u.%u — win32 backend\n",
          (NANO_EFI_VERSION_CURRENT >> 16) & 0xFFFFu,
          NANO_EFI_VERSION_CURRENT & 0xFFFFu);
  EfiStatus s = SensorLoggerEntry(&Svc, NULL);

  return EFI_ERROR(s) ? 1 : 0;
}
