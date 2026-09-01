// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <Arduino.h>
#include "detector.h"

void uiInit();
void uiBootScreen();

// radioLabel is the header's mode/radio indicator, e.g. "BOTH: WiFi",
// "BOTH: BLE", "WiFi ONLY", "BLE ONLY" - caller resolves it since it depends
// on band mode state that ui.cpp doesn't own.
void uiRender(const DetectorStatus& st, const char* radioLabel);
