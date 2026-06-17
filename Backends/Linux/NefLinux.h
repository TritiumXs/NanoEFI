/*
 * backends/linux/NefLinux.h — NanoEFI Linux Backend
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 NanoEFI Contributors
 */

#ifndef NEF_LINUX_H
#define NEF_LINUX_H

#include "NefServices.h"

/*
 * NanoEfiLinuxInit — Fill a NanoEfiServiceTable with the Linux implementation.
 *
 * Call once before launching any driver.  Mock GPIO prints pin state to stdout.
 * Logging uses vprintf; no rate limiting.
 */
EfiStatus NanoEfiLinuxInit(NanoEfiServiceTable* Svc);

#endif /* NEF_LINUX_H */
