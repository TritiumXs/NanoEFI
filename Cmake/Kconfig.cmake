# cmake/kconfig.cmake — Kconfig integration
#
# Behaviour:
#   1. First run (no .config): launches menuconfig interactively and blocks
#      until the user saves and exits.  Build then proceeds automatically.
#   2. Subsequent runs: parses existing .config and sets CMake variables.
#   3. If .config changes (after `cmake --build <dir> --target menuconfig`),
#      CMake re-runs configure automatically on the next build invocation.

set(KCONFIG_FILE "${CMAKE_SOURCE_DIR}/.config")

# ── Python detection ──────────────────────────────────────────────────
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# ── First-run: launch menuconfig automatically ────────────────────────
if(NOT EXISTS "${KCONFIG_FILE}")
    message(STATUS "No .config found — launching menuconfig...")
    execute_process(
        COMMAND ${Python3_EXECUTABLE} -m menuconfig ${CMAKE_SOURCE_DIR}/Kconfig
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        RESULT_VARIABLE _MC_RESULT
    )
    if(NOT _MC_RESULT EQUAL 0 OR NOT EXISTS "${KCONFIG_FILE}")
        message(FATAL_ERROR
            "menuconfig did not produce a .config.\n"
            "Run manually: python -m menuconfig Kconfig")
    endif()
endif()

# ── Re-configure when .config changes ────────────────────────────────
set_property(DIRECTORY "${CMAKE_SOURCE_DIR}" APPEND PROPERTY
    CMAKE_CONFIGURE_DEPENDS "${KCONFIG_FILE}")

message(STATUS "Loading .config from ${KCONFIG_FILE}")

# ── Parse .config ─────────────────────────────────────────────────────
file(STRINGS "${KCONFIG_FILE}" _KCONFIG_LINES)

foreach(_LINE ${_KCONFIG_LINES})
    if(_LINE MATCHES "^#" OR _LINE STREQUAL "")
        continue()
    endif()
    if(_LINE MATCHES "^CONFIG_([A-Za-z0-9_]+)=y$")
        set(CONFIG_${CMAKE_MATCH_1} 1)
    elseif(_LINE MATCHES "^CONFIG_([A-Za-z0-9_]+)=\"(.*)\"$")
        set(CONFIG_${CMAKE_MATCH_1} "${CMAKE_MATCH_2}")
    endif()
endforeach()

# ── Backend ───────────────────────────────────────────────────────────
if(DEFINED CONFIG_NANOEFI_BACKEND_LINUX)
    set(NANOEFI_BACKEND "linux"     CACHE STRING "" FORCE)
elseif(DEFINED CONFIG_NANOEFI_BACKEND_WIN32)
    set(NANOEFI_BACKEND "win32"     CACHE STRING "" FORCE)
elseif(DEFINED CONFIG_NANOEFI_BACKEND_BAREMETAL)
    set(NANOEFI_BACKEND "baremetal" CACHE STRING "" FORCE)
endif()

# ── RTOS plugin (baremetal only) ─────────────────────────────────────
if(DEFINED CONFIG_NANOEFI_RTOS_FREERTOS)
    set(NANOEFI_RTOS "freertos" CACHE STRING "" FORCE)
    if(DEFINED CONFIG_NANOEFI_FREERTOS_DIR)
        set(FREERTOS_SOURCE_DIR "${CONFIG_NANOEFI_FREERTOS_DIR}" CACHE PATH "" FORCE)
    endif()
else()
    set(NANOEFI_RTOS "none" CACHE STRING "" FORCE)
endif()

# ── CPU / FPU ─────────────────────────────────────────────────────────
if(DEFINED CONFIG_NANOEFI_CPU_CORTEX_M0)
    set(NANOEFI_CPU "cortex-m0" CACHE STRING "" FORCE)
elseif(DEFINED CONFIG_NANOEFI_CPU_CORTEX_M3)
    set(NANOEFI_CPU "cortex-m3" CACHE STRING "" FORCE)
elseif(DEFINED CONFIG_NANOEFI_CPU_CORTEX_M4)
    set(NANOEFI_CPU "cortex-m4" CACHE STRING "" FORCE)
elseif(DEFINED CONFIG_NANOEFI_CPU_CORTEX_M7)
    set(NANOEFI_CPU "cortex-m7" CACHE STRING "" FORCE)
endif()

if(DEFINED CONFIG_NANOEFI_FLOAT_ABI_HARD)
    set(NANOEFI_FLOAT_ABI "hard"   CACHE STRING "" FORCE)
elseif(DEFINED CONFIG_NANOEFI_FLOAT_ABI_SOFTFP)
    set(NANOEFI_FLOAT_ABI "softfp" CACHE STRING "" FORCE)
else()
    set(NANOEFI_FLOAT_ABI "soft"   CACHE STRING "" FORCE)
endif()

if(DEFINED CONFIG_NANOEFI_FPU_FPV4_SP)
    set(NANOEFI_FPU "fpv4-sp-d16" CACHE STRING "" FORCE)
elseif(DEFINED CONFIG_NANOEFI_FPU_FPV5_SP)
    set(NANOEFI_FPU "fpv5-sp-d16" CACHE STRING "" FORCE)
elseif(DEFINED CONFIG_NANOEFI_FPU_FPV5_D16)
    set(NANOEFI_FPU "fpv5-d16"    CACHE STRING "" FORCE)
else()
    set(NANOEFI_FPU ""             CACHE STRING "" FORCE)
endif()

# ── Build type ────────────────────────────────────────────────────────
if(DEFINED CONFIG_NANOEFI_BUILD_DEBUG)
    set(CMAKE_BUILD_TYPE "Debug"          CACHE STRING "" FORCE)
elseif(DEFINED CONFIG_NANOEFI_BUILD_RELEASE)
    set(CMAKE_BUILD_TYPE "Release"        CACHE STRING "" FORCE)
elseif(DEFINED CONFIG_NANOEFI_BUILD_MINSIZEREL)
    set(CMAKE_BUILD_TYPE "MinSizeRel"     CACHE STRING "" FORCE)
else()
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "" FORCE)
endif()
