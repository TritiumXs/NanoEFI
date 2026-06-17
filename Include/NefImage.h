/**
 * @file NefImage.h
 * @brief NanoEFI Driver Image Header (NDIH) — 28-byte binary format.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 NanoEFI Contributors
 *
 * FREESTANDING: includes only NefErrno.h.
 *
 * Every .nef driver image begins with a 28-byte NDIH.  The loader
 * validates this header before executing any driver code.
 *
 * @par Binary Layout (all fields little-endian, packed)
 * @code
 *   Offset  Size  Field
 *   ──────  ────  ──────────────────────────────────────────────────
 *    0x00     4   Magic           "NEFI" (0x4946454E LE)
 *    0x04     2   Version         NDIH format version (currently 0x0001)
 *    0x06     1   ArchId          NANO_EFI_ARCH_*
 *    0x07     1   Flags           NANO_EFI_IMAGE_FLAG_*
 *    0x08     4   EntryOffset     Byte offset of entry point from image base
 *    0x0C     4   DataSize        .data section size in bytes (loader allocs
 * RAM) 0x10     4   BssSize         .bss  section size in bytes (zero-filled)
 *    0x14     4   MinSvcVersion   Minimum NanoEfiServiceTable.Version accepted
 *    0x18     4   ImageSize       Total image size in bytes including CRC32
 * trailer ──────  ──── Total   28 bytes
 * @endcode
 *
 * @par Signed-mode extension
 * In @c NanoEfiSecuritySigned mode a 96-byte block (32-byte Ed25519 public
 * key + 64-byte signature) is appended immediately after the header, before
 * the code section.  @c EntryOffset accounts for this padding.
 *
 * @see NefLoader.c  — validates the header and launches the driver
 * @see ndih_pack.c  — prepends this header and appends the CRC32 trailer
 */

#ifndef NEF_IMAGE_H
#define NEF_IMAGE_H

#include "NefErrno.h"

/**
 * @defgroup ndih NDIH Image Format
 * @brief Constants, flags, and the packed header struct for .nef driver images.
 * @{
 */

/* ── Magic and Version ──────────────────────────────────────────────────── */

/** ASCII magic string at image offset 0x00. */
#define NANO_EFI_NDIH_MAGIC_STR "NEFI"

/** Magic as a LE uint32 — compared directly against the first 4 bytes. */
#define NANO_EFI_NDIH_MAGIC_U32 ((UINT32)0x4946454Eu) /* "NEFI" LE */

/** Current NDIH format version written by ndih_pack and checked by NefLoad. */
#define NANO_EFI_NDIH_VERSION ((UINT16)0x0001u)

/* ── Image Flags (NanoEfiImageHeader.Flags) ─────────────────────────────── */

/**
 * @defgroup ndih_flags Image Flags
 * @brief Bit flags stored in @c NanoEfiImageHeader.Flags.
 * @{
 */

/**
 * @brief Execute-in-Place flag.
 *
 * When set, the .text section resides in Flash and must not be copied to RAM.
 * The loader skips the .text relocation step and jumps directly to the Flash
 * address.  @c DataSize and @c BssSize still describe RAM requirements for
 * writable sections.
 */
#define NANO_EFI_IMAGE_FLAG_XIP 0x01u

/** @} */ /* ndih_flags */

/* ── NDIH Structure (28 bytes, packed) ──────────────────────────────────── */

/**
 * @brief Packed 28-byte header present at byte 0 of every .nef image.
 *
 * The loader reads this struct, validates it, then maps the image into
 * executable memory.  All multi-byte fields are stored little-endian.
 *
 * @note The trailing 4-byte CRC32 (Ethernet poly, reflected) is **not**
 *       part of this struct.  It follows immediately after the last byte
 *       of the image payload and is verified by NefLoad before the driver
 *       entry point is called.
 */
typedef struct {
  UINT8 Magic[4]; /**< Must equal ::NANO_EFI_NDIH_MAGIC_STR ("NEFI"). */
  UINT16
  Version;      /**< NDIH format version; must equal ::NANO_EFI_NDIH_VERSION. */
  UINT8 ArchId; /**< Target ISA — one of the @c NANO_EFI_ARCH_* constants. */
  UINT8 Flags;  /**< Bit field of @c NANO_EFI_IMAGE_FLAG_* values. */
  UINT32 EntryOffset; /**< Byte offset of NanoEfiDriverEntry from image base
                         (byte 0). */
  UINT32
  DataSize; /**< Size of the .data section; loader allocates this many bytes
               of RAM and copies the initialised data from the image. */
  UINT32 BssSize;       /**< Size of the .bss section; loader allocates and
                           zero-fills. */
  UINT32 MinSvcVersion; /**< Minimum acceptable NanoEfiServiceTable::Version;
                           NefLoad rejects older runtimes. */
  UINT32 ImageSize; /**< Total image size in bytes, including this header and
                       the 4-byte CRC32 trailer. */
} __attribute__((packed)) NanoEfiImageHeader;

/** @cond INTERNAL */
/* Compile-time size assertion — breaks build if struct packing is wrong. */
typedef char _NdihSizeCheck[sizeof(NanoEfiImageHeader) == 28 ? 1 : -1];
/** @endcond */

/** @} */ /* ndih */

/* ── Driver Entry Prototype ─────────────────────────────────────────────── */

struct _NanoEfiServiceTable;

/**
 * @brief Function signature every .nef driver must export as its entry point.
 *
 * The loader resolves @c EntryOffset from the image base, casts it to this
 * type, and invokes it after completing memory setup.
 *
 * @param Svc  Pointer to the host's service table.  Never NULL.
 * @param Arg  Optional argument forwarded from the NefLoad caller (may be
 * NULL).
 * @return     ::EFI_SUCCESS on success, or an @c EFI_* / @c NEF_* error code.
 *
 * @note Annotated with ::NEF_CALLCONV to handle the SysV/MS-ABI bridge when
 *       a driver is cross-compiled for Windows with @c -DNEF_TARGET_WINDOWS.
 */
typedef EfiStatus(NEF_CALLCONV* NanoEfiDriverEntry)(
    struct _NanoEfiServiceTable* Svc, VOID* Arg OPTIONAL);

#endif /* NEF_IMAGE_H */
