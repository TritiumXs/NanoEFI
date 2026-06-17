cmake_minimum_required(VERSION 3.20)
project(nano-cli C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include($ENV{NANOEFI_SDK_DIR}/Cmake/NanoEFI.cmake)

add_nef_driver(loader SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Loader/Loader.c)
add_nef_driver(shell  SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Shell/Shell.c)

add_nef_host(nano_cli
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Main.c
    DEPENDS loader shell)
