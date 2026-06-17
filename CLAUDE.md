# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Identity

NanoEFI is a minimal, portable runtime interface inspired by UEFI's service-table pattern,
independently implemented in pure C. Design goal: **binary lightness** — service table RAM
overhead is 176 B (32-bit), backend Flash footprint ~2 KB after dead-code elimination.
Target: MCUs (ARMv7-M, RISC-V) and PCs (Linux, Windows). The service table is the contract;
drivers are the commodity; backends are the product.

## Build Commands

The root `CMakeLists.txt` builds **SDK libraries only** (headers, lib, loader, platform backend).
It does not build examples or tools — those live in standalone sub-projects.

```bash
# SDK libraries (auto-detects win32 on Windows, linux on Unix)
cmake --preset native-debug   && cmake --build --preset native-debug
cmake --preset native-release && cmake --build --preset native-release
```

### sensor_logger example (Linux — quickest smoke-test)

```bash
cmake -B build -G Ninja && cmake --build build
./build/sensor_logger
```

`Examples/SensorLogger/SensorLogger.c` is the canonical driver: includes only `NefServices.h`,
runs on Linux now, cross-compiles to STM32/RISC-V unchanged.

### Cross-compile for ARMv7-M

```bash
cmake -B build/arm -G Ninja -DCMAKE_TOOLCHAIN_FILE=Cmake/ArmNoneEabi.cmake
cmake --build build/arm
```

### Hello dynamic-loader example (Windows)

```powershell
# One-time: set NANOEFI_SDK_DIR in the current shell
. Scripts/SetupEnv.ps1

cd examples/hello
cmake --preset default   # generator: Ninja, binaryDir: build/
cmake --build build
.\build\hello-win32.exe .\build\hello.nef
```

The preset requires `NANOEFI_SDK_DIR` (set by `setup_env.ps1` to the repo root).
The driver (`Hello.c`) is cross-compiled to a flat binary via
`clang --target=x86_64-pc-linux-gnu -fuse-ld=lld` + `llvm-objcopy -O binary`,
then packed by `ndih_pack` into `hello.nef`.

## Architecture (3 layers, 1 rule)

```
  DRIVERS / APPS           ← include NefServices.h ONLY. Never backend headers.
       │                     Call EVERYTHING through Svc->*.
       ▼
  SERVICE TABLE (22 funcs) ← NanoEfiServiceTable (NefServices.h). All fallible ops return EfiStatus.
       │                     Output params for handles. Extension *void for optional peripherals.
       ▼
  BACKENDS (~150 LOC each) ← Backends/Linux/NefLinux.c  — pthreads, POSIX clock
                             Backends/Win32/NefWin32.c  — Win32 HANDLE-based
                             (freertos.c / baremetal.c planned)
```

**The inviolable rule:** Drivers MUST NOT include any backend or platform header.
They interact with the world exclusively through the `Svc` pointer passed to their entry function.

## Calling Convention Bridge (`NEF_CALLCONV`)

A driver cross-compiled with `--target=x86_64-pc-linux-gnu` uses the System V AMD64 ABI
(args in RDI/RSI) while the Win32 backend uses Microsoft x64 ABI (args in RCX/RDX).
All service-table function pointers and `NanoEfiDriverEntry` are annotated with
`NEF_CALLCONV`, which expands to `__attribute__((ms_abi))` when `NEF_TARGET_WINDOWS` is
defined. **Cross-compiled drivers for Windows must pass `-DNEF_TARGET_WINDOWS`** (already done
in `Examples/Hello/CMakeLists.txt`). Native Windows and Linux builds leave `NEF_CALLCONV`
empty — no change needed.

## Key Files (read order for onboarding)

1. `Include/NefErrno.h` — freestanding primitive types, `EfiStatus`, `NEF_*` aliases, `NEF_CALLCONV` macro.
2. `Include/NefServices.h` — the entire API: `NanoEfiServiceTable` (22 fn pointers + `Version` + `Extension`).
2a. `Include/NefString.h` / `Include/NefFormat.h` — freestanding libc subset (`NefMemcpy`, `NefSnprintf`, …); use in drivers/backends instead of libc. Implementations in `Lib/`.
3. `Include/NefImage.h` — `NanoEfiImageHeader` (28-byte NDIH), `NanoEfiDriverEntry` typedef.
4. `Include/NefArch.h` — arch IDs, capability IDs (`NANO_EFI_CAP_*`), GPIO modes, timeout constants.
5. `Include/NefSecurity.h` — `NanoEfiSecurity` enum: `Open` / `Signed` / `Locked`.
6. `Backends/Linux/NefLinux.c` — reference backend; canonical 22-function implementation.
7. `Backends/Win32/NefWin32.c` — Win32 backend; `HANDLE` maps directly to opaque handle types.
8. `Loader/NefLoader.c` — NDIH validator + driver launcher; verifies CRC32, allocates .data/.bss RAM, calls `Entry(Svc, Arg)`.
9. `Tools/NdihPack.c` — prepends 28-byte NDIH to a flat binary and appends CRC32.
10. `Examples/Hello/` — minimal dynamic-loading demo (cross-compiled driver + Win32 host).
11. `Examples/SensorLogger/` — primary Linux demo; same source targets STM32/RISC-V.

