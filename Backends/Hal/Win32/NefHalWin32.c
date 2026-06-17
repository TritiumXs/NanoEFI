/*
 * backends/hal/win32/NefHalWin32.c — Win32 HAL
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 NanoEFI Contributors
 */

#include <stdio.h>
#include <windows.h>

#include "../NefHal.h"
#include "NefArch.h"

/*──────────────────────────────────────────────────────────────────────
 * Lifecycle
 *──────────────────────────────────────────────────────────────────────*/

EfiStatus NefHalInit(void) { return NEF_OK; }

/*──────────────────────────────────────────────────────────────────────
 * Task
 *──────────────────────────────────────────────────────────────────────*/

typedef struct {
  NefHalTaskFn fn;
  void* arg;
} TaskCtx;

static DWORD WINAPI TaskWrapper(LPVOID p) {
  TaskCtx* c = p;
  c->fn(c->arg);
  HeapFree(GetProcessHeap(), 0, c);

  return 0;
}

EfiStatus NefHalTaskCreate(NefHalTaskFn Fn, void* Arg, UINT32 Stack,
                           UINT32 Prio, NefHalHandle* Out) {
  (void)Prio;
  if (!Fn || !Out)
    return NEF_EINVAL;
  TaskCtx* c = HeapAlloc(GetProcessHeap(), 0, sizeof(*c));
  if (!c)
    return NEF_ENOMEM;
  c->fn = Fn;
  c->arg = Arg;
  HANDLE h = CreateThread(NULL, Stack, TaskWrapper, c, 0, NULL);
  if (!h) {
    HeapFree(GetProcessHeap(), 0, c);

    return NEF_ENOMEM;
  }
  *Out = h;

  return NEF_OK;
}

EfiStatus NefHalTaskDelete(NefHalHandle Task) {
  if (!Task)
    return NEF_EINVAL;
  TerminateThread((HANDLE)Task, 0);
  CloseHandle((HANDLE)Task);

  return NEF_OK;
}

EfiStatus NefHalTaskSleep(UINT32 Ms) {
  Sleep(Ms);

  return NEF_OK;
}

/*──────────────────────────────────────────────────────────────────────
 * Mutex / Semaphore / Event  (Win32 HANDLEs map directly)
 *──────────────────────────────────────────────────────────────────────*/

EfiStatus NefHalMutexCreate(NefHalHandle* Out) {
  if (!Out)
    return NEF_EINVAL;
  HANDLE h = CreateMutexA(NULL, FALSE, NULL);
  if (!h)
    return NEF_ENOMEM;
  *Out = h;

  return NEF_OK;
}

EfiStatus NefHalMutexLock(NefHalHandle M, UINT32 TimeoutMs) {
  if (!M)
    return NEF_EINVAL;

  return WaitForSingleObject((HANDLE)M, TimeoutMs) == WAIT_OBJECT_0
             ? NEF_OK
             : NEF_ETIMEDOUT;
}

EfiStatus NefHalMutexUnlock(NefHalHandle M) {
  if (!M)
    return NEF_EINVAL;
  ReleaseMutex((HANDLE)M);

  return NEF_OK;
}

EfiStatus NefHalMutexDelete(NefHalHandle M) {
  if (!M)
    return NEF_EINVAL;
  CloseHandle((HANDLE)M);

  return NEF_OK;
}

EfiStatus NefHalSemCreate(UINT32 Initial, UINT32 Max, NefHalHandle* Out) {
  if (!Out || Initial > Max)
    return NEF_EINVAL;
  HANDLE h = CreateSemaphoreA(NULL, (LONG)Initial, (LONG)Max, NULL);
  if (!h)
    return NEF_ENOMEM;
  *Out = h;

  return NEF_OK;
}

EfiStatus NefHalSemTake(NefHalHandle S, UINT32 TimeoutMs) {
  if (!S)
    return NEF_EINVAL;

  return WaitForSingleObject((HANDLE)S, TimeoutMs) == WAIT_OBJECT_0
             ? NEF_OK
             : NEF_ETIMEDOUT;
}

EfiStatus NefHalSemGive(NefHalHandle S) {
  if (!S)
    return NEF_EINVAL;
  ReleaseSemaphore((HANDLE)S, 1, NULL);

  return NEF_OK;
}

EfiStatus NefHalSemDelete(NefHalHandle S) {
  if (!S)
    return NEF_EINVAL;
  CloseHandle((HANDLE)S);

  return NEF_OK;
}

EfiStatus NefHalEventCreate(NefHalHandle* Out) {
  if (!Out)
    return NEF_EINVAL;
  HANDLE h = CreateEventA(NULL, FALSE, FALSE, NULL); /* auto-reset */
  if (!h)
    return NEF_ENOMEM;
  *Out = h;

  return NEF_OK;
}

EfiStatus NefHalEventWait(NefHalHandle E, UINT32 TimeoutMs) {
  if (!E)
    return NEF_EINVAL;

  return WaitForSingleObject((HANDLE)E, TimeoutMs) == WAIT_OBJECT_0
             ? NEF_OK
             : NEF_ETIMEDOUT;
}

EfiStatus NefHalEventSignal(NefHalHandle E) {
  if (!E)
    return NEF_EINVAL;
  SetEvent((HANDLE)E);

  return NEF_OK;
}

EfiStatus NefHalEventDelete(NefHalHandle E) {
  if (!E)
    return NEF_EINVAL;
  CloseHandle((HANDLE)E);

  return NEF_OK;
}

/*──────────────────────────────────────────────────────────────────────
 * Time / Log / GPIO / Capability
 *──────────────────────────────────────────────────────────────────────*/

UINT64 NefHalGetTick(void) { return (UINT64)GetTickCount64(); }
EfiStatus NefHalDelayMs(UINT32 Ms) {
  Sleep(Ms);

  return NEF_OK;
}

void NefHalLog(const char* Fmt, va_list Ap) { vprintf(Fmt, Ap); }

static UINT8 GpioState[256];

EfiStatus NefHalPinSet(UINT8 Pin, UINT8 Val) {
  GpioState[Pin] = Val & 1u;
  printf("[GPIO] pin %u = %u\n", Pin, GpioState[Pin]);

  return NEF_OK;
}

EfiStatus NefHalPinGet(UINT8 Pin, UINT8* Val) {
  if (!Val)
    return NEF_EINVAL;
  *Val = GpioState[Pin];

  return NEF_OK;
}

EfiStatus NefHalPinMode(UINT8 Pin, UINT8 Mode) {
  (void)Pin;
  (void)Mode;

  return NEF_OK;
}

EfiStatus NefHalQueryCap(UINT32 Id, UINT32* Val) {
  if (!Val)
    return NEF_EINVAL;
  switch (Id) {
    case NANO_EFI_CAP_FPU:
    case NANO_EFI_CAP_DSP:
    case NANO_EFI_CAP_MALLOC:
    case NANO_EFI_CAP_CACHE:
      *Val = 1u;
      break;
    default:
      *Val = 0u;
      break;
  }

  return NEF_OK;
}

/*──────────────────────────────────────────────────────────────────────
 * Exec memory
 *──────────────────────────────────────────────────────────────────────*/

void* NefHalAllocExec(NEF_SIZE Bytes) {
  return VirtualAlloc(NULL, Bytes, MEM_COMMIT | MEM_RESERVE,
                      PAGE_EXECUTE_READWRITE);
}

void NefHalFreeExec(void* P, NEF_SIZE Bytes) {
  (void)Bytes;
  if (P)
    VirtualFree(P, 0, MEM_RELEASE);
}
