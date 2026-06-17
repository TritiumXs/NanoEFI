/*
 * nef_security.h — NanoEFI Driver Load Security Policy
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 NanoEFI Contributors
 *
 * Security level is a compile-time constant, not a runtime flag.
 * Set NANO_EFI_SECURITY_LEVEL in CMakeLists.txt or via -D.
 */

#ifndef NEF_SECURITY_H
#define NEF_SECURITY_H

/*──────────────────────────────────────────────────────────────────────
 * Security Policy Enum
 *──────────────────────────────────────────────────────────────────────*/

typedef enum {
  /*
   * NanoEfiSecurityOpen — Load any driver image without authentication.
   * Intended for development.  CRC32 integrity check still applies.
   */
  NanoEfiSecurityOpen = 0,

  /*
   * NanoEfiSecuritySigned — Only load images whose Ed25519 signature
   * verifies against the embedded public key.  The 96-byte signature
   * block (32-byte pubkey + 64-byte sig) is appended immediately after
   * the 28-byte NDIH header, before image code.  CRC32 also applies.
   */
  NanoEfiSecuritySigned = 1,

  /*
   * NanoEfiSecurityLocked — Reject all dynamic driver loading.
   * NefLoad() returns NEF_EPERM immediately.  Only code burned into
   * Flash at production time runs.
   */
  NanoEfiSecurityLocked = 2,
} NanoEfiSecurity;

/*──────────────────────────────────────────────────────────────────────
 * Ed25519 Constants (SIGNED mode)
 *──────────────────────────────────────────────────────────────────────*/

#define NANO_EFI_ED25519_PUBKEY_SIZE 32u /* bytes */
#define NANO_EFI_ED25519_SIG_SIZE 64u    /* bytes */
#define NANO_EFI_SIG_BLOCK_SIZE \
  (NANO_EFI_ED25519_PUBKEY_SIZE + NANO_EFI_ED25519_SIG_SIZE) /* 96 bytes */

/*──────────────────────────────────────────────────────────────────────
 * Default security level (override via CMake: -DNANO_EFI_SECURITY=1)
 *──────────────────────────────────────────────────────────────────────*/

#ifndef NANO_EFI_SECURITY_LEVEL
#define NANO_EFI_SECURITY_LEVEL NanoEfiSecurityOpen
#endif

#endif /* NEF_SECURITY_H */
