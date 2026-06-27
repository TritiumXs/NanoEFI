/*
 * Backends/Hal/Baremetal/NefHalBaremetal.c — Portable baremetal HAL
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 NanoEFI Contributors
 *
 * Implements NefHal.h using:
 *   - Static memory pools (no heap required)
 *   - Busy-wait / spin for Mutex, Semaphore, Event
 *   - Board abstraction via NefBoard.h
 *
 * Task: baremetal is cooperative/single-threaded. CreateTask runs the
 * function synchronously (blocking). Callers that need real concurrency
 * should load an RTOS driver image instead.
 */

#include <stdarg.h>
#include <stddef.h>

#include "../NefHal.h"
#include "NefArch.h"
#include "NefBoard.h"
#include "NefFormat.h"

/*──────────────────────────────────────────────────────────────────────
 * Pool sizes — tune via -DNEF_POOL_* at compile time
 *──────────────────────────────────────────────────────────────────────*/
#ifndef NEF_POOL_TASKS
#define NEF_POOL_TASKS  4
#endif
#ifndef NEF_POOL_MUTEX
#define NEF_POOL_MUTEX  8
#endif
#ifndef NEF_POOL_SEM
#define NEF_POOL_SEM    8
#endif
#ifndef NEF_POOL_EVENT
#define NEF_POOL_EVENT  8
#endif

/*──────────────────────────────────────────────────────────────────────
 * Internal types
 *──────────────────────────────────────────────────────────────────────*/
typedef struct { UINT8 Used; NefHalTaskFn Fn; void *Arg; } TaskObj;
typedef struct { UINT8 Used; UINT8 Locked; } MutexObj;
typedef struct { UINT8 Used; UINT32 Cnt; UINT32 Max; } SemObj;
typedef struct { UINT8 Used; UINT8 Sig; } EventObj;

static TaskObj  STaskPool[NEF_POOL_TASKS];
static MutexObj SMutexPool[NEF_POOL_MUTEX];
static SemObj   SSemPool[NEF_POOL_SEM];
static EventObj SEventPool[NEF_POOL_EVENT];

#define ALLOC_FROM_POOL(Pool, Out) do {                 \
    UINT32 _s = NefBoardEnterCritical();                \
    for (UINT32 _i = 0; _i < NEF_ARRAY_SIZE(Pool); _i++) { \
        if (!(Pool)[_i].Used) {                         \
            (Pool)[_i].Used = 1;                        \
            *(Out) = &(Pool)[_i];                       \
            NefBoardExitCritical(_s);                   \
            goto _alloc_ok;                             \
        }                                               \
    }                                                   \
    NefBoardExitCritical(_s);                           \
    return NEF_ENOMEM;                                  \
    _alloc_ok:;                                         \
} while (0)

#ifndef NEF_ARRAY_SIZE
#define NEF_ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))
#endif

/*──────────────────────────────────────────────────────────────────────
 * Lifecycle
 *──────────────────────────────────────────────────────────────────────*/
EfiStatus NefHalInit(void) {
    NefBoardInit();
    return NEF_OK;
}

/*──────────────────────────────────────────────────────────────────────
 * Task — cooperative: runs fn() synchronously
 *──────────────────────────────────────────────────────────────────────*/
EfiStatus NefHalTaskCreate(NefHalTaskFn Fn, void *Arg, UINT32 Stack,
                           UINT32 Prio, NefHalHandle *Out) {
    (void)Stack; (void)Prio;
    if (!Fn || !Out) return NEF_EINVAL;
    TaskObj *T;
    ALLOC_FROM_POOL(STaskPool, &T);
    T->Fn = Fn; T->Arg = Arg;
    *Out = T;
    Fn(Arg);   /* cooperative: run to completion */
    return NEF_OK;
}

EfiStatus NefHalTaskDelete(NefHalHandle Task) {
    if (!Task) return NEF_EINVAL;
    ((TaskObj *)Task)->Used = 0;
    return NEF_OK;
}

EfiStatus NefHalTaskSleep(UINT32 Ms) {
    NefBoardDelayMs(Ms);
    return NEF_OK;
}

/*──────────────────────────────────────────────────────────────────────
 * Mutex — spin-wait with timeout
 *──────────────────────────────────────────────────────────────────────*/
EfiStatus NefHalMutexCreate(NefHalHandle *Out) {
    if (!Out) return NEF_EINVAL;
    MutexObj *M;
    ALLOC_FROM_POOL(SMutexPool, &M);
    M->Locked = 0;
    *Out = M;
    return NEF_OK;
}

EfiStatus NefHalMutexLock(NefHalHandle M, UINT32 TimeoutMs) {
    if (!M) return NEF_EINVAL;
    MutexObj *m = M;
    UINT64 Deadline = NefBoardGetTickMs() + TimeoutMs;
    while (1) {
        UINT32 S = NefBoardEnterCritical();
        if (!m->Locked) { m->Locked = 1; NefBoardExitCritical(S); return NEF_OK; }
        NefBoardExitCritical(S);
        if (TimeoutMs == NANO_EFI_TIMEOUT_POLL)    return NEF_ETIMEDOUT;
        if (TimeoutMs != NANO_EFI_TIMEOUT_FOREVER &&
            NefBoardGetTickMs() >= Deadline)        return NEF_ETIMEDOUT;
    }
}

EfiStatus NefHalMutexUnlock(NefHalHandle M) {
    if (!M) return NEF_EINVAL;
    UINT32 S = NefBoardEnterCritical();
    ((MutexObj *)M)->Locked = 0;
    NefBoardExitCritical(S);
    return NEF_OK;
}

