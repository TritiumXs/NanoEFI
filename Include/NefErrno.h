/*
 * nef_errno.h — NanoEFI Primitive Types and Error Codes
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 NanoEFI Contributors
 *
 * FREESTANDING: this header includes nothing.  It is safe to use in
 * bare-metal environments with no libc.  All integer widths are defined
 * directly from GCC/Clang type aliases rather than <stdint.h>.
 */

#ifndef NEF_ERRNO_H
#define NEF_ERRNO_H

/*──────────────────────────────────────────────────────────────────────
 * Primitive Types  (no <stdint.h> required)
 *
 * GCC/Clang provide __INT8_TYPE__ etc. even in freestanding mode.
 * Fallback to traditional C type assumptions for other toolchains.
 *──────────────────────────────────────────────────────────────────────*/

#if defined(__UINT8_TYPE__)
typedef __UINT8_TYPE__ UINT8;
typedef __INT8_TYPE__ INT8;
typedef __UINT16_TYPE__ UINT16;
typedef __INT16_TYPE__ INT16;
typedef __UINT32_TYPE__ UINT32;
typedef __INT32_TYPE__ INT32;
typedef __UINT64_TYPE__ UINT64;
typedef __INT64_TYPE__ INT64;
#else
typedef unsigned char UINT8;
typedef signed char INT8;
typedef unsigned short UINT16;
typedef signed short INT16;
typedef unsigned int UINT32;
typedef signed int INT32;
typedef unsigned long long UINT64;
typedef signed long long INT64;
#endif

/* Pointer-width integer — matches sizeof(void*) on every target */
#if defined(__LP64__) || defined(_WIN64) || defined(__x86_64__) || \
    defined(__aarch64__)
typedef UINT64 UINTN;
typedef INT64 INTN;
#else
typedef UINT32 UINTN;
typedef INT32 INTN;
#endif

/* Size type (replaces size_t) */
typedef UINTN NEF_SIZE;

/* Common constants */
#ifndef VOID
#define VOID void
#endif

#ifndef NULL
#define NULL ((void*)0)
#endif

/* SAL-style parameter annotations (documentation only, no runtime effect) */
#ifndef IN
#define IN
#define OUT
#define OPTIONAL
#endif

/*──────────────────────────────────────────────────────────────────────
 * EFI_STATUS — high-bit error encoding
 *
 * Bit 63 (64-bit) or bit 31 (32-bit) is the error flag.
 * EFI_SUCCESS = 0.  All error codes compare < 0 as signed integers,
 * making EFI_ERROR() and negative-value checks equivalent.
 *──────────────────────────────────────────────────────────────────────*/

typedef UINTN EfiStatus;

#define EFI_ERROR_BIT \
  ((EfiStatus)((UINTN)1u << ((UINTN)sizeof(EfiStatus) * 8u - 1u)))
#define EFI_ERROR(s) (((EfiStatus)(s) & EFI_ERROR_BIT) != (EfiStatus)0)

#define EFI_SUCCESS ((EfiStatus)0)
#define EFI_INVALID_PARAMETER (EFI_ERROR_BIT | (EfiStatus)2u)
#define EFI_TIMEOUT (EFI_ERROR_BIT | (EfiStatus)3u)
#define EFI_OUT_OF_RESOURCES (EFI_ERROR_BIT | (EfiStatus)4u)
#define EFI_UNSUPPORTED (EFI_ERROR_BIT | (EfiStatus)5u)
#define EFI_BUSY (EFI_ERROR_BIT | (EfiStatus)6u)
#define EFI_ACCESS_DENIED (EFI_ERROR_BIT | (EfiStatus)7u)

/* NEF_ convenience aliases */
#define NEF_OK EFI_SUCCESS
#define NEF_ENOSYS EFI_UNSUPPORTED
#define NEF_ENOMEM EFI_OUT_OF_RESOURCES
#define NEF_EINVAL EFI_INVALID_PARAMETER
#define NEF_ETIMEDOUT EFI_TIMEOUT
#define NEF_EBUSY EFI_BUSY
#define NEF_EPERM EFI_ACCESS_DENIED
#define NEF_ENOENT (EFI_ERROR_BIT | (EfiStatus)14u) /* no such file   */
#define NEF_EOF (EFI_ERROR_BIT | (EfiStatus)31u)    /* end of stream  */

/*──────────────────────────────────────────────────────────────────────
 * Calling-convention bridge
 *
 * A driver compiled with a Linux toolchain (x86_64-pc-linux-gnu, System V
 * AMD64 ABI) but intended to run on Windows must define NEF_TARGET_WINDOWS
 * so that all service-table callbacks and the driver entry are decorated
 * with __attribute__((ms_abi)), matching the register assignment used by
 * the Win32 backend (RCX/RDX/R8/R9 instead of RDI/RSI/RDX/RCX).
 *
 * On native Windows builds _WIN32 is defined by the toolchain, so
 * NEF_CALLCONV expands to nothing — the ABI is already Microsoft x64.
 * On native Linux builds neither macro is defined and NEF_CALLCONV is
 * also empty.
 *──────────────────────────────────────────────────────────────────────*/
#if defined(_WIN32)
#define NEF_CALLCONV /* native Windows — already MS ABI */
#elif defined(NEF_TARGET_WINDOWS)
#define NEF_CALLCONV __attribute__((ms_abi))
#else
#define NEF_CALLCONV /* native Linux / bare-metal        */
#endif

#endif /* NEF_ERRNO_H */
