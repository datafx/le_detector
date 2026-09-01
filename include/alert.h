// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <Arduino.h>
#include "detector.h"

enum AlertState : uint8_t { ALERT_CLEAR = 0, ALERT_ACTIVE };

void alertInit();

// Direct buzzer control honoring BUZZER_ACTIVE_LOW and BUZZER_ENABLED, for use
// outside the alert state machine (e.g. the power-on self test).
void alertSetBuzzer(bool on);

// Non-blocking. Call every loop iteration; it drives both outputs from one
// shared phase counter so the LED and buzzer are always in step. holdMs is
// the caller-resolved decay duration (DUAL_MODE_HOLD_MS or
// SINGLE_MODE_HOLD_MS depending on current band mode) - alert.cpp doesn't
// know about band mode itself.
void alertUpdate(const DetectorStatus& st, uint32_t holdMs);

// Silences the buzzer for the currently active alert only (LED keeps
// flashing). Does nothing outside ALERT_ACTIVE, and does not persist once
// this alert instance clears - the next alert starts unmuted.
void alertMute();

AlertState alertState();
uint32_t   alertPeriodMs();   // current flash period, for the UI
bool       alertOutputOn();   // current on/off phase, for the UI
