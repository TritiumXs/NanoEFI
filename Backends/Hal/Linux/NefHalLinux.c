/*
 * backends/hal/linux/NefHalLinux.c — Linux HAL (pthreads + POSIX)
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 NanoEFI Contributors
 */

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include "../NefHal.h"
#include "NefArch.h"

/*──────────────────────────────────────────────────────────────────────
 * Internal types
 *──────────────────────────────────────────────────────────────────────*/

typedef struct {
  pthread_t t;
  NefHalTaskFn fn;
  void* arg;
} TaskObj;
typedef struct {
  pthread_mutex_t m;
} MutexObj;
typedef struct {
  pthread_mutex_t m;
  pthread_cond_t c;
  unsigned cnt, max;
} SemObj;
typedef struct {
  pthread_mutex_t m;
  pthread_cond_t c;
  int sig;
} EventObj;

static UINT8 GpioState[256];

static int MakeDeadline(UINT32 Ms, struct timespec* ts) {
  if (clock_gettime(CLOCK_REALTIME, ts))
    return -1;
  ts->tv_sec += (time_t)(Ms / 1000u);
  ts->tv_nsec += (long)(Ms % 1000u) * 1000000L;
  if (ts->tv_nsec >= 1000000000L) {
    ts->tv_sec++;
    ts->tv_nsec -= 1000000000L;
  }

  return 0;
}

/*──────────────────────────────────────────────────────────────────────
 * Lifecycle
 *──────────────────────────────────────────────────────────────────────*/

EfiStatus NefHalInit(void) { return NEF_OK; }

/*──────────────────────────────────────────────────────────────────────
 * Task
 *──────────────────────────────────────────────────────────────────────*/

static void* TaskWrapper(void* p) {
  TaskObj* t = p;
  t->fn(t->arg);

  return NULL;
}

EfiStatus NefHalTaskCreate(NefHalTaskFn Fn, void* Arg, UINT32 Stack,
                           UINT32 Prio, NefHalHandle* Out) {
  (void)Prio;
  if (!Fn || !Out)
    return NEF_EINVAL;
  TaskObj* t = malloc(sizeof(*t));
  if (!t)
    return NEF_ENOMEM;
  t->fn = Fn;
  t->arg = Arg;
  pthread_attr_t a;
  pthread_attr_init(&a);
  if (Stack)
    pthread_attr_setstacksize(&a, Stack);
  int rc = pthread_create(&t->t, &a, TaskWrapper, t);
  pthread_attr_destroy(&a);
  if (rc) {
    free(t);

    return NEF_ENOMEM;
  }
  *Out = t;

  return NEF_OK;
}

EfiStatus NefHalTaskDelete(NefHalHandle Task) {
  if (!Task)
    return NEF_EINVAL;
  TaskObj* t = Task;
  pthread_cancel(t->t);
  pthread_join(t->t, NULL);
  free(t);

  return NEF_OK;
}

EfiStatus NefHalTaskSleep(UINT32 Ms) {
  usleep((useconds_t)Ms * 1000u);

  return NEF_OK;
}

/*──────────────────────────────────────────────────────────────────────
 * Mutex
 *──────────────────────────────────────────────────────────────────────*/

EfiStatus NefHalMutexCreate(NefHalHandle* Out) {
  if (!Out)
    return NEF_EINVAL;
  MutexObj* m = malloc(sizeof(*m));
  if (!m)
    return NEF_ENOMEM;
  pthread_mutex_init(&m->m, NULL);
  *Out = m;

  return NEF_OK;
}

EfiStatus NefHalMutexLock(NefHalHandle M, UINT32 TimeoutMs) {
  if (!M)
    return NEF_EINVAL;
  MutexObj* m = M;
  if (TimeoutMs == NANO_EFI_TIMEOUT_POLL)
    return pthread_mutex_trylock(&m->m) ? NEF_ETIMEDOUT : NEF_OK;
  if (TimeoutMs == NANO_EFI_TIMEOUT_FOREVER)
    return pthread_mutex_lock(&m->m) ? NEF_EINVAL : NEF_OK;
  struct timespec ts;
  MakeDeadline(TimeoutMs, &ts);

  return pthread_mutex_timedlock(&m->m, &ts) == ETIMEDOUT ? NEF_ETIMEDOUT
                                                          : NEF_OK;
}

EfiStatus NefHalMutexUnlock(NefHalHandle M) {
  if (!M)
    return NEF_EINVAL;
  pthread_mutex_unlock(&((MutexObj*)M)->m);

  return NEF_OK;
}

EfiStatus NefHalMutexDelete(NefHalHandle M) {
  if (!M)
    return NEF_EINVAL;
  MutexObj* m = M;
  pthread_mutex_destroy(&m->m);
  free(m);

  return NEF_OK;
}

/*──────────────────────────────────────────────────────────────────────
 * Semaphore
 *──────────────────────────────────────────────────────────────────────*/

