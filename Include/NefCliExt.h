/*
 * include/NefCliExt.h — CLI I/O extension (attaches to Svc->Extension)
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 NanoEFI Contributors
 *
 * Hosted CLI backends populate this struct and assign it to Svc->Extension
 * before loading loader.nef.  NULL on bare-metal targets without a filesystem.
 */

#ifndef NEF_CLI_EXT_H
#define NEF_CLI_EXT_H

#include "NefErrno.h"

typedef struct {
  /*
   * ReadFile — read at most Max-1 bytes of Path into Buf (NUL-terminated).
   * *Got receives byte count on success if non-NULL.
   * Returns: NEF_OK | NEF_ENOENT | NEF_ENOMEM | NEF_EINVAL
   */
  EfiStatus(NEF_CALLCONV* ReadFile)(IN const char* Path, OUT char* Buf,
                                    IN UINT32 Max, OUT UINT32* Got OPTIONAL);

  /*
   * ExecNef — load and run a .nef driver from Path; blocks until entry returns.
   * Returns: NEF_OK | NEF_ENOENT | NEF_EINVAL | driver's own status
   */
  EfiStatus(NEF_CALLCONV* ExecNef)(IN const char* Path, IN VOID* Arg OPTIONAL);

  /*
   * ReadLine — read one console line into Buf (NUL-terminated, \n stripped).
   * Returns: NEF_OK | NEF_EOF | NEF_EINVAL
   */
  EfiStatus(NEF_CALLCONV* ReadLine)(OUT char* Buf, IN UINT32 Max);
} NanoEfiCliExt;

#endif /* NEF_CLI_EXT_H */