EfiStatus NefHalMutexDelete(NefHalHandle M) {
    if (!M) return NEF_EINVAL;
    ((MutexObj *)M)->Used = 0;
    return NEF_OK;
}

/*──────────────────────────────────────────────────────────────────────
 * Semaphore
 *──────────────────────────────────────────────────────────────────────*/
EfiStatus NefHalSemCreate(UINT32 Initial, UINT32 Max, NefHalHandle *Out) {
    if (!Out || Initial > Max) return NEF_EINVAL;
    SemObj *S;
    ALLOC_FROM_POOL(SSemPool, &S);
    S->Cnt = Initial; S->Max = Max;
    *Out = S;
    return NEF_OK;
}

EfiStatus NefHalSemTake(NefHalHandle S, UINT32 TimeoutMs) {
    if (!S) return NEF_EINVAL;
    SemObj *s = S;
    UINT64 Deadline = NefBoardGetTickMs() + TimeoutMs;
    while (1) {
        UINT32 Sv = NefBoardEnterCritical();
        if (s->Cnt) { s->Cnt--; NefBoardExitCritical(Sv); return NEF_OK; }
        NefBoardExitCritical(Sv);
        if (TimeoutMs == NANO_EFI_TIMEOUT_POLL)    return NEF_ETIMEDOUT;
        if (TimeoutMs != NANO_EFI_TIMEOUT_FOREVER &&
            NefBoardGetTickMs() >= Deadline)        return NEF_ETIMEDOUT;
    }
}

EfiStatus NefHalSemGive(NefHalHandle S) {
    if (!S) return NEF_EINVAL;
    SemObj *s = S;
    UINT32 Sv = NefBoardEnterCritical();
    if (s->Cnt < s->Max) s->Cnt++;
    NefBoardExitCritical(Sv);
    return NEF_OK;
}

EfiStatus NefHalSemDelete(NefHalHandle S) {
    if (!S) return NEF_EINVAL;
    ((SemObj *)S)->Used = 0;
    return NEF_OK;
}

/*──────────────────────────────────────────────────────────────────────
 * Event (auto-reset)
 *──────────────────────────────────────────────────────────────────────*/
EfiStatus NefHalEventCreate(NefHalHandle *Out) {
    if (!Out) return NEF_EINVAL;
    EventObj *E;
    ALLOC_FROM_POOL(SEventPool, &E);
    E->Sig = 0;
    *Out = E;
    return NEF_OK;
}

EfiStatus NefHalEventWait(NefHalHandle E, UINT32 TimeoutMs) {
    if (!E) return NEF_EINVAL;
    EventObj *e = E;
    UINT64 Deadline = NefBoardGetTickMs() + TimeoutMs;
    while (1) {
        UINT32 S = NefBoardEnterCritical();
        if (e->Sig) { e->Sig = 0; NefBoardExitCritical(S); return NEF_OK; }
        NefBoardExitCritical(S);
        if (TimeoutMs == NANO_EFI_TIMEOUT_POLL)    return NEF_ETIMEDOUT;
        if (TimeoutMs != NANO_EFI_TIMEOUT_FOREVER &&
            NefBoardGetTickMs() >= Deadline)        return NEF_ETIMEDOUT;
    }
}

EfiStatus NefHalEventSignal(NefHalHandle E) {
    if (!E) return NEF_EINVAL;
    UINT32 S = NefBoardEnterCritical();
    ((EventObj *)E)->Sig = 1;
    NefBoardExitCritical(S);
    return NEF_OK;
}

EfiStatus NefHalEventDelete(NefHalHandle E) {
    if (!E) return NEF_EINVAL;
    ((EventObj *)E)->Used = 0;
    return NEF_OK;
}

/*──────────────────────────────────────────────────────────────────────
 * Time / Log / GPIO / Capability
 *──────────────────────────────────────────────────────────────────────*/
UINT64    NefHalGetTick(void)              { return NefBoardGetTickMs(); }
EfiStatus NefHalDelayMs(UINT32 Ms)        { NefBoardDelayMs(Ms); return NEF_OK; }

void NefHalLog(const char *Fmt, va_list Ap) {
    char Buf[128];
    NefVsnprintf(Buf, sizeof(Buf), Fmt, Ap);
    for (char *P = Buf; *P; P++) NefBoardUartPutchar(*P);
}

EfiStatus NefHalPinSet(UINT8 Pin, UINT8 Val)       { return NefBoardPinSet(Pin, Val); }
EfiStatus NefHalPinGet(UINT8 Pin, UINT8 *Val)      { return NefBoardPinGet(Pin, Val); }
EfiStatus NefHalPinMode(UINT8 Pin, UINT8 Mode)     { return NefBoardPinMode(Pin, Mode); }
EfiStatus NefHalQueryCap(UINT32 Id, UINT32 *Val) {
    if (!Val) return NEF_EINVAL;
    *Val = NefBoardQueryCap(Id);
    return NEF_OK;
}

/*──────────────────────────────────────────────────────────────────────
 * Exec memory — baremetal: driver image lives in flash/static RAM,
 * no dynamic exec alloc needed. Return NULL to signal unavailability.
 *──────────────────────────────────────────────────────────────────────*/
void *NefHalAllocExec(NEF_SIZE Bytes) { (void)Bytes; return NULL; }
void  NefHalFreeExec(void *P, NEF_SIZE Bytes) { (void)P; (void)Bytes; }
