/*
 * nef_services.h — NanoEFI Service Table
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 NanoEFI Contributors
 *
 * This header defines the sole interface between a NanoEFI driver and the
 * platform.  A driver receives a NanoEfiServiceTable* at entry time and
 * must not retain any platform-specific headers or symbols.
 *
 * Naming conventions:
 *   Types       — PascalCase (NanoEfiServiceTable, NanoEfiTask, ...)
 *   Macros      — SCREAMING_SNAKE_CASE (NANO_EFI_VERSION, NEF_OK, ...)
 *   Functions   — PascalCase function pointers inside the struct
 *   Utilities   — nef_ snake_case (nef_snprintf, nef_memcpy, ...)
 *
 * Return values: EfiStatus (0 = EFI_SUCCESS = NEF_OK; error bit set on
 * failure). Test with EFI_ERROR(s) or compare to NEF_* aliases.
 *
 * Version encoding: (Major << 16) | Minor.  v1.0 = 0x00010000.
 */

#ifndef NEF_SERVICES_H
#define NEF_SERVICES_H

#include "NefErrno.h"
/* nef_arch.h constants (NANO_EFI_TIMEOUT_*, NANO_EFI_PIN_*) are used
 * in the semantic-contract comments above; drivers include it separately. */

/*──────────────────────────────────────────────────────────────────────
 * Service table version
 *──────────────────────────────────────────────────────────────────────*/

#define NANO_EFI_VERSION_MAKE(maj, min) (((UINT32)(maj) << 16u) | (UINT32)(min))
#define NANO_EFI_VERSION_CURRENT NANO_EFI_VERSION_MAKE(1, 0) /* 0x00010000 */

/*──────────────────────────────────────────────────────────────────────
 * Opaque Handle Types
 *
 * Handles are void* internally; the backend casts them to its own
 * heap-allocated objects.  Drivers treat them as cookies.
 *──────────────────────────────────────────────────────────────────────*/

typedef VOID* NanoEfiTask;
typedef VOID* NanoEfiMutex;
typedef VOID* NanoEfiSemaphore;
typedef VOID* NanoEfiEvent;

/* Task body — runs in its own stack context */
typedef VOID(NEF_CALLCONV* NanoEfiTaskEntry)(VOID* Arg);

/*──────────────────────────────────────────────────────────────────────
 * Service Table
 *──────────────────────────────────────────────────────────────────────*/

