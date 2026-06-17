cmake_minimum_required(VERSION 3.20)
project(NanoEFI VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

add_compile_options(-Wall -Wextra
    "$<$<CONFIG:Debug>:-O0>" "$<$<CONFIG:Debug>:-g3>"
    "$<$<CONFIG:RelWithDebInfo>:-O2>" "$<$<CONFIG:RelWithDebInfo>:-g>"
    "$<$<CONFIG:Release>:-O2>"
    "$<$<CONFIG:MinSizeRel>:-Os>")

#──────────────────────────────────────────────────────────────────────
# Core Interface Target  (header-only; drivers link against this)
#──────────────────────────────────────────────────────────────────────

add_library(nanoEFI-headers INTERFACE)
target_include_directories(nanoEFI-headers INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Include>)

#──────────────────────────────────────────────────────────────────────
# Built-in Library (nef_ utilities; freestanding)
#──────────────────────────────────────────────────────────────────────

add_library(nanoEFI-lib STATIC
    Lib/NefString.c
    Lib/NefFormat.c)
target_link_libraries(nanoEFI-lib PUBLIC nanoEFI-headers)

#──────────────────────────────────────────────────────────────────────
# Loader (OPEN mode; links nanoEFI-lib for NefMemcpy etc.)
#──────────────────────────────────────────────────────────────────────

add_library(nanoEFI-loader STATIC Loader/NefLoader.c)
target_link_libraries(nanoEFI-loader PUBLIC nanoEFI-headers nanoEFI-lib)

#──────────────────────────────────────────────────────────────────────
# Platform Backends
#──────────────────────────────────────────────────────────────────────

# Linux backend (POSIX — requires pthreads)
if(NOT WIN32)
    find_package(Threads REQUIRED)
    add_library(nanoEFI-linux STATIC Backends/Linux/NefLinux.c)
    target_compile_definitions(nanoEFI-linux PRIVATE _GNU_SOURCE)
    target_link_libraries(nanoEFI-linux PUBLIC nanoEFI-headers Threads::Threads)
endif()

# Win32 backend (native Windows API — no external dependencies)
if(WIN32)
    add_library(nanoEFI-win32 STATIC Backends/Win32/NefWin32.c)
    target_include_directories(nanoEFI-win32 PUBLIC Backends/Win32)
    target_link_libraries(nanoEFI-win32 PUBLIC nanoEFI-headers)
    # tolerate identical typedef redeclarations (Win32 SDK vs NefErrno.h)
    target_compile_options(nanoEFI-win32 PRIVATE -Wno-pedantic)
endif()

#──────────────────────────────────────────────────────────────────────
# Summary
#──────────────────────────────────────────────────────────────────────

message(STATUS "NanoEFI ${PROJECT_VERSION}")
message(STATUS "  Build type : ${CMAKE_BUILD_TYPE}")
message(STATUS "  Compiler   : ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "  Target     : ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}")
