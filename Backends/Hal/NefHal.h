/*
 * backends/hal/NefHal.h — NanoEFI Platform HAL Interface
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 NanoEFI Contributors
 *
 * Each platform provides one NefHal*.c that implements every symbol here.
 * Only NefBackend.c and the platform HAL files ever include this header.
 */

#ifndef NEF_HAL_H
#define NEF_HAL_H

#include <stdarg.h>

#include "NefErrno.h"

typedef void* NefHalHandle;
typedef void (*NefHalTaskFn)(void* Arg);

/* Lifecycle */
EfiStatus NefHalInit(void);

/* Task */
EfiStatus NefHalTaskCreate(NefHalTaskFn Fn, void* Arg, UINT32 Stack,
                           UINT32 Prio, NefHalHandle* Out);
EfiStatus NefHalTaskDelete(NefHalHandle Task);
EfiStatus NefHalTaskSleep(UINT32 Ms);

/* Mutex */
EfiStatus NefHalMutexCreate(NefHalHandle* Out);
EfiStatus NefHalMutexLock(NefHalHandle M, UINT32 TimeoutMs);
EfiStatus NefHalMutexUnlock(NefHalHandle M);
EfiStatus NefHalMutexDelete(NefHalHandle M);

/* Semaphore */
EfiStatus NefHalSemCreate(UINT32 Initial, UINT32 Max, NefHalHandle* Out);
EfiStatus NefHalSemTake(NefHalHandle S, UINT32 TimeoutMs);
EfiStatus NefHalSemGive(NefHalHandle S);
EfiStatus NefHalSemDelete(NefHalHandle S);

/* Event (auto-reset) */
EfiStatus NefHalEventCreate(NefHalHandle* Out);
EfiStatus NefHalEventWait(NefHalHandle E, UINT32 TimeoutMs);
EfiStatus NefHalEventSignal(NefHalHandle E);
EfiStatus NefHalEventDelete(NefHalHandle E);

/* Time */
UINT64 NefHalGetTick(void);
EfiStatus NefHalDelayMs(UINT32 Ms);

/* Log */
void NefHalLog(const char* Fmt, va_list Ap);

/* GPIO */
EfiStatus NefHalPinSet(UINT8 Pin, UINT8 Val);
EfiStatus NefHalPinGet(UINT8 Pin, UINT8* Val);
EfiStatus NefHalPinMode(UINT8 Pin, UINT8 Mode);

/* Capability */
EfiStatus NefHalQueryCap(UINT32 Id, UINT32* Val);

/* Exec memory — host loader use only, not in service table */
void* NefHalAllocExec(NEF_SIZE Bytes);
void NefHalFreeExec(void* P, NEF_SIZE Bytes);

#endif /* NEF_HAL_H */
