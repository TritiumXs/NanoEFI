/*
 * nef_arch.h — Architecture IDs, Capability IDs, GPIO Modes
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 NanoEFI Contributors
 *
 * FREESTANDING: no includes.
 */

#ifndef NEF_ARCH_H
#define NEF_ARCH_H

#include "NefErrno.h"

/* Architecture IDs (NanoEfiImageHeader.ArchId) */
#define NANO_EFI_ARCH_ARMV7M 0x01u  /* Cortex-M0/M3/M4/M7        */
#define NANO_EFI_ARCH_RISCV32 0x02u /* RV32I / RV32IMAC           */
#define NANO_EFI_ARCH_X86_64 0x10u  /* x86-64  (dev/test)         */
#define NANO_EFI_ARCH_AARCH64 0x11u /* AArch64 (dev/test)         */

#if defined(__x86_64__)
#define NANO_EFI_ARCH_CURRENT NANO_EFI_ARCH_X86_64
#elif defined(__aarch64__)
#define NANO_EFI_ARCH_CURRENT NANO_EFI_ARCH_AARCH64
#elif defined(__arm__) || defined(__thumb__)
#define NANO_EFI_ARCH_CURRENT NANO_EFI_ARCH_ARMV7M
#elif defined(__riscv) && (__riscv_xlen == 32)
#define NANO_EFI_ARCH_CURRENT NANO_EFI_ARCH_RISCV32
#else
#error "Unknown target — define NANO_EFI_ARCH_CURRENT manually"
#endif

/* Capability IDs for QueryCapability() */
#define NANO_EFI_CAP_FPU 0x0001u
#define NANO_EFI_CAP_MPU 0x0002u
#define NANO_EFI_CAP_DSP 0x0003u
#define NANO_EFI_CAP_TRUSTZONE 0x0004u
#define NANO_EFI_CAP_MALLOC 0x0005u
#define NANO_EFI_CAP_CACHE 0x0006u

/* GPIO pin modes for PinMode() */
#define NANO_EFI_PIN_INPUT 0x00u
#define NANO_EFI_PIN_OUTPUT 0x01u
#define NANO_EFI_PIN_INPUT_PU 0x02u /* pull-up   */
#define NANO_EFI_PIN_INPUT_PD 0x03u /* pull-down */

/* Timeout constants */
#define NANO_EFI_TIMEOUT_POLL ((UINT32)0u)
#define NANO_EFI_TIMEOUT_FOREVER ((UINT32)0xFFFFFFFFu)

#endif /* NEF_ARCH_H */
