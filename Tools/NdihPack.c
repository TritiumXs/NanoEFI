/*
 * tools/ndih_pack.c — Pack a flat binary into a NanoEFI .nef image
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 NanoEFI Contributors
 *
 * Usage: ndih_pack <input.bin> <output.nef>
 *
 * Produces: [28-byte NDIH][code from input.bin][4-byte CRC32]
 * Streams input in chunks; never loads the full binary into memory.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NANO_EFI_ARCH_X86_64 0x10u
#define SVC_VERSION_1_0 0x00010000u
#define CHUNK_SIZE 65536u

static uint32_t Crc32Update(uint32_t Crc, const uint8_t* Data, size_t Len) {
  while (Len--) {
    Crc ^= *Data++;
    for (int I = 0; I < 8; I++)
      Crc = (Crc & 1u) ? (0xEDB88320u ^ (Crc >> 1u)) : (Crc >> 1u);
  }
  return Crc;
}

static void W16(uint8_t* P, uint16_t V) { P[0] = V; P[1] = V >> 8; }
static void W32(uint8_t* P, uint32_t V) {
  P[0] = V; P[1] = V >> 8; P[2] = V >> 16; P[3] = V >> 24;
}

int main(int argc, char** argv) {
  if (argc != 3) {
    fprintf(stderr, "usage: ndih_pack <input.bin> <output.nef>\n");
    return 1;
  }

  FILE* In;
  FILE* Out;
#ifdef _MSC_VER
  fopen_s(&In,  argv[1], "rb");
  fopen_s(&Out, argv[2], "wb");
#else
  In  = fopen(argv[1], "rb");
  Out = fopen(argv[2], "wb");
#endif
  if (!In)  { perror(argv[1]); return 1; }
  if (!Out) { perror(argv[2]); fclose(In); return 1; }

  fseek(In, 0, SEEK_END);
  long CodeLen = ftell(In);
  rewind(In);

  /* Build 28-byte header on the stack */
  uint8_t Hdr[28] = {0};
  memcpy(Hdr, "NEFI", 4);
  W16(Hdr + 4,  0x0001u);
  Hdr[6] = NANO_EFI_ARCH_X86_64;
  Hdr[7] = 0;
  W32(Hdr + 8,  28u);
  W32(Hdr + 12, 0u);
  W32(Hdr + 16, 0u);
  W32(Hdr + 20, SVC_VERSION_1_0);
  W32(Hdr + 24, (uint32_t)(28 + CodeLen + 4));

  fwrite(Hdr, 1, 28, Out);
  uint32_t Crc = Crc32Update(0xFFFFFFFFu, Hdr, 28);

  /* Stream payload */
  uint8_t* Chunk = malloc(CHUNK_SIZE);
  if (!Chunk) { fclose(In); fclose(Out); return 1; }

  size_t N;
  while ((N = fread(Chunk, 1, CHUNK_SIZE, In)) > 0) {
    Crc = Crc32Update(Crc, Chunk, N);
    fwrite(Chunk, 1, N, Out);
  }
  free(Chunk);
  fclose(In);

  Crc ^= 0xFFFFFFFFu;

  uint8_t Trailer[4];
  W32(Trailer, Crc);
  fwrite(Trailer, 1, 4, Out);
  fclose(Out);

  size_t Total = 28 + (size_t)CodeLen + 4;
  printf("ndih_pack: %s  (%zu bytes, CRC32=0x%08X)\n", argv[2], Total, Crc);
  return 0;
}
