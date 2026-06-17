/*
 * backends/NefBackend.c — Unified NanoEFI Backend (HAL-backed)
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 NanoEFI Contributors
 *
 * Wires NefHal_* calls into NanoEfiServiceTable function pointers.
 * Platform selection happens at link time via cmake/NanoEFI.cmake.
 */

#include "NefBackend.h"

#include <stdarg.h>
#include <string.h>

#include "Hal/NefHal.h"

/*──────────────────────────────────────────────────────────────────────
 * Service table shims — add NEF_CALLCONV for ABI bridge
 *──────────────────────────────────────────────────────────────────────*/

static EfiStatus NEF_CALLCONV BkCreateTask(IN NanoEfiTaskEntry Entry,
                                           IN VOID* Arg, IN UINT32 Stack,
                                           IN UINT32 Prio,
                                           OUT NanoEfiTask* Out) {
  return NefHalTaskCreate((NefHalTaskFn)Entry, Arg, Stack, Prio,
                          (NefHalHandle*)Out);
}

static EfiStatus NEF_CALLCONV BkDeleteTask(IN NanoEfiTask Task) {
  return NefHalTaskDelete((NefHalHandle)Task);
}

static EfiStatus NEF_CALLCONV BkTaskSleep(IN UINT32 Ms) {
  return NefHalTaskSleep(Ms);
}

static EfiStatus NEF_CALLCONV BkCreateMutex(OUT NanoEfiMutex* Out) {
  return NefHalMutexCreate((NefHalHandle*)Out);
}

static EfiStatus NEF_CALLCONV BkLockMutex(IN NanoEfiMutex M, IN UINT32 Tmo) {
  return NefHalMutexLock((NefHalHandle)M, Tmo);
}

static EfiStatus NEF_CALLCONV BkUnlockMutex(IN NanoEfiMutex M) {
  return NefHalMutexUnlock((NefHalHandle)M);
}

static EfiStatus NEF_CALLCONV BkDeleteMutex(IN NanoEfiMutex M) {
  return NefHalMutexDelete((NefHalHandle)M);
}

static EfiStatus NEF_CALLCONV BkCreateSemaphore(IN UINT32 Init, IN UINT32 Max,
                                                OUT NanoEfiSemaphore* Out) {
  return NefHalSemCreate(Init, Max, (NefHalHandle*)Out);
}

static EfiStatus NEF_CALLCONV BkTakeSemaphore(IN NanoEfiSemaphore S,
                                              IN UINT32 Tmo) {
  return NefHalSemTake((NefHalHandle)S, Tmo);
}

static EfiStatus NEF_CALLCONV BkGiveSemaphore(IN NanoEfiSemaphore S) {
  return NefHalSemGive((NefHalHandle)S);
}

static EfiStatus NEF_CALLCONV BkDeleteSemaphore(IN NanoEfiSemaphore S) {
  return NefHalSemDelete((NefHalHandle)S);
}

static EfiStatus NEF_CALLCONV BkCreateEvent(OUT NanoEfiEvent* Out) {
  return NefHalEventCreate((NefHalHandle*)Out);
}

static EfiStatus NEF_CALLCONV BkWaitEvent(IN NanoEfiEvent E, IN UINT32 Tmo) {
  return NefHalEventWait((NefHalHandle)E, Tmo);
}

static EfiStatus NEF_CALLCONV BkSignalEvent(IN NanoEfiEvent E) {
  return NefHalEventSignal((NefHalHandle)E);
}

static EfiStatus NEF_CALLCONV BkDeleteEvent(IN NanoEfiEvent E) {
  return NefHalEventDelete((NefHalHandle)E);
}

static UINT64 NEF_CALLCONV BkGetTick(VOID) { return NefHalGetTick(); }

static EfiStatus NEF_CALLCONV BkDelayMs(IN UINT32 Ms) {
  return NefHalDelayMs(Ms);
}

static VOID NEF_CALLCONV BkLog(IN const char* Fmt, ...) {
  va_list ap;
  va_start(ap, Fmt);
  NefHalLog(Fmt, ap);
  va_end(ap);
}

static EfiStatus NEF_CALLCONV BkPinSet(IN UINT8 Pin, IN UINT8 Val) {
  return NefHalPinSet(Pin, Val);
}

static EfiStatus NEF_CALLCONV BkPinGet(IN UINT8 Pin, OUT UINT8* Val) {
  return NefHalPinGet(Pin, Val);
}

static EfiStatus NEF_CALLCONV BkPinMode(IN UINT8 Pin, IN UINT8 Mode) {
  return NefHalPinMode(Pin, Mode);
}

static EfiStatus NEF_CALLCONV BkQueryCapability(IN UINT32 Id, OUT UINT32* Val) {
  return NefHalQueryCap(Id, Val);
}

/*──────────────────────────────────────────────────────────────────────
 * Public API
 *──────────────────────────────────────────────────────────────────────*/

EfiStatus NanoEfiBackendInit(NanoEfiServiceTable* Svc) {
  EfiStatus s;
  if (!Svc)
    return NEF_EINVAL;
  s = NefHalInit();
  if (EFI_ERROR(s))
    return s;

  memset(Svc, 0, sizeof(*Svc));
  Svc->Version = NANO_EFI_VERSION_CURRENT;
  Svc->CreateTask = BkCreateTask;
  Svc->DeleteTask = BkDeleteTask;
  Svc->TaskSleep = BkTaskSleep;
  Svc->CreateMutex = BkCreateMutex;
  Svc->LockMutex = BkLockMutex;
  Svc->UnlockMutex = BkUnlockMutex;
  Svc->DeleteMutex = BkDeleteMutex;
  Svc->CreateSemaphore = BkCreateSemaphore;
  Svc->TakeSemaphore = BkTakeSemaphore;
  Svc->GiveSemaphore = BkGiveSemaphore;
  Svc->DeleteSemaphore = BkDeleteSemaphore;
  Svc->CreateEvent = BkCreateEvent;
  Svc->WaitEvent = BkWaitEvent;
  Svc->SignalEvent = BkSignalEvent;
  Svc->DeleteEvent = BkDeleteEvent;
  Svc->GetTick = BkGetTick;
  Svc->DelayMs = BkDelayMs;
  Svc->Log = BkLog;
  Svc->PinSet = BkPinSet;
  Svc->PinGet = BkPinGet;
  Svc->PinMode = BkPinMode;
  Svc->QueryCapability = BkQueryCapability;
  Svc->Extension = NULL;

  return NEF_OK;
}

void* NefAllocExec(NEF_SIZE Bytes) { return NefHalAllocExec(Bytes); }
void NefFreeExec(void* P, NEF_SIZE Bytes) { NefHalFreeExec(P, Bytes); }