typedef struct _NanoEfiServiceTable {
  /*
   * Version — NANO_EFI_VERSION_MAKE(major, minor).
   * Drivers check this against MinSvcVersion from their NDIH header.
   * Increment minor on backward-compatible additions,
   * increment major on any ABI-breaking change.
   */
  UINT32 Version;

  /* ── Task Management ─────────────────────────────────────────────
   *
   * Tasks are independently scheduled units of execution.  Each task
   * has its own stack.  Priority is backend-defined (higher = more
   * urgent on preemptive platforms; ignored on bare-metal).
   */

  /*
   * CreateTask — Allocate and start a new task.
   *
   * Pre:  Entry != NULL, Out != NULL, StackSize > 0.
   * Post: *Out holds a valid handle; the task is runnable immediately.
   *
   * ISR-safe: NO    May block: NO (returns NEF_ENOMEM on failure)
   * Returns:  NEF_OK | NEF_ENOMEM | NEF_EINVAL
   */
  EfiStatus(NEF_CALLCONV* CreateTask)(IN NanoEfiTaskEntry Entry,
                    IN VOID* Arg OPTIONAL,
                    IN UINT32 StackSize, IN UINT32 Priority,
                    OUT NanoEfiTask* Out);

  /*
   * DeleteTask — Terminate and free a task.
   *
   * Pre:  Task is a valid handle from CreateTask.
   *       Must NOT be called with the calling task's own handle.
   * Post: Task stack and OS object are freed.
   *
   * ISR-safe: NO    May block: YES (waits for task exit on some backends)
   * Returns:  NEF_OK | NEF_EINVAL
   */
  EfiStatus(NEF_CALLCONV* DeleteTask)(IN NanoEfiTask Task);

  /*
   * TaskSleep — Suspend the calling task for at least Ms milliseconds.
   *
   * Pre:  Called from a task context (not from an ISR).
   * Post: Calling task resumes after >= Ms ms; other tasks may run.
   *
   * ISR-safe: NO    May block: YES
   * Returns:  NEF_OK
   */
  EfiStatus(NEF_CALLCONV* TaskSleep)(IN UINT32 Ms);

  /* ── Mutex ───────────────────────────────────────────────────────
   *
   * Binary mutex with ownership tracking.  Not ISR-safe — use a
   * semaphore (GiveSemaphore is ISR-safe) for ISR↔task signalling.
   */

  /*
   * CreateMutex — Create an unlocked mutex.
   *
   * ISR-safe: NO    May block: NO
   * Returns:  NEF_OK | NEF_ENOMEM | NEF_EINVAL
   */
  EfiStatus(NEF_CALLCONV* CreateMutex)(OUT NanoEfiMutex* Out);

  /*
   * LockMutex — Acquire the mutex.
   *
   * TimeoutMs: NANO_EFI_TIMEOUT_POLL (0)    → return immediately if busy
   *            NANO_EFI_TIMEOUT_FOREVER      → block until acquired
   *            positive value                → block at most N ms
   *
   * ISR-safe: NO    May block: YES
   * Returns:  NEF_OK | NEF_ETIMEDOUT | NEF_EINVAL
   */
  EfiStatus(NEF_CALLCONV* LockMutex)(IN NanoEfiMutex Mutex,
                    IN UINT32 TimeoutMs);

  /*
   * UnlockMutex — Release ownership of the mutex.
   *
   * Pre:  Caller currently holds the mutex.
   *
   * ISR-safe: NO    May block: NO
   * Returns:  NEF_OK | NEF_EINVAL | NEF_EPERM
   */
  EfiStatus(NEF_CALLCONV* UnlockMutex)(IN NanoEfiMutex Mutex);

  /*
   * DeleteMutex — Destroy the mutex.
   *
   * Pre:  No tasks are waiting on the mutex.
   *
   * ISR-safe: NO    May block: NO
   * Returns:  NEF_OK | NEF_EINVAL
   */
  EfiStatus(NEF_CALLCONV* DeleteMutex)(IN NanoEfiMutex Mutex);

  /* ── Counting Semaphore ──────────────────────────────────────────
   *
   * GiveSemaphore is ISR-safe and suitable for ISR→task notification.
   * Initial = 0, Maximum = 1 gives a binary semaphore.
   */

  /*
   * CreateSemaphore — Create a counting semaphore.
   *
   * Initial: starting count (0 = locked, > 0 = available).
   * Maximum: upper bound enforced by GiveSemaphore.
   *
   * ISR-safe: NO    May block: NO
   * Returns:  NEF_OK | NEF_ENOMEM | NEF_EINVAL
   */
  EfiStatus(NEF_CALLCONV* CreateSemaphore)(IN UINT32 Initial, IN UINT32 Maximum,
                       OUT NanoEfiSemaphore* Out);

  /*
   * TakeSemaphore — Decrement the semaphore; block if count is zero.
   *
   * ISR-safe: NO    May block: YES
   * Returns:  NEF_OK | NEF_ETIMEDOUT | NEF_EINVAL
   */
  EfiStatus(NEF_CALLCONV* TakeSemaphore)(IN NanoEfiSemaphore Semaphore,
                      IN UINT32 TimeoutMs);

  /*
   * GiveSemaphore — Increment the semaphore count.
   *
   * Safe to call from an ISR on all RTOS and bare-metal backends.
   * Silently saturates at Maximum (no error).
   *
   * ISR-safe: YES   May block: NO
   * Returns:  NEF_OK | NEF_EINVAL
   */
  EfiStatus(NEF_CALLCONV* GiveSemaphore)(IN NanoEfiSemaphore Semaphore);

  /*
   * DeleteSemaphore — Destroy the semaphore.
   *
   * Pre: No tasks are waiting.
   *
   * ISR-safe: NO    May block: NO
   * Returns:  NEF_OK | NEF_EINVAL
   */
  EfiStatus(NEF_CALLCONV* DeleteSemaphore)(IN NanoEfiSemaphore Semaphore);

  /* ── Event (binary, auto-reset) ──────────────────────────────────
   *
   * SignalEvent wakes exactly one WaitEvent caller.
   * The event resets automatically after a successful wait.
   * SignalEvent is ISR-safe.
   */

  /*
   * CreateEvent — Create a non-signaled event.
   *
   * ISR-safe: NO    May block: NO
   * Returns:  NEF_OK | NEF_ENOMEM | NEF_EINVAL
   */
  EfiStatus(NEF_CALLCONV* CreateEvent)(OUT NanoEfiEvent* Out);

  /*
   * WaitEvent — Block until signaled or timeout expires.
   *
   * Auto-resets the event on successful return.
   *
   * ISR-safe: NO    May block: YES
   * Returns:  NEF_OK | NEF_ETIMEDOUT | NEF_EINVAL
   */
  EfiStatus(NEF_CALLCONV* WaitEvent)(IN NanoEfiEvent Event,
                    IN UINT32 TimeoutMs);

  /*
   * SignalEvent — Signal the event, unblocking one waiter.
   *
   * Safe to call from an ISR.
   * If no task is waiting the signal is "remembered" until next WaitEvent.
   *
   * ISR-safe: YES   May block: NO
   * Returns:  NEF_OK | NEF_EINVAL
   */
  EfiStatus(NEF_CALLCONV* SignalEvent)(IN NanoEfiEvent Event);

  /*
   * DeleteEvent — Destroy the event.
   *
   * Pre: No tasks are waiting.
   *
   * ISR-safe: NO    May block: NO
   * Returns:  NEF_OK | NEF_EINVAL
   */
  EfiStatus(NEF_CALLCONV* DeleteEvent)(IN NanoEfiEvent Event);

  /* ── Time ────────────────────────────────────────────────────────*/

  /*
   * GetTick — Monotonic millisecond timestamp.
   *
   * Post: Return value is non-decreasing and never wraps within a
   *       single power cycle (UINT64 at 1 ms resolution overflows in
   *       ~585 million years).
   *
   * ISR-safe: YES   May block: NO
   */
  UINT64(NEF_CALLCONV* GetTick)(VOID);

  /*
   * DelayMs — Suspend execution for at least Ms milliseconds.
   *
   * On RTOS backends, yields CPU to other tasks.
   * On bare-metal, busy-waits.
   *
   * ISR-safe: NO    May block: YES
   * Returns:  NEF_OK
   */
  EfiStatus(NEF_CALLCONV* DelayMs)(IN UINT32 Ms);

  /* ── Logging ─────────────────────────────────────────────────────
   *
   * Log routes output to the platform channel (UART, semihosting,
   * console, etc.).  Uses nef_vsnprintf internally on freestanding
   * backends.  No floating-point format specifiers.
   *
   * ISR-safe: platform-dependent   May block: platform-dependent
   */
  VOID(NEF_CALLCONV* Log)(IN const char* Fmt, ...);

  /* ── GPIO ────────────────────────────────────────────────────────
   *
   * Pin numbers are backend-defined.  On Linux the GPIO is mocked.
   * On bare-metal pins map to hardware GPIO numbers.
   */

  /*
   * PinSet — Drive a configured output pin to Value (0 or 1).
   *
   * Pre:  Pin has been configured as output via PinMode.
   *
   * ISR-safe: YES (bare-metal)   May block: NO
   * Returns:  NEF_OK | NEF_EINVAL | NEF_ENOSYS
   */
  EfiStatus(NEF_CALLCONV* PinSet)(IN UINT8 Pin, IN UINT8 Value);

  /*
   * PinGet — Read the current level of a GPIO pin.
   *
   * ISR-safe: YES   May block: NO
   * Returns:  NEF_OK | NEF_EINVAL | NEF_ENOSYS
   */
  EfiStatus(NEF_CALLCONV* PinGet)(IN UINT8 Pin, OUT UINT8* Value);

  /*
   * PinMode — Configure pin direction / pull resistor.
   *
   * Mode: NANO_EFI_PIN_INPUT | NANO_EFI_PIN_OUTPUT |
   *       NANO_EFI_PIN_INPUT_PU | NANO_EFI_PIN_INPUT_PD
   *
   * ISR-safe: NO    May block: NO
   * Returns:  NEF_OK | NEF_EINVAL | NEF_ENOSYS
   */
  EfiStatus(NEF_CALLCONV* PinMode)(IN UINT8 Pin, IN UINT8 Mode);

  /* ── Capability Query ────────────────────────────────────────────*/

  /*
   * QueryCapability — Test whether a platform capability is available.
   *
   * Value is set to 1 if the capability is present, 0 otherwise.
   * Quantitative capabilities (e.g. NANO_EFI_CAP_CACHE) may store
   * a size in bytes in Value.
   *
   * ISR-safe: YES   May block: NO
   * Returns:  NEF_OK | NEF_EINVAL
   */
  EfiStatus(NEF_CALLCONV* QueryCapability)(IN UINT32 CapabilityId,
                       OUT UINT32* Value);

  /* ── Extension ───────────────────────────────────────────────────
   *
   * Pointer to an optional extension service table (NULL in v1.0).
   * Future peripheral extensions (SPI, I²C, UART, USB, …) are
   * exposed here without altering this struct's memory layout.
   * Drivers check for NULL before dereferencing.
   */
  VOID* Extension;

} NanoEfiServiceTable;

/* Short alias used in driver code */
typedef NanoEfiServiceTable NanoEfiSvc;

#endif /* NEF_SERVICES_H */
