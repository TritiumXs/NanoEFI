/*
 * backends/linux/NefLinux.c — NanoEFI Linux Backend Implementation
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 NanoEFI Contributors
 *
 * Platform: Linux (POSIX).
 * Dependencies: pthreads, librt (clock_gettime), stdio.
 */

#include "NefLinux.h"

#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "NefArch.h"

/*──────────────────────────────────────────────────────────────────────
 * Internal Object Types
 *──────────────────────────────────────────────────────────────────────*/

typedef struct {
  pthread_t Thread;
  NanoEfiTaskEntry Entry;
  void* Arg;
} TaskObj;

typedef struct {
  pthread_mutex_t Lock;
} MutexObj;

typedef struct {
  pthread_mutex_t Lock;
  pthread_cond_t Cond;
  unsigned Count;
  unsigned Max;
} SemObj;

typedef struct {
  pthread_mutex_t Lock;
  pthread_cond_t Cond;
  int Signaled;
} EventObj;

/*──────────────────────────────────────────────────────────────────────
 * Mock GPIO state
 *──────────────────────────────────────────────────────────────────────*/

static UINT8 GpioState[256];

/*──────────────────────────────────────────────────────────────────────
 * Helpers
 *──────────────────────────────────────────────────────────────────────*/

static void* TaskWrapper(void* Arg) {
  TaskObj* T = (TaskObj*)Arg;
  T->Entry(T->Arg);

  return NULL;
}

/* Convert NanoEFI timeout to POSIX timespec (absolute deadline) */
static int MakeDeadline(UINT32 TimeoutMs, struct timespec* Out) {
  if (clock_gettime(CLOCK_REALTIME, Out) != 0)
    return -1;
  Out->tv_sec += (time_t)(TimeoutMs / 1000u);
  Out->tv_nsec += (long)(TimeoutMs % 1000u) * 1000000L;
  if (Out->tv_nsec >= 1000000000L) {
    Out->tv_sec++;
    Out->tv_nsec -= 1000000000L;
  }

  return 0;
}

/*──────────────────────────────────────────────────────────────────────
 * Task
 *──────────────────────────────────────────────────────────────────────*/

static EfiStatus LinuxCreateTask(IN NanoEfiTaskEntry Entry, IN VOID* Arg,
                                 IN UINT32 StackSize, IN UINT32 Priority,
                                 OUT NanoEfiTask* Out) {
  (void)Priority;
  if (!Entry || !Out)
    return NEF_EINVAL;

  TaskObj* T = malloc(sizeof(*T));
  if (!T)
    return NEF_ENOMEM;
  T->Entry = Entry;
  T->Arg = Arg;

  pthread_attr_t Attr;
  pthread_attr_init(&Attr);
  if (StackSize)
    pthread_attr_setstacksize(&Attr, StackSize);

  int Rc = pthread_create(&T->Thread, &Attr, TaskWrapper, T);
  pthread_attr_destroy(&Attr);
  if (Rc) {
    free(T);

    return NEF_ENOMEM;
  }

  *Out = (NanoEfiTask)T;

  return NEF_OK;
}

static EfiStatus LinuxDeleteTask(IN NanoEfiTask Task) {
  if (!Task)
    return NEF_EINVAL;
  TaskObj* T = (TaskObj*)Task;
  pthread_cancel(T->Thread);
  pthread_join(T->Thread, NULL);
  free(T);

  return NEF_OK;
}

static EfiStatus LinuxTaskSleep(IN UINT32 Ms) {
  usleep((useconds_t)Ms * 1000u);

  return NEF_OK;
}

/*──────────────────────────────────────────────────────────────────────
 * Mutex
 *──────────────────────────────────────────────────────────────────────*/

static EfiStatus LinuxCreateMutex(OUT NanoEfiMutex* Out) {
  if (!Out)
    return NEF_EINVAL;
  MutexObj* M = malloc(sizeof(*M));
  if (!M)
    return NEF_ENOMEM;
  pthread_mutex_init(&M->Lock, NULL);
  *Out = (NanoEfiMutex)M;

  return NEF_OK;
}

static EfiStatus LinuxLockMutex(IN NanoEfiMutex Mutex, IN UINT32 TimeoutMs) {
  if (!Mutex)
    return NEF_EINVAL;
  MutexObj* M = (MutexObj*)Mutex;

  if (TimeoutMs == NANO_EFI_TIMEOUT_POLL)
    return pthread_mutex_trylock(&M->Lock) ? NEF_ETIMEDOUT : NEF_OK;

  if (TimeoutMs == NANO_EFI_TIMEOUT_FOREVER) {
    pthread_mutex_lock(&M->Lock);

    return NEF_OK;
  }

  struct timespec Ts;
  if (MakeDeadline(TimeoutMs, &Ts))
    return NEF_EINVAL;

  return pthread_mutex_timedlock(&M->Lock, &Ts) == ETIMEDOUT ? NEF_ETIMEDOUT
                                                             : NEF_OK;
}

