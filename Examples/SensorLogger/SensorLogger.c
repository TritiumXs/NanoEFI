/*
 * examples/sensor_logger/SensorLogger.c — Platform-Agnostic Sensor Logger
 * Driver
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 NanoEFI Contributors
 *
 * This is the ONLY source file that runs on every platform.  It includes
 * nothing except NefServices.h and NefFormat.h.  The same binary (after
 * cross-compilation) runs on Linux, STM32F103, and GD32VF103.
 *
 * Simulated sensor: a sawtooth temperature signal (0–99 °C) advancing
 * by 1 each tick.  On real MCU targets, replace ReadSensor() with an
 * actual I²C or ADC read via Svc->Extension.
 */

#include "NefFormat.h"
#include "NefServices.h"

/*──────────────────────────────────────────────────────────────────────
 * Simulated sensor — replace with real hardware read on MCU
 *──────────────────────────────────────────────────────────────────────*/

static INT32 ReadSensor(UINT64 Tick) {
  /* Sawtooth: 0..99 repeating, one step per second */

  return (INT32)(Tick / 1000ULL % 100ULL);
}

/*──────────────────────────────────────────────────────────────────────
 * Driver Entry
 *──────────────────────────────────────────────────────────────────────*/

#define SAMPLE_PERIOD_MS 1000u /* 1 Hz */
#define SAMPLE_COUNT 10u       /* run for 10 samples, then exit */

EfiStatus SensorLoggerEntry(NanoEfiServiceTable* Svc, VOID* Arg) {
  char Buf[64];
  UINT32 i;
  INT32 Temp;
  UINT64 Tick;

  (VOID) Arg;

  Svc->Log("[sensor_logger] Starting — %u samples at %u ms intervals\n",
           SAMPLE_COUNT, SAMPLE_PERIOD_MS);

  for (i = 0; i < SAMPLE_COUNT; i++) {
    Svc->DelayMs(SAMPLE_PERIOD_MS);

    Tick = Svc->GetTick();
    Temp = ReadSensor(Tick);

    NefSnprintf(Buf, sizeof Buf,
                "[sensor_logger] sample=%u  temp=%d C  tick=%u ms", i, Temp,
                (UINT32)Tick);

    Svc->Log("%s\n", Buf);

    /* Toggle GPIO 0 as a heartbeat */
    Svc->PinSet(0, (UINT8)(i & 1u));
  }

  Svc->Log("[sensor_logger] Done.\n");

  return NEF_OK;
}
