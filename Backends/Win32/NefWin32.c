/*
 * backends/win32/NefWin32.c — NanoEFI Win32 Backend
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 NanoEFI Contributors
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "NefWin32.h"
#include "NefArch.h"

/*──────────────────────────────────────────────────────────────────────
 * Task
 *──────────────────────────────────────────────────────────────────────*/

typedef struct {
  NanoEfiTaskEntry Entry;
  VOID* Arg;
} TaskCtx;

static DWORD WINAPI TaskWrapper(LPVOID P) {
  TaskCtx* Ctx = (TaskCtx*)P;
  Ctx->Entry(Ctx->Arg);
  HeapFree(GetProcessHeap(), 0, Ctx);

  return 0;
}

static EfiStatus Win32CreateTask(IN NanoEfiTaskEntry Entry,
                  IN VOID* Arg,
                  IN UINT32 StackSize,
                  IN UINT32 Priority,
                  OUT NanoEfiTask* Out) {
  (void)Priority; // Make the compiler happy; Win32 doesn't support task priorities in this context
  if (!Entry || !Out)
    return NEF_EINVAL;

  TaskCtx* Ctx = HeapAlloc(GetProcessHeap(), 0, sizeof(*Ctx));
  if (!Ctx)
    return NEF_ENOMEM;
  Ctx->Entry = Entry;
  Ctx->Arg = Arg;

  HANDLE h = CreateThread(NULL, StackSize, TaskWrapper, Ctx, 0, NULL);
  if (!h) {
    HeapFree(GetProcessHeap(), 0, Ctx);

    return NEF_ENOMEM;
  }
  *Out = (NanoEfiTask)h;

  return NEF_OK;
}

static EfiStatus Win32DeleteTask(IN NanoEfiTask Task) {
  if (!Task)
    return NEF_EINVAL;
  TerminateThread((HANDLE)Task, 0);
  CloseHandle((HANDLE)Task);

  return NEF_OK;
}

static EfiStatus Win32TaskSleep(IN UINT32 Ms) {
  Sleep(Ms);

  return NEF_OK;
}

/*──────────────────────────────────────────────────────────────────────
 * Mutex  (NANO_EFI_TIMEOUT_FOREVER == INFINITE == 0xFFFFFFFF)
 *──────────────────────────────────────────────────────────────────────*/

static EfiStatus Win32CreateMutex(OUT NanoEfiMutex* Out) {
  if (!Out)
    return NEF_EINVAL;
  HANDLE h = CreateMutexA(NULL, FALSE, NULL);
  if (!h)
    return NEF_ENOMEM;
  *Out = (NanoEfiMutex)h;

  return NEF_OK;
}

static EfiStatus Win32LockMutex(IN NanoEfiMutex Mutex, IN UINT32 TimeoutMs) {
  if (!Mutex)
    return NEF_EINVAL;

  return WaitForSingleObject((HANDLE)Mutex, TimeoutMs) == WAIT_OBJECT_0
             ? NEF_OK
             : NEF_ETIMEDOUT;
}

static EfiStatus Win32UnlockMutex(IN NanoEfiMutex Mutex) {
  if (!Mutex)
    return NEF_EINVAL;
  ReleaseMutex((HANDLE)Mutex);

  return NEF_OK;
}

static EfiStatus Win32DeleteMutex(IN NanoEfiMutex Mutex) {
  if (!Mutex)
    return NEF_EINVAL;
  CloseHandle((HANDLE)Mutex);

  return NEF_OK;
}

/*──────────────────────────────────────────────────────────────────────
 * Semaphore
 *──────────────────────────────────────────────────────────────────────*/

static EfiStatus Win32CreateSemaphore(IN UINT32 Initial, IN UINT32 Max,
                                      OUT NanoEfiSemaphore* Out) {
  if (!Out || Initial > Max)
    return NEF_EINVAL;
  HANDLE h = CreateSemaphoreA(NULL, (LONG)Initial, (LONG)Max, NULL);
  if (!h)
    return NEF_ENOMEM;
  *Out = (NanoEfiSemaphore)h;

  return NEF_OK;
}

static EfiStatus Win32TakeSemaphore(IN NanoEfiSemaphore Sem,
                                    IN UINT32 TimeoutMs) {
  if (!Sem)
    return NEF_EINVAL;

  return WaitForSingleObject((HANDLE)Sem, TimeoutMs) == WAIT_OBJECT_0
             ? NEF_OK
             : NEF_ETIMEDOUT;
}

static EfiStatus Win32GiveSemaphore(IN NanoEfiSemaphore Sem) {
  if (!Sem)
    return NEF_EINVAL;
  ReleaseSemaphore((HANDLE)Sem, 1, NULL);

  return NEF_OK;
}

static EfiStatus Win32DeleteSemaphore(IN NanoEfiSemaphore Sem) {
  if (!Sem)
    return NEF_EINVAL;
  CloseHandle((HANDLE)Sem);

  return NEF_OK;
}

