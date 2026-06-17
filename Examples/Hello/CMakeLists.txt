cmake_minimum_required(VERSION 3.20)
project(hello-nanoefi C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include($ENV{NANOEFI_SDK_DIR}/Cmake/NanoEFI.cmake)

add_nef_driver(hello
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Hello.c
    ENTRY   HelloEntry)
