/*
 * loader/nef_loader.c — NanoEFI Driver Image Loader (OPEN mode)
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 NanoEFI Contributors
 *
 * Freestanding: uses only nef_* headers.
 */

#include "NefLoader.h"

#include "NefArch.h"
#include "NefImage.h"
#include "NefString.h"

/*──────────────────────────────────────────────────────────────────────
 * CRC32 (Ethernet polynomial 0xEDB88320, reflected input/output)
 *──────────────────────────────────────────────────────────────────────*/

static UINT32 Crc32Byte(UINT32 Crc, UINT8 Byte) {
  UINT32 C = Crc ^ (UINT32)Byte;
  int I;
  for (I = 0; I < 8; I++)
    C = (C & 1u) ? (0xEDB88320u ^ (C >> 1u)) : (C >> 1u);

  return C;
}

static UINT32 Crc32(const VOID* Data, NEF_SIZE Len) {
  const UINT8* P = (const UINT8*)Data;
  UINT32 Crc = 0xFFFFFFFFu;
  while (Len--)
    Crc = Crc32Byte(Crc, *P++);

  return Crc ^ 0xFFFFFFFFu;
}

/*──────────────────────────────────────────────────────────────────────
 * Read a uint32_t from a potentially unaligned byte pointer
 *──────────────────────────────────────────────────────────────────────*/

static UINT32 ReadU32(const UINT8* P) {
  return (UINT32)P[0] | ((UINT32)P[1] << 8u) | ((UINT32)P[2] << 16u) |
         ((UINT32)P[3] << 24u);
}

/*──────────────────────────────────────────────────────────────────────
 * NefLoad
 *──────────────────────────────────────────────────────────────────────*/

EfiStatus NefLoad(IN const VOID* Image, IN NEF_SIZE ImageBytes,
                  IN NanoEfiServiceTable* Svc, IN VOID* Arg,
                  IN NefAllocFn Alloc, IN NefFreeFn Free,
                  IN NanoEfiSecurity SecurityLevel) {
  const NanoEfiImageHeader* Hdr;
  const UINT8* Base;
  UINT8* Ram;
  UINT32 StoredCrc, ComputedCrc;
  NanoEfiDriverEntry Entry;
  EfiStatus Status;

  /* ── LOCKED: refuse all dynamic loading ──────────────────────── */
  if (SecurityLevel == NanoEfiSecurityLocked)
    return NEF_EPERM;

  /* ── Basic pointer / size validation ─────────────────────────── */
  if (!Image || !Svc || !Alloc || !Free)
    return NEF_EINVAL;

  if (ImageBytes < sizeof(NanoEfiImageHeader))
    return NEF_EINVAL;

  Base = (const UINT8*)Image;
  Hdr = (const NanoEfiImageHeader*)Image;

  /* ── 1. Magic ──────────────────────────────────────────────────── */
  if (NefMemcmp(Hdr->Magic, NANO_EFI_NDIH_MAGIC_STR, 4) != 0)
    return NEF_EINVAL;

  /* ── 2. NDIH version ───────────────────────────────────────────── */
  if (Hdr->Version > NANO_EFI_NDIH_VERSION)
    return NEF_ENOSYS;

  /* ── 3. Architecture ───────────────────────────────────────────── */
  if (Hdr->ArchId != (UINT8)NANO_EFI_ARCH_CURRENT)
    return NEF_ENOSYS;

  /* ── 4. Entry offset within image ─────────────────────────────── */
  if ((NEF_SIZE)Hdr->EntryOffset >= (NEF_SIZE)Hdr->ImageSize)
    return NEF_EINVAL;

  /* ── 5. Image fits in provided buffer ─────────────────────────── */
  if ((NEF_SIZE)Hdr->ImageSize > ImageBytes)
    return NEF_EINVAL;

  /* ── 6. Service table version ──────────────────────────────────── */
  if (Hdr->MinSvcVersion > Svc->Version)
    return NEF_ENOSYS;

  /* ── 7. CRC32 trailer present ──────────────────────────────────── */
  if (Hdr->ImageSize < 4u)
    return NEF_EINVAL;

  StoredCrc = ReadU32(Base + Hdr->ImageSize - 4u);
  ComputedCrc = Crc32(Base, (NEF_SIZE)Hdr->ImageSize - 4u);

  if (StoredCrc != ComputedCrc)
    return NEF_EINVAL;

  /* ── 8. Ed25519 signature (SIGNED mode only) ───────────────────── */
  if (SecurityLevel == NanoEfiSecuritySigned) {
    /* TODO: implement Ed25519 verify using embedded public key     */
    return NEF_ENOSYS;
  }

  /* ── Allocate RAM for .data and .bss ───────────────────────────── */
  Ram = (UINT8*)NULL;
  if (Hdr->DataSize + Hdr->BssSize > 0u) {
    Ram = (UINT8*)Alloc((NEF_SIZE)(Hdr->DataSize + Hdr->BssSize));
    if (!Ram)
      return NEF_ENOMEM;

    if (Hdr->DataSize > 0u)
      NefMemcpy(Ram, Base + Hdr->EntryOffset + Hdr->DataSize, Hdr->DataSize);

    NefMemset(Ram + Hdr->DataSize, 0, (NEF_SIZE)Hdr->BssSize);
  }

  /* ── Execute driver ─────────────────────────────────────────────── */
  Entry = (NanoEfiDriverEntry)(UINTN)(Base + Hdr->EntryOffset);
  Status = Entry(Svc, Arg);

  /* ── Free RAM ───────────────────────────────────────────────────── */
  if (Ram)
    Free(Ram);

  return Status;
}
