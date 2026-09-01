// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <Arduino.h>
#include "detector.h"

void uiInit();
void uiBootScreen();
void uiRender(const DetectorStatus& st, bool wifiPhase);
