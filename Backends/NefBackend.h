/*
 * backends/NefBackend.h — NanoEFI Unified Backend
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 NanoEFI Contributors
 *
 * This is the only backend header samples and host programs should include.
 * Platform details live in hal/; cmake/NanoEFI.cmake selects the HAL impl.
 */

#ifndef NEF_BACKEND_H
#define NEF_BACKEND_H

#include "NefServices.h"

/*
 * NanoEfiBackendInit — initialise the HAL and fill *Svc.
 * Call once before loading any .nef driver.
 * Returns: NEF_OK | NEF_EINVAL | platform error
 */
EfiStatus NanoEfiBackendInit(NanoEfiServiceTable* Svc);

/*
 * NefAllocExec / NefFreeExec — allocate/free RWX memory for .nef images.
 * Host loader use only; not part of the service table.
 */
void* NefAllocExec(NEF_SIZE Bytes);
void NefFreeExec(void* P, NEF_SIZE Bytes);

#endif /* NEF_BACKEND_H */