## NDIH Header Layout (28 bytes, all fields LE)

```
Offset  Size  Field
0x00    4     Magic: "NEFI" (0x4946454E LE)
0x04    2     Version: NDIH format version (currently 0x0001)
0x06    1     ArchId: NANO_EFI_ARCH_*
0x07    1     Flags: bit 0 = XIP-capable
0x08    4     EntryOffset: byte offset of entry point from image base
0x0C    4     DataSize: .data section size (loader allocs RAM)
0x10    4     BssSize: .bss section size (zero-filled)
0x14    4     MinSvcVersion: minimum NanoEfiServiceTable.Version
0x18    4     ImageSize: total image size including 4-byte CRC32 trailer
```

Arch IDs: `0x01` ARMv7-M, `0x02` RISCV32, `0x10` x86-64, `0x11` AArch64.
Images end with a 4-byte CRC32 (Ethernet poly, reflected). `ndih_pack` appends it; `NefLoad` verifies it.

## Dynamic Driver Build Flow

```
driver.c  →  clang --target=x86_64-pc-linux-gnu -fuse-ld=lld
              -DNEF_TARGET_WINDOWS -nostdlib -ffreestanding
              -T driver.ld -Wl,-e,EntryFn          →  ELF
          →  llvm-objcopy -O binary  driver.elf     →  flat .bin
          →  ndih_pack <flat.bin> <out.nef>          →  [NDIH 28B][code][CRC32 4B]

host.exe  →  VirtualAlloc(PAGE_EXECUTE_READWRITE)
          →  fread(.nef into exec buffer)
          →  NefLoad(buf, sz, &svc, NULL, malloc, free, NanoEfiSecurityOpen)
```

`driver.ld` must place the entry symbol first: `SECTIONS { . = 0; .text : { KEEP(*(.text.entry)) *(.text*) } ... }`.
`OUTPUT_FORMAT(binary)` must NOT be in `driver.ld` — use `llvm-objcopy` to extract the flat binary instead.

## API Conventions

- All fallible ops return `EfiStatus`. Test with `EFI_ERROR(s)` or compare to `NEF_*` aliases.
- Handles (`NanoEfiTask`, `NanoEfiMutex`, etc.) are `void*`; backends cast to their heap objects.
- `timeout_ms = 0` → poll. `timeout_ms = 0xFFFFFFFF` → block forever (`== INFINITE` on Windows).
- Drivers must `QueryCapability(NANO_EFI_CAP_MALLOC, &v)` before using heap (NULL on MCUs without heap).
- `Svc->Extension` is NULL in v1.0. Future peripheral tables hang off it.
- `NanoEfiWin32Init` / `NanoEfiLinuxInit` return `EfiStatus` — check with `EFI_ERROR()` before using `Svc`.

## What Stays OUT of the Core Table

Peripherals (I²C, SPI, UART, CAN, USB), filesystem, networking, display, DMA — these belong in
extension tables. The test: if it can be built from the 22 existing primitives, it stays out.

## Coding Style

Formatting: Google C++ style (`BasedOnStyle: Google`). Enforced via `.clang-format` — run `clang-format -i` before committing.
Naming: PascalCase (UpperCamelCase) for all identifiers — functions, variables, types, macros. Never snake_case.
Struct initializers: always use designated initializers (`{.Field = val}`). Never positional (`{val1, val2}`).
`Signed-off-by:` required on all commits.
`#include` paths must never use `../` traversal — rely on CMake `-I` flags so all headers are referenced by filename only (e.g. `"NefArch.h"`, not `"../../Include/NefArch.h"`).

## Commit Convention

```
subsystem: short description (max 72 chars)

Signed-off-by: Lucy Wong <mova845@outlook.com>
```

Valid prefixes: `backend:`, `loader:`, `header:`, `example:`, `test:`, `tools:`, `spec:`.

## License

MIT (`SPDX-License-Identifier: MIT`). Every file must carry the SPDX tag and copyright line.