static EfiStatus LinuxUnlockMutex(IN NanoEfiMutex Mutex) {
  if (!Mutex)
    return NEF_EINVAL;
  pthread_mutex_unlock(&((MutexObj*)Mutex)->Lock);

  return NEF_OK;
}

static EfiStatus LinuxDeleteMutex(IN NanoEfiMutex Mutex) {
  if (!Mutex)
    return NEF_EINVAL;
  MutexObj* M = (MutexObj*)Mutex;
  pthread_mutex_destroy(&M->Lock);
  free(M);

  return NEF_OK;
}

/*──────────────────────────────────────────────────────────────────────
 * Semaphore
 *──────────────────────────────────────────────────────────────────────*/

static EfiStatus LinuxCreateSemaphore(IN UINT32 Initial, IN UINT32 Max,
                                      OUT NanoEfiSemaphore* Out) {
  if (!Out || Initial > Max)
    return NEF_EINVAL;
  SemObj* S = malloc(sizeof(*S));
  if (!S)
    return NEF_ENOMEM;
  pthread_mutex_init(&S->Lock, NULL);
  pthread_cond_init(&S->Cond, NULL);
  S->Count = Initial;
  S->Max = Max;
  *Out = (NanoEfiSemaphore)S;

  return NEF_OK;
}

static EfiStatus LinuxTakeSemaphore(IN NanoEfiSemaphore Semaphore,
                                    IN UINT32 TimeoutMs) {
  if (!Semaphore)
    return NEF_EINVAL;
  SemObj* S = (SemObj*)Semaphore;
  EfiStatus St = NEF_OK;

  pthread_mutex_lock(&S->Lock);

  if (TimeoutMs == NANO_EFI_TIMEOUT_POLL) {
    St = S->Count ? NEF_OK : NEF_ETIMEDOUT;
    if (!EFI_ERROR(St))
      S->Count--;

  } else if (TimeoutMs == NANO_EFI_TIMEOUT_FOREVER) {
    while (!S->Count)
      pthread_cond_wait(&S->Cond, &S->Lock);
    S->Count--;

  } else {
    struct timespec Ts;
    MakeDeadline(TimeoutMs, &Ts);
    while (!S->Count && St == NEF_OK) {
      int R = pthread_cond_timedwait(&S->Cond, &S->Lock, &Ts);
      if (R == ETIMEDOUT)
        St = NEF_ETIMEDOUT;
    }
    if (!EFI_ERROR(St))
      S->Count--;
  }

  pthread_mutex_unlock(&S->Lock);

  return St;
}

static EfiStatus LinuxGiveSemaphore(IN NanoEfiSemaphore Semaphore) {
  if (!Semaphore)
    return NEF_EINVAL;
  SemObj* S = (SemObj*)Semaphore;
  pthread_mutex_lock(&S->Lock);
  if (S->Count < S->Max) {
    S->Count++;
    pthread_cond_signal(&S->Cond);
  }
  pthread_mutex_unlock(&S->Lock);

  return NEF_OK;
}

static EfiStatus LinuxDeleteSemaphore(IN NanoEfiSemaphore Semaphore) {
  if (!Semaphore)
    return NEF_EINVAL;
  SemObj* S = (SemObj*)Semaphore;
  pthread_cond_destroy(&S->Cond);
  pthread_mutex_destroy(&S->Lock);
  free(S);

  return NEF_OK;
}

/*──────────────────────────────────────────────────────────────────────
 * Event
 *──────────────────────────────────────────────────────────────────────*/

static EfiStatus LinuxCreateEvent(OUT NanoEfiEvent* Out) {
  if (!Out)
    return NEF_EINVAL;
  EventObj* E = malloc(sizeof(*E));
  if (!E)
    return NEF_ENOMEM;
  pthread_mutex_init(&E->Lock, NULL);
  pthread_cond_init(&E->Cond, NULL);
  E->Signaled = 0;
  *Out = (NanoEfiEvent)E;

  return NEF_OK;
}

static EfiStatus LinuxWaitEvent(IN NanoEfiEvent Event, IN UINT32 TimeoutMs) {
  if (!Event)
    return NEF_EINVAL;
  EventObj* E = (EventObj*)Event;
  EfiStatus St = NEF_OK;

  pthread_mutex_lock(&E->Lock);

  if (TimeoutMs == NANO_EFI_TIMEOUT_POLL) {
    St = E->Signaled ? NEF_OK : NEF_ETIMEDOUT;

  } else if (TimeoutMs == NANO_EFI_TIMEOUT_FOREVER) {
    while (!E->Signaled)
      pthread_cond_wait(&E->Cond, &E->Lock);

  } else {
    struct timespec Ts;
    MakeDeadline(TimeoutMs, &Ts);
    while (!E->Signaled && St == NEF_OK) {
      int R = pthread_cond_timedwait(&E->Cond, &E->Lock, &Ts);
      if (R == ETIMEDOUT)
        St = NEF_ETIMEDOUT;
    }
  }

  if (!EFI_ERROR(St))
    E->Signaled = 0; /* auto-reset */
  pthread_mutex_unlock(&E->Lock);

  return St;
}

