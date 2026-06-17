# Cmake/NanoEFI.cmake — SDK module for NanoEFI projects
#
# Usage in any project CMakeLists.txt:
#   include($ENV{NANOEFI_SDK_DIR}/Cmake/NanoEFI.cmake)
#
# Provides:
#   nanoEFI-backend, nanoEFI-loader, ndih_pack
#   add_nef_driver(target SOURCES ... [ENTRY ...] [INCLUDES ...] [DEFINES ...])
#   add_nef_host(target SOURCES ... [DEPENDS ...] [LIBS ...])

cmake_minimum_required(VERSION 3.20)

# ── SDK root derived from this file's location ────────────────────────
get_filename_component(NANOEFI_SDK_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

# ── HAL selection ─────────────────────────────────────────────────────
if(WIN32)
    set(_NEF_HAL_SRC   ${NANOEFI_SDK_DIR}/Backends/Hal/Win32/NefHalWin32.c)
    set(_NEF_PLAT_SRC  ${NANOEFI_SDK_DIR}/Backends/Win32/NefWin32.c)
    set(_NEF_HAL_DEFS "")
    set(_NEF_HAL_LIBS "")
else()
    find_package(Threads REQUIRED)
    set(_NEF_HAL_SRC   ${NANOEFI_SDK_DIR}/Backends/Hal/Linux/NefHalLinux.c)
    set(_NEF_PLAT_SRC  ${NANOEFI_SDK_DIR}/Backends/Linux/NefLinux.c)
    set(_NEF_HAL_DEFS _GNU_SOURCE)
    set(_NEF_HAL_LIBS Threads::Threads)
endif()

# ── nanoEFI-backend (backend + HAL) ──────────────────────────────────
if(NOT TARGET nanoEFI-backend)
    add_library(nanoEFI-backend STATIC
        ${NANOEFI_SDK_DIR}/Backends/NefBackend.c
        ${_NEF_PLAT_SRC}
        ${_NEF_HAL_SRC}
        ${NANOEFI_SDK_DIR}/Lib/NefString.c
        ${NANOEFI_SDK_DIR}/Lib/NefFormat.c)
    if(WIN32)
        set(_NEF_BACKEND_INC ${NANOEFI_SDK_DIR}/Backends/Win32)
    else()
        set(_NEF_BACKEND_INC ${NANOEFI_SDK_DIR}/Backends/Linux)
    endif()
    target_include_directories(nanoEFI-backend PUBLIC
        ${NANOEFI_SDK_DIR}/Include
        ${NANOEFI_SDK_DIR}/Backends
        ${_NEF_BACKEND_INC})
    target_include_directories(nanoEFI-backend PRIVATE
        ${NANOEFI_SDK_DIR}/Backends/Hal)
    target_compile_definitions(nanoEFI-backend PRIVATE ${_NEF_HAL_DEFS})
    target_link_libraries(nanoEFI-backend PUBLIC ${_NEF_HAL_LIBS})
endif()

# ── nanoEFI-loader ────────────────────────────────────────────────────
if(NOT TARGET nanoEFI-loader)
    add_library(nanoEFI-loader STATIC ${NANOEFI_SDK_DIR}/Loader/NefLoader.c)
    target_link_libraries(nanoEFI-loader PUBLIC nanoEFI-backend)
endif()

# ── ndih_pack tool ────────────────────────────────────────────────────
if(NOT TARGET ndih_pack)
    add_executable(ndih_pack ${NANOEFI_SDK_DIR}/Tools/NdihPack.c)
endif()

# ── Driver toolchain selection ────────────────────────────────────────
# -DNEF_DRIVER_TOOLCHAIN=llvm  (default) clang + lld
# -DNEF_DRIVER_TOOLCHAIN=gcc           cross-gcc + ld.bfd
#   For GCC cross-compile set -DNEF_CROSS_PREFIX=<triple>-  e.g. arm-none-eabi-
set(NEF_DRIVER_TOOLCHAIN "llvm" CACHE STRING "Driver toolchain: llvm or gcc")

# Default driver arch triple — override with -DNEF_DRIVER_ARCH=...
if(NOT DEFINED NEF_DRIVER_ARCH)
    set(NEF_DRIVER_ARCH "x86_64-pc-linux-gnu")
endif()

if(NEF_DRIVER_TOOLCHAIN STREQUAL "llvm")
    find_program(NEF_CC  clang       REQUIRED)
    find_program(NEF_OBJCOPY llvm-objcopy REQUIRED)
    set(_NEF_CC_FLAGS   "--target=${NEF_DRIVER_ARCH}" "-fuse-ld=lld")
else()
    set(_gcc "${NEF_CROSS_PREFIX}gcc")
    find_program(NEF_CC  ${_gcc}     REQUIRED)
    find_program(NEF_OBJCOPY "${NEF_CROSS_PREFIX}objcopy" REQUIRED)
    set(_NEF_CC_FLAGS   "")
endif()

# add_nef_driver(<target>
#     SOURCES <file> [<file>...]
#     [ENTRY   <symbol>]          # default: NefDriverEntry
#     [INCLUDES <dir>...]         # extra -I paths
#     [DEFINES  <def>...])        # extra -D defines (without -D prefix)
function(add_nef_driver TARGET)
    cmake_parse_arguments(ARG "" "ENTRY" "SOURCES;INCLUDES;DEFINES" ${ARGN})
    if(NOT ARG_ENTRY)
        set(ARG_ENTRY "NefDriverEntry")
    endif()

    set(ELF ${CMAKE_BINARY_DIR}/${TARGET}.elf)
    set(BIN ${CMAKE_BINARY_DIR}/${TARGET}.bin)
    set(NEF ${CMAKE_BINARY_DIR}/${TARGET}.nef)

    set(_EXTRA_INCS "")
    foreach(D IN LISTS ARG_INCLUDES)
        list(APPEND _EXTRA_INCS -I${D})
    endforeach()

    set(_EXTRA_DEFS "")
    foreach(D IN LISTS ARG_DEFINES)
        list(APPEND _EXTRA_DEFS -D${D})
    endforeach()

    if(WIN32)
        list(APPEND _EXTRA_DEFS -DNEF_TARGET_WINDOWS)
    endif()

    add_custom_command(OUTPUT ${ELF}
        COMMAND ${NEF_CC} ${_NEF_CC_FLAGS}
            -nostdlib -nostartfiles -ffreestanding -O2
            -I${NANOEFI_SDK_DIR}/Include
            ${_EXTRA_INCS} ${_EXTRA_DEFS}
            -T ${NANOEFI_SDK_DIR}/Cmake/Driver.ld
            -Wl,-e,${ARG_ENTRY}
            ${ARG_SOURCES} ${NANOEFI_SDK_DIR}/Lib/NefString.c
            -o ${ELF}
        DEPENDS ${ARG_SOURCES} ${NANOEFI_SDK_DIR}/Cmake/Driver.ld
        VERBATIM)

    add_custom_command(OUTPUT ${BIN}
        COMMAND ${NEF_OBJCOPY} -O binary ${ELF} ${BIN}
        DEPENDS ${ELF} VERBATIM)

    add_custom_command(OUTPUT ${NEF}
        COMMAND ndih_pack ${BIN} ${NEF}
        DEPENDS ndih_pack ${BIN} VERBATIM)

    add_custom_target(${TARGET} ALL DEPENDS ${NEF})
endfunction()

# add_nef_host(<target>
#     SOURCES <file> [<file>...]
#     [DEPENDS  <nef-target>...]  # .nef driver targets this host loads
#     [LIBS     <lib>...])        # extra link libraries
function(add_nef_host TARGET)
    cmake_parse_arguments(ARG "" "" "SOURCES;DEPENDS;LIBS" ${ARGN})

    add_executable(${TARGET} ${ARG_SOURCES})
    target_link_libraries(${TARGET} PRIVATE nanoEFI-backend nanoEFI-loader ${ARG_LIBS})
    if(WIN32)
        target_compile_definitions(${TARGET} PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
    if(ARG_DEPENDS)
        add_dependencies(${TARGET} ${ARG_DEPENDS})
    endif()
endfunction()
