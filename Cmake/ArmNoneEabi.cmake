# NanoEFI ARMv7-M Generic Toolchain
#
# Covers Cortex-M0 / M0+ / M3 / M4 / M7.
# Tune via cache variables at configure time — no MCU is hardcoded.
#
# Examples:
#   cmake -B build/m0  -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake \
#         -DNANOEFI_CPU=cortex-m0
#
#   cmake -B build/m4f -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake \
#         -DNANOEFI_CPU=cortex-m4 -DNANOEFI_FLOAT_ABI=hard -DNANOEFI_FPU=fpv4-sp-d16
#
#   cmake -B build/m7  -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake \
#         -DNANOEFI_CPU=cortex-m7 -DNANOEFI_FLOAT_ABI=hard -DNANOEFI_FPU=fpv5-d16

set(CMAKE_SYSTEM_NAME               Generic)
set(CMAKE_SYSTEM_PROCESSOR          arm)

#──────────────────────────────────────────────────────────────────────
# User-configurable CPU / FPU knobs
#──────────────────────────────────────────────────────────────────────

set(NANOEFI_CPU       cortex-m3  CACHE STRING "ARM CPU (-mcpu=)")
set(NANOEFI_FLOAT_ABI soft       CACHE STRING "Floating-point ABI (soft / softfp / hard)")
set(NANOEFI_FPU        ""        CACHE STRING "FPU type (-mfpu=), empty if none")

set_property(CACHE NANOEFI_FLOAT_ABI PROPERTY STRINGS soft softfp hard)

#──────────────────────────────────────────────────────────────────────
# Toolchain binaries
#──────────────────────────────────────────────────────────────────────

set(TOOLCHAIN_PREFIX arm-none-eabi- CACHE STRING "ARM GCC toolchain prefix")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_C_COMPILER    ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_CXX_COMPILER  ${TOOLCHAIN_PREFIX}g++)
set(CMAKE_ASM_COMPILER  ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_AR            ${TOOLCHAIN_PREFIX}gcc-ar)
set(CMAKE_OBJCOPY       ${TOOLCHAIN_PREFIX}objcopy)
set(CMAKE_SIZE          ${TOOLCHAIN_PREFIX}size)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

#──────────────────────────────────────────────────────────────────────
# Build flags — assembled from the knobs above
#──────────────────────────────────────────────────────────────────────

set(CPU_FLAGS -mcpu=${NANOEFI_CPU} -mthumb)

if(NANOEFI_FLOAT_ABI)
    string(STRIP "${NANOEFI_FLOAT_ABI}" NANOEFI_FLOAT_ABI)
    set(CPU_FLAGS ${CPU_FLAGS} -mfloat-abi=${NANOEFI_FLOAT_ABI})
endif()

if(NANOEFI_FPU)
    string(STRIP "${NANOEFI_FPU}" NANOEFI_FPU)
    set(CPU_FLAGS ${CPU_FLAGS} -mfpu=${NANOEFI_FPU})
endif()

set(CMAKE_C_FLAGS_INIT        "${CPU_FLAGS} -fdata-sections -ffunction-sections")
set(CMAKE_ASM_FLAGS_INIT      "${CPU_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,--gc-sections -specs=nosys.specs -specs=nano.specs")

#──────────────────────────────────────────────────────────────────────
# Diagnostic
#──────────────────────────────────────────────────────────────────────

message(STATUS "ARM toolchain loaded:")
message(STATUS "  CPU       : ${NANOEFI_CPU}")
message(STATUS "  Float ABI : ${NANOEFI_FLOAT_ABI}")
message(STATUS "  FPU       : ${NANOEFI_FPU}")
message(STATUS "  Flags     : ${CPU_FLAGS}")