static EfiStatus LinuxSignalEvent(IN NanoEfiEvent Event) {
  if (!Event)
    return NEF_EINVAL;
  EventObj* E = (EventObj*)Event;
  pthread_mutex_lock(&E->Lock);
  E->Signaled = 1;
  pthread_cond_signal(&E->Cond);
  pthread_mutex_unlock(&E->Lock);

  return NEF_OK;
}

static EfiStatus LinuxDeleteEvent(IN NanoEfiEvent Event) {
  if (!Event)
    return NEF_EINVAL;
  EventObj* E = (EventObj*)Event;
  pthread_cond_destroy(&E->Cond);
  pthread_mutex_destroy(&E->Lock);
  free(E);

  return NEF_OK;
}

/*──────────────────────────────────────────────────────────────────────
 * Time
 *──────────────────────────────────────────────────────────────────────*/

static UINT64 LinuxGetTick(void) {
  struct timespec Ts;
  clock_gettime(CLOCK_MONOTONIC, &Ts);

  return (UINT64)Ts.tv_sec * 1000ULL + (UINT64)Ts.tv_nsec / 1000000ULL;
}

static EfiStatus LinuxDelayMs(IN UINT32 Ms) {
  usleep((useconds_t)Ms * 1000u);

  return NEF_OK;
}

/*──────────────────────────────────────────────────────────────────────
 * Logging
 *──────────────────────────────────────────────────────────────────────*/

static void LinuxLog(const char* Fmt, ...) {
  va_list Ap;
  va_start(Ap, Fmt);
  vprintf(Fmt, Ap);
  va_end(Ap);
}

/*──────────────────────────────────────────────────────────────────────
 * GPIO (mock — state tracked in array, changes printed to stdout)
 *──────────────────────────────────────────────────────────────────────*/

static EfiStatus LinuxPinSet(IN UINT8 Pin, IN UINT8 Value) {
  GpioState[Pin] = Value & 1u;
  printf("[GPIO] pin %u = %u\n", Pin, GpioState[Pin]);

  return NEF_OK;
}

static EfiStatus LinuxPinGet(IN UINT8 Pin, OUT UINT8* Value) {
  if (!Value)
    return NEF_EINVAL;
  *Value = GpioState[Pin];

  return NEF_OK;
}

static EfiStatus LinuxPinMode(IN UINT8 Pin, IN UINT8 Mode) {
  (void)Pin;
  (void)Mode; /* direction is implicit on Linux mock */

  return NEF_OK;
}

/*──────────────────────────────────────────────────────────────────────
 * Capability Query
 *──────────────────────────────────────────────────────────────────────*/

static EfiStatus LinuxQueryCapability(IN UINT32 Id, OUT UINT32* Value) {
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

EfiStatus NanoEfiLinuxInit(NanoEfiServiceTable* Svc) {
  if (!Svc)
    return NEF_EINVAL;
  memset(Svc, 0, sizeof(*Svc));

  Svc->Version = NANO_EFI_VERSION_CURRENT;

  Svc->CreateTask = LinuxCreateTask;
  Svc->DeleteTask = LinuxDeleteTask;
  Svc->TaskSleep = LinuxTaskSleep;

  Svc->CreateMutex = LinuxCreateMutex;
  Svc->LockMutex = LinuxLockMutex;
  Svc->UnlockMutex = LinuxUnlockMutex;
  Svc->DeleteMutex = LinuxDeleteMutex;

  Svc->CreateSemaphore = LinuxCreateSemaphore;
  Svc->TakeSemaphore = LinuxTakeSemaphore;
  Svc->GiveSemaphore = LinuxGiveSemaphore;
  Svc->DeleteSemaphore = LinuxDeleteSemaphore;

  Svc->CreateEvent = LinuxCreateEvent;
  Svc->WaitEvent = LinuxWaitEvent;
  Svc->SignalEvent = LinuxSignalEvent;
  Svc->DeleteEvent = LinuxDeleteEvent;

  Svc->GetTick = LinuxGetTick;
  Svc->DelayMs = LinuxDelayMs;

  Svc->Log = LinuxLog;

  Svc->PinSet = LinuxPinSet;
  Svc->PinGet = LinuxPinGet;
  Svc->PinMode = LinuxPinMode;

  Svc->QueryCapability = LinuxQueryCapability;

  Svc->Extension = NULL;

  return NEF_OK;
}