/*──────────────────────────────────────────────────────────────────────
 * Event (auto-reset)
 *──────────────────────────────────────────────────────────────────────*/

static EfiStatus Win32CreateEvent(OUT NanoEfiEvent* Out) {
  if (!Out)
    return NEF_EINVAL;
  HANDLE h = CreateEventA(NULL, FALSE, FALSE, NULL);
  if (!h)
    return NEF_ENOMEM;
  *Out = (NanoEfiEvent)h;

  return NEF_OK;
}

static EfiStatus Win32WaitEvent(IN NanoEfiEvent Ev, IN UINT32 TimeoutMs) {
  if (!Ev)
    return NEF_EINVAL;

  return WaitForSingleObject((HANDLE)Ev, TimeoutMs) == WAIT_OBJECT_0
             ? NEF_OK
             : NEF_ETIMEDOUT;
}

static EfiStatus Win32SignalEvent(IN NanoEfiEvent Ev) {
  if (!Ev)
    return NEF_EINVAL;
  SetEvent((HANDLE)Ev);

  return NEF_OK;
}

static EfiStatus Win32DeleteEvent(IN NanoEfiEvent Ev) {
  if (!Ev)
    return NEF_EINVAL;
  CloseHandle((HANDLE)Ev);

  return NEF_OK;
}

/*──────────────────────────────────────────────────────────────────────
 * Time / Log / GPIO / Capability
 *──────────────────────────────────────────────────────────────────────*/

static UINT64 Win32GetTick(void) { return (UINT64)GetTickCount64(); }

static EfiStatus Win32DelayMs(IN UINT32 Ms) {
  Sleep(Ms);

  return NEF_OK;
}

static void Win32Log(const char* Fmt, ...) {
  va_list ap;
  va_start(ap, Fmt);
  vprintf(Fmt, ap);
  va_end(ap);
}

static UINT8 GpioState[256];

static EfiStatus Win32PinSet(IN UINT8 Pin, IN UINT8 Value) {
  GpioState[Pin] = Value & 1u;
  printf("[GPIO] pin %u = %u\n", Pin, GpioState[Pin]);

  return NEF_OK;
}

static EfiStatus Win32PinGet(IN UINT8 Pin, OUT UINT8* Value) {
  if (!Value)
    return NEF_EINVAL;
  *Value = GpioState[Pin];

  return NEF_OK;
}

static EfiStatus Win32PinMode(IN UINT8 Pin, IN UINT8 Mode) {
  (void)Pin;
  (void)Mode;

  return NEF_OK;
}

static EfiStatus Win32QueryCapability(IN UINT32 Id, OUT UINT32* Value) {
  if (!Value)
    return NEF_EINVAL;
  switch (Id) {
    case NANO_EFI_CAP_FPU:
    case NANO_EFI_CAP_DSP:
    case NANO_EFI_CAP_MALLOC:
    case NANO_EFI_CAP_CACHE:
      *Value = 1u;
      break;
    default:
      *Value = 0u;
      break;
  }

  return NEF_OK;
}

/*──────────────────────────────────────────────────────────────────────
 * Constructor
 *──────────────────────────────────────────────────────────────────────*/

EfiStatus NanoEfiWin32Init(NanoEfiServiceTable* Svc) {
  if (!Svc)
    return NEF_EINVAL;
  memset(Svc, 0, sizeof(*Svc));
  Svc->Version = NANO_EFI_VERSION_CURRENT;
  Svc->CreateTask = Win32CreateTask;
  Svc->DeleteTask = Win32DeleteTask;
  Svc->TaskSleep = Win32TaskSleep;
  Svc->CreateMutex = Win32CreateMutex;
  Svc->LockMutex = Win32LockMutex;
  Svc->UnlockMutex = Win32UnlockMutex;
  Svc->DeleteMutex = Win32DeleteMutex;
  Svc->CreateSemaphore = Win32CreateSemaphore;
  Svc->TakeSemaphore = Win32TakeSemaphore;
  Svc->GiveSemaphore = Win32GiveSemaphore;
  Svc->DeleteSemaphore = Win32DeleteSemaphore;
  Svc->CreateEvent = Win32CreateEvent;
  Svc->WaitEvent = Win32WaitEvent;
  Svc->SignalEvent = Win32SignalEvent;
  Svc->DeleteEvent = Win32DeleteEvent;
  Svc->GetTick = Win32GetTick;
  Svc->DelayMs = Win32DelayMs;
  Svc->Log = Win32Log;
  Svc->PinSet = Win32PinSet;
  Svc->PinGet = Win32PinGet;
  Svc->PinMode = Win32PinMode;
  Svc->QueryCapability = Win32QueryCapability;
  Svc->Extension = NULL;

  return NEF_OK;
}
