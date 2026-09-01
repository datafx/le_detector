/*
    LE Gear Detector - ESP32-WROOM-32U DevKitC

    Passive BLE + WiFi scanner that watches for OUIs on a vendor watchlist and
    signals hits on an LED and buzzer, flashing faster as the signal gets stronger.

    Portions adapted from nyanBOX (https://github.com/jbohack/nyanBOX)
    Copyright (c) 2025 jbohack - MIT License
    SPDX-License-Identifier: MIT
*/

#include <Arduino.h>
#include "config.h"
#include "detector.h"
#include "alert.h"
#include "ui.h"

// The two radio stacks cannot be up at the same time, so we alternate.
enum Phase : uint8_t { PHASE_WIFI, PHASE_BLE };

static Phase    s_phase      = PHASE_WIFI;
static uint32_t s_phaseStart = 0;
static uint32_t s_lastUi     = 0;

void setup() {
    Serial.begin(115200);

    uiInit();
    uiBootScreen();

    alertInit();
    detectorInit();

    // Quick power-on self test so you can confirm both outputs are wired right.
    digitalWrite(PIN_LED, HIGH);
    alertSetBuzzer(true);
    delay(120);
    digitalWrite(PIN_LED, LOW);
    alertSetBuzzer(false);

    delay(600);

    detectorStartWifiPhase();
    s_phase      = PHASE_WIFI;
    s_phaseStart = millis();
}

void loop() {
    uint32_t now = millis();

    // --- radio phase machine ---
    if (s_phase == PHASE_WIFI) {
        detectorHopChannel();
        if (now - s_phaseStart >= WIFI_PHASE_MS) {
            detectorStopWifiPhase();
            detectorStartBlePhase();
            s_phase      = PHASE_BLE;
            s_phaseStart = millis();
        }
    } else {
        if (now - s_phaseStart >= BLE_PHASE_MS) {
            detectorStopBlePhase();
            detectorStartWifiPhase();
            s_phase      = PHASE_WIFI;
            s_phaseStart = millis();
        }
    }

    detectorExpire();

    DetectorStatus st = detectorStatus();

    // Alerts run every iteration - the flash timing depends on it.
    alertUpdate(st);

    // Display is comparatively slow, so throttle it.
    if (now - s_lastUi >= UI_REFRESH_MS) {
        uiRender(st, s_phase == PHASE_WIFI);
        s_lastUi = now;
    }
}
