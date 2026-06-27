/*
 * Backends/Hal/Baremetal/NefBoard.h — Board abstraction for baremetal HAL
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 NanoEFI Contributors
 *
 * Each board provides one NefBoard*.c implementing these symbols.
 */

#ifndef NEF_BOARD_H
#define NEF_BOARD_H

#include "NefErrno.h"

/* Board init (clocks, UART, SysTick) */
void     NefBoardInit(void);

/* Monotonic tick in milliseconds */
UINT64   NefBoardGetTickMs(void);

/* Busy-wait */
void     NefBoardDelayMs(UINT32 Ms);

/* UART log output (blocking) */
void     NefBoardUartPutchar(char C);

/* GPIO */
EfiStatus NefBoardPinSet(UINT8 Pin, UINT8 Val);
EfiStatus NefBoardPinGet(UINT8 Pin, UINT8 *Val);
EfiStatus NefBoardPinMode(UINT8 Pin, UINT8 Mode);

/* Capability flags (board-specific) */
UINT32   NefBoardQueryCap(UINT32 Id);

/* Critical section (disable/restore interrupts) */
UINT32   NefBoardEnterCritical(void);
void     NefBoardExitCritical(UINT32 SavedState);

#endif /* NEF_BOARD_H */
