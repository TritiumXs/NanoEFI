/*
 * include/nef_loader.h — NanoEFI Driver Image Loader
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 NanoEFI Contributors
 */

#ifndef NEF_LOADER_H
#define NEF_LOADER_H

#include "NefErrno.h"
#include "NefSecurity.h"
#include "NefServices.h"

/*
 * RAM allocation callbacks — caller provides platform-specific alloc/free.
 * On Linux: pass malloc / free.
 * On bare-metal: pass a static-pool allocator.
 */
typedef VOID* (*NefAllocFn)(NEF_SIZE Size);
typedef VOID (*NefFreeFn)(VOID* Ptr);

/*
 * NefLoad — Validate, load, and execute a NanoEFI driver image.
 *
 * Parameters:
 *   Image         — Pointer to the raw image bytes in memory.
 *   ImageBytes    — Number of bytes available at Image (used for bounds
 * checks). Svc           — Platform service table passed to the driver entry.
 *   Arg           — Opaque argument forwarded to the driver entry.
 *   Alloc/Free    — Platform allocator for .data/.bss RAM.
 *   SecurityLevel — NanoEfiSecurityOpen | Signed | Locked.
 *
 * Validation sequence (OPEN mode):
 *   1. Magic == "NEFI"
 *   2. NDIH Version compatibility
 *   3. ArchId matches current platform
 *   4. EntryOffset < ImageSize
 *   5. DataSize + BssSize within available RAM (ImageBytes used as proxy)
 *   6. MinSvcVersion <= Svc->Version
 *   7. CRC32 over image bytes 0..ImageSize-5 vs. last 4 bytes
 *
 * SIGNED mode additionally verifies Ed25519 signature.
 * LOCKED mode returns NEF_EPERM immediately without inspection.
 *
 * Returns: NEF_OK on success (driver has returned), or a NEF_E* error code.
 */
EfiStatus NefLoad(IN const VOID* Image, IN NEF_SIZE ImageBytes,
                  IN NanoEfiServiceTable* Svc, IN VOID* Arg OPTIONAL,
                  IN NefAllocFn Alloc, IN NefFreeFn Free,
                  IN NanoEfiSecurity SecurityLevel);

#endif /* NEF_LOADER_H */
