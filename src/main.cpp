/*
    LE Gear Detector - ESP32-WROOM-32U DevKitC

    Passive BLE + WiFi scanner that watches for OUIs on a vendor watchlist and
    signals hits on an LED and buzzer, flashing faster as the signal gets stronger.

    Portions adapted from nyanBOX (https://github.com/jbohack/nyanBOX)
    Copyright (c) 2025 jbohack - MIT License
    SPDX-License-Identifier: MIT
*/

#include <Arduino.h>
#include <Preferences.h>
#include "config.h"
#include "detector.h"
#include "alert.h"
#include "ui.h"

// The two radio stacks cannot be up at the same time, so we alternate.
enum Phase : uint8_t { PHASE_WIFI, PHASE_BLE };

// Runtime-selectable via BOOT-hold, persisted in NVS. Decision 2 (one radio,
// time-sliced) still holds - a single-band mode just skips one side of the
// alternation rather than running both at once.
enum BandMode : uint8_t { BAND_WIFI_ONLY = 0, BAND_BLE_ONLY = 1, BAND_BOTH = 2 };

static Phase     s_phase      = PHASE_WIFI;
static uint32_t  s_phaseStart = 0;
static uint32_t  s_lastUi     = 0;
static BandMode  s_bandMode   = BAND_BOTH;
static Preferences s_prefs;

// --- BOOT button: tap = mute, hold 1s = cycle band mode ---
static bool     s_btnRaw        = false;
static bool     s_btnStable     = false;
static uint32_t s_btnLastChange = 0;
static uint32_t s_btnPressStart = 0;
static bool     s_btnHoldFired  = false;

static void switchToWifi() {
    detectorStopBlePhase();
    detectorStartWifiPhase();
    s_phase      = PHASE_WIFI;
    s_phaseStart = millis();
}

static void switchToBle() {
    detectorStopWifiPhase();
    detectorStartBlePhase();
    s_phase      = PHASE_BLE;
    s_phaseStart = millis();
}

// Forces the running phase to match a newly-selected band mode; BAND_BOTH
// just lets the normal alternation continue from wherever it is.
static void applyBandMode(BandMode mode) {
    s_bandMode = mode;
    if (mode == BAND_BLE_ONLY && s_phase == PHASE_WIFI) switchToBle();
    if (mode == BAND_WIFI_ONLY && s_phase == PHASE_BLE) switchToWifi();
}

static void cycleBandMode() {
    BandMode next;
    switch (s_bandMode) {
        case BAND_BOTH:      next = BAND_WIFI_ONLY; break;
        case BAND_WIFI_ONLY: next = BAND_BLE_ONLY;  break;
        default:              next = BAND_BOTH;      break;
    }
    applyBandMode(next);
    s_prefs.putUChar("bandMode", (uint8_t)next);
}

static void pollButton(uint32_t now) {
    bool raw = (digitalRead(PIN_BOOT) == LOW);   // active-low
    if (raw != s_btnRaw) {
        s_btnRaw        = raw;
        s_btnLastChange = now;
    }

    if (now - s_btnLastChange >= BUTTON_DEBOUNCE_MS && raw != s_btnStable) {
        s_btnStable = raw;
        if (s_btnStable) {
            s_btnPressStart = now;
            s_btnHoldFired  = false;
        } else if (!s_btnHoldFired) {
            alertMute();   // released before the hold threshold -> tap
        }
    }

    if (s_btnStable && !s_btnHoldFired && (now - s_btnPressStart >= BUTTON_HOLD_MS)) {
        s_btnHoldFired = true;
        cycleBandMode();
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(PIN_BOOT, INPUT_PULLUP);

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

    s_prefs.begin("ledet", false);
    s_bandMode = (BandMode)s_prefs.getUChar("bandMode", BAND_BOTH);

    if (s_bandMode == BAND_BLE_ONLY) {
        detectorStartBlePhase();
        s_phase = PHASE_BLE;
    } else {
        detectorStartWifiPhase();
        s_phase = PHASE_WIFI;
    }
    s_phaseStart = millis();
}

void loop() {
    uint32_t now = millis();

    pollButton(now);

    // --- radio phase machine ---
    if (s_phase == PHASE_WIFI) {
        detectorHopChannel();
        bool timedOut = (now - s_phaseStart >= WIFI_PHASE_MS);
        if (s_bandMode != BAND_WIFI_ONLY && timedOut) switchToBle();
    } else {
        bool timedOut = (now - s_phaseStart >= BLE_PHASE_MS);
        if (s_bandMode != BAND_BLE_ONLY && timedOut) switchToWifi();
    }

    detectorExpire();

    DetectorStatus st = detectorStatus();

    // Alerts run every iteration - the flash timing depends on it. Hold
    // duration depends on band mode: dual mode needs slack to survive a
    // full WiFi<->BLE round trip, single mode doesn't have that blind window.
    uint32_t holdMs = (s_bandMode == BAND_BOTH) ? DUAL_MODE_HOLD_MS : SINGLE_MODE_HOLD_MS;
    alertUpdate(st, holdMs);

    // Display is comparatively slow, so throttle it.
    if (now - s_lastUi >= UI_REFRESH_MS) {
        const char* radioLabel;
        if (s_bandMode == BAND_WIFI_ONLY)      radioLabel = "WiFi ONLY";
        else if (s_bandMode == BAND_BLE_ONLY)  radioLabel = "BLE ONLY";
        else radioLabel = (s_phase == PHASE_WIFI) ? "AUTO: WiFi" : "AUTO: BLE";

        uiRender(st, radioLabel);
        s_lastUi = now;
    }
}