EfiStatus NefHalSemCreate(UINT32 Initial, UINT32 Max, NefHalHandle* Out) {
  if (!Out || Initial > Max)
    return NEF_EINVAL;
  SemObj* s = malloc(sizeof(*s));
  if (!s)
    return NEF_ENOMEM;
  pthread_mutex_init(&s->m, NULL);
  pthread_cond_init(&s->c, NULL);
  s->cnt = Initial;
  s->max = Max;
  *Out = s;

  return NEF_OK;
}

EfiStatus NefHalSemTake(NefHalHandle S, UINT32 TimeoutMs) {
  if (!S)
    return NEF_EINVAL;
  SemObj* s = S;
  EfiStatus st = NEF_OK;
  pthread_mutex_lock(&s->m);
  if (TimeoutMs == NANO_EFI_TIMEOUT_POLL) {
    st = s->cnt ? NEF_OK : NEF_ETIMEDOUT;
    if (!EFI_ERROR(st))
      s->cnt--;
  } else if (TimeoutMs == NANO_EFI_TIMEOUT_FOREVER) {
    while (!s->cnt)
      pthread_cond_wait(&s->c, &s->m);
    s->cnt--;
  } else {
    struct timespec ts;
    MakeDeadline(TimeoutMs, &ts);
    while (!s->cnt && !EFI_ERROR(st))
      if (pthread_cond_timedwait(&s->c, &s->m, &ts) == ETIMEDOUT)
        st = NEF_ETIMEDOUT;
    if (!EFI_ERROR(st))
      s->cnt--;
  }
  pthread_mutex_unlock(&s->m);

  return st;
}

EfiStatus NefHalSemGive(NefHalHandle S) {
  if (!S)
    return NEF_EINVAL;
  SemObj* s = S;
  pthread_mutex_lock(&s->m);
  if (s->cnt < s->max) {
    s->cnt++;
    pthread_cond_signal(&s->c);
  }
  pthread_mutex_unlock(&s->m);

  return NEF_OK;
}

EfiStatus NefHalSemDelete(NefHalHandle S) {
  if (!S)
    return NEF_EINVAL;
  SemObj* s = S;
  pthread_cond_destroy(&s->c);
  pthread_mutex_destroy(&s->m);
  free(s);

  return NEF_OK;
}

/*──────────────────────────────────────────────────────────────────────
 * Event
 *──────────────────────────────────────────────────────────────────────*/

EfiStatus NefHalEventCreate(NefHalHandle* Out) {
  if (!Out)
    return NEF_EINVAL;
  EventObj* e = malloc(sizeof(*e));
  if (!e)
    return NEF_ENOMEM;
  pthread_mutex_init(&e->m, NULL);
  pthread_cond_init(&e->c, NULL);
  e->sig = 0;
  *Out = e;

  return NEF_OK;
}

EfiStatus NefHalEventWait(NefHalHandle E, UINT32 TimeoutMs) {
  if (!E)
    return NEF_EINVAL;
  EventObj* e = E;
  EfiStatus st = NEF_OK;
  pthread_mutex_lock(&e->m);
  if (TimeoutMs == NANO_EFI_TIMEOUT_POLL) {
    st = e->sig ? NEF_OK : NEF_ETIMEDOUT;
  } else if (TimeoutMs == NANO_EFI_TIMEOUT_FOREVER) {
    while (!e->sig)
      pthread_cond_wait(&e->c, &e->m);
  } else {
    struct timespec ts;
    MakeDeadline(TimeoutMs, &ts);
    while (!e->sig && !EFI_ERROR(st))
      if (pthread_cond_timedwait(&e->c, &e->m, &ts) == ETIMEDOUT)
        st = NEF_ETIMEDOUT;
  }
  if (!EFI_ERROR(st))
    e->sig = 0;
  pthread_mutex_unlock(&e->m);

  return st;
}

EfiStatus NefHalEventSignal(NefHalHandle E) {
  if (!E)
    return NEF_EINVAL;
  EventObj* e = E;
  pthread_mutex_lock(&e->m);
  e->sig = 1;
  pthread_cond_signal(&e->c);
  pthread_mutex_unlock(&e->m);

  return NEF_OK;
}

EfiStatus NefHalEventDelete(NefHalHandle E) {
  if (!E)
    return NEF_EINVAL;
  EventObj* e = E;
  pthread_cond_destroy(&e->c);
  pthread_mutex_destroy(&e->m);
  free(e);

  return NEF_OK;
}

/*──────────────────────────────────────────────────────────────────────
 * Time / Log / GPIO / Capability
 *──────────────────────────────────────────────────────────────────────*/

UINT64 NefHalGetTick(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);

  return (UINT64)ts.tv_sec * 1000ULL + (UINT64)ts.tv_nsec / 1000000ULL;
}

EfiStatus NefHalDelayMs(UINT32 Ms) {
  usleep((useconds_t)Ms * 1000u);

  return NEF_OK;
}

void NefHalLog(const char* Fmt, va_list Ap) { vprintf(Fmt, Ap); }

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
  void* p = mmap(NULL, Bytes, PROT_READ | PROT_WRITE | PROT_EXEC,
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  return (p == MAP_FAILED) ? NULL : p;
}

void NefHalFreeExec(void* P, NEF_SIZE Bytes) {
  if (P)
    munmap(P, Bytes);
}
