# NanoEFI

**A lightweight embedded driver interface standard and portable runtime.**

> *Write a driver once. Run it on Linux, STM32, RISC-V — unchanged.*

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![C Standard](https://img.shields.io/badge/C-C11-brightgreen.svg)](include/)
[![RAM overhead](https://img.shields.io/badge/RAM%20overhead-~176%20B-brightgreen.svg)](include/NefServices.h)

---

## What Is NanoEFI?

NanoEFI is an **embedded driver interface standard** — not an RTOS, not a kernel.

It defines a single struct (`NanoEfiServiceTable`) that acts as the universal contract between driver code and the platform. A driver receives a pointer to this table at entry time and calls everything through it. It links no platform symbols, includes no platform headers.

```
┌────────────────────────────────────┐
│   Driver / Application             │
│   #include "NefServices.h" only    │
│   calls everything via Svc->...    │
└──────────────┬─────────────────────┘
               │  NanoEfiServiceTable*
┌──────────────▼─────────────────────┐
│   Service Table  (22 fn pointers)  │
│   Task  Mutex  Sem  Event          │
│   Time  Log  GPIO  QueryCapability │
└──────────────┬─────────────────────┘
               │  platform native API
┌──────────────▼─────────────────────┐
│   Backend                          │
│   Linux (pthread)                  │
│   FreeRTOS / Zephyr                │
│   Bare-metal                       │
└────────────────────────────────────┘
```

### Positioning

| | NanoEFI | mbed OS HAL | UEFI drivers | Bare-metal |
|---|---|---|---|---|
| Interface style | C function pointers | C++ classes | C protocols + GUIDs | None |
| Binary portable | ✅ (same arch) | ❌ source only | ✅ (x86 only) | ❌ |
| No MMU required | ✅ | ✅ | ❌ | ✅ |
| PC dev/debug | ✅ native | ❌ | ✅ (OVMF) | ❌ |
| RAM overhead | ~176 B (table) | ~4 KB | ~128 MB | 0 |
| Freestanding core | ✅ | ❌ | ❌ | ✅ |

NanoEFI and Zephyr/FreeRTOS/Linux are **layered**, not competing:
those systems are NanoEFI's **backends**.

---

## Quick Start

### Prerequisites

```
GCC or Clang, CMake ≥ 3.20, Ninja
```

### Linux (30 seconds)

```bash
git clone https://github.com/nanoefi/nanoefi.git
cd nanoefi
cmake -B build -G Ninja
cmake --build build
./build/sensor_logger
```

Expected output:

```
[NanoEFI] v1.0 — linux backend
[sensor_logger] Starting — 10 samples at 1000 ms intervals
[sensor_logger] sample=0  temp=14 C  tick=... ms
[GPIO] pin 0 = 0
...
[sensor_logger] Done.
```

The `sensor_logger` driver source (`examples/sensor_logger/SensorLogger.c`) includes **only `NefServices.h`**. The same file cross-compiles to STM32 and RISC-V without modification.

---

## Project Structure

```
nanoefi/
├── include/
│   ├── NefErrno.h       ← Primitive types (freestanding), EfiStatus, NEF_* codes
│   ├── NefArch.h        ← Architecture IDs, capability IDs, GPIO modes
│   ├── NefSecurity.h    ← Security policy enum (Open / Signed / Locked)
│   ├── NefImage.h       ← NDIH driver image header (28 bytes)
│   ├── NefServices.h    ← THE interface: NanoEfiServiceTable (22 fn pointers)
│   ├── NefString.h      ← Built-in libc: NefMemcpy, NefStrlen, ...
│   ├── NefFormat.h      ← Built-in libc: NefVsnprintf, NefSnprintf
│   └── NefLoader.h      ← Loader API: NefLoad()
├── lib/
│   ├── NefString.c      ← Freestanding string utilities
│   └── NefFormat.c      ← Freestanding printf subset (no float)
├── loader/
│   └── NefLoader.c      ← NDIH validation + driver execution (OPEN mode)
├── backends/
│   ├── linux/           ← POSIX backend (pthread, clock_gettime, mock GPIO)
│   ├── freertos/        ← (planned)
│   ├── zephyr/          ← (planned)
│   └── baremetal/       ← (planned)
├── examples/
│   └── sensor_logger/   ← Reference driver: same source, 3 build targets
├── cts/                 ← Compliance Test Suite (planned)
└── docs/
    └── spec.md          ← Interface semantic specification
```

---

## The Service Table

`NanoEfiServiceTable` is the **only** interface a driver ever sees.

```c
typedef struct _NanoEfiServiceTable {
    UINT32  Version;                 /* 0x00010000 = v1.0 */

    /* Task management */
    EfiStatus (*CreateTask)(...);
    EfiStatus (*DeleteTask)(...);
    EfiStatus (*TaskSleep)(UINT32 Ms);

    /* Mutex, Semaphore, Event (4 functions each) */
    ...

    /* Time */
    UINT64    (*GetTick)(VOID);
    EfiStatus (*DelayMs)(UINT32 Ms);

    /* Logging (uses NefVsnprintf on freestanding backends) */
    VOID      (*Log)(const char *Fmt, ...);

    /* GPIO */
    EfiStatus (*PinSet)(UINT8 Pin, UINT8 Value);
    EfiStatus (*PinGet)(UINT8 Pin, UINT8 *Value);
    EfiStatus (*PinMode)(UINT8 Pin, UINT8 Mode);

    /* Capability discovery */
    EfiStatus (*QueryCapability)(UINT32 Id, UINT32 *Value);

    VOID      *Extension;            /* NULL in v1.0; future peripheral APIs */
} NanoEfiServiceTable;
```

Full semantic contracts (ISR-safety, blocking behaviour, preconditions) are documented in [`include/NefServices.h`](include/NefServices.h).

---

## Writing a Driver

```c
#include "NefServices.h"

EfiStatus MyDriverEntry(NanoEfiServiceTable *Svc, VOID *Arg)
{
    UINT32 HasFpu = 0;
    Svc->QueryCapability(NANO_EFI_CAP_FPU, &HasFpu);

    Svc->Log("FPU: %s\n", HasFpu ? "YES" : "NO");
    Svc->PinSet(13, 1);
    Svc->DelayMs(500);
    Svc->PinSet(13, 0);

    return NEF_OK;
}
```

Compile once per target architecture. Run on any backend that implements `NanoEfiServiceTable`.

---

## Driver Image Format (NDIH)

Every `.nef` driver image begins with a 28-byte header:

```
Offset  Size  Field
 0x00    4    Magic          "NEFI"
 0x04    2    Version        NDIH format version
 0x06    1    ArchId         NANO_EFI_ARCH_* (0x01=ARMv7-M, 0x02=RV32, 0x10=x86-64)
 0x07    1    Flags          bit 0: XIP-capable
 0x08    4    EntryOffset    byte offset of entry point
 0x0C    4    DataSize       .data section size
 0x10    4    BssSize        .bss  section size
 0x14    4    MinSvcVersion  minimum service table version
 0x18    4    ImageSize      total image size (for CRC32)
```

The loader validates the header, checks CRC32, and optionally verifies an Ed25519 signature (SIGNED mode) before executing the driver.

---

## Backends

| Backend | Status | Dependencies |
|---------|--------|-------------|
| `backends/linux/` | ✅ Implemented | pthreads, librt |
| `backends/freertos/` | 🔧 Planned | FreeRTOS kernel |
| `backends/zephyr/` | 🔧 Planned | Zephyr RTOS API |
| `backends/baremetal/` | 🔧 Planned | SysTick + critical sections |

A backend is ~150–300 lines of C: fill a `NanoEfiServiceTable`, expose an `Init()` function. No framework required.

---

## Error Codes

All fallible operations return `EfiStatus`. Test with `EFI_ERROR(s)`.

```c
#define NEF_OK          EFI_SUCCESS            /* 0                */
#define NEF_ENOSYS      EFI_UNSUPPORTED        /* not implemented  */
#define NEF_ENOMEM      EFI_OUT_OF_RESOURCES   /* out of memory    */
#define NEF_EINVAL      EFI_INVALID_PARAMETER  /* bad argument     */
#define NEF_ETIMEDOUT   EFI_TIMEOUT            /* timed out        */
#define NEF_EBUSY       EFI_BUSY               /* resource busy    */
#define NEF_EPERM       EFI_ACCESS_DENIED      /* security denied  */
```

---

## Roadmap

- [x] Core headers (`NefServices.h`, `NefImage.h`, `NefErrno.h`, …)
- [x] Built-in libc (`NefString.c`, `NefFormat.c`)
- [x] Loader — OPEN mode + CRC32
- [x] Linux backend — full 22-function implementation
- [x] `sensor_logger` demo — Linux
- [ ] FreeRTOS backend + STM32F103 target
- [ ] Bare-metal backend + GD32VF103 RISC-V target
- [ ] Loader — SIGNED mode (Ed25519)
- [ ] Compliance Test Suite (CTS)
- [ ] `docs/spec.md` — interface semantic specification

---

## Contributing

1. **Add a backend** — implement all 22 function pointers, expose `NanoEfiXxxInit()`. See `backends/linux/NefLinux.c` (~300 LOC) as reference.
2. **Add a driver** — include only `NefServices.h`. No platform headers.
3. **Improve the CTS** — every function pointer has a documented contract in `NefServices.h`; write a test for each invariant.

Commit convention: `component: short description` (e.g. `loader: add Ed25519 verify`).
All code: C11, `-Wall -Wextra`, freestanding-safe for core headers.

---

## License

MIT — see [LICENSE](LICENSE).
