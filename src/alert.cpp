// SPDX-License-Identifier: GPL-3.0-only
#include "alert.h"
#include "config.h"

static AlertState s_state       = ALERT_CLEAR;
static uint32_t   s_period      = FLASH_PERIOD_SLOW;
static uint32_t   s_phaseStart  = 0;
static bool       s_outputOn    = false;
static bool       s_muted       = false;

void alertSetBuzzer(bool on) {
    bool sound = BUZZER_ENABLED && on;
    bool level = BUZZER_ACTIVE_LOW ? !sound : sound;
    digitalWrite(PIN_BUZZER, level ? HIGH : LOW);
}

void alertInit() {
    pinMode(PIN_LED, OUTPUT);
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_LED, LOW);
    alertSetBuzzer(false);
    s_state      = ALERT_CLEAR;
    s_phaseStart = millis();
    s_outputOn   = false;
    s_muted      = false;
}

// Strong signal -> short period (fast flash). Weak -> long period (slow flash).
static uint32_t rssiToPeriod(int16_t rssi) {
    if (rssi <= RSSI_WEAK)   return FLASH_PERIOD_SLOW;
    if (rssi >= RSSI_STRONG) return FLASH_PERIOD_FAST;

    int32_t span   = (int32_t)RSSI_STRONG - (int32_t)RSSI_WEAK;      // e.g. 50
    int32_t offset = (int32_t)rssi        - (int32_t)RSSI_WEAK;      // 0..span
    int32_t range  = (int32_t)FLASH_PERIOD_SLOW - (int32_t)FLASH_PERIOD_FAST;

    return (uint32_t)((int32_t)FLASH_PERIOD_SLOW - (offset * range) / span);
}

static void driveOutputs(bool on, uint32_t onElapsed) {
    digitalWrite(PIN_LED, on ? HIGH : LOW);

    // Buzzer shares the LED's phase, but is capped so a fast flash chirps
    // rather than turning into a continuous tone. Muting silences only the
    // buzzer - the LED keeps flashing as the visual cue.
    bool buzz = on && !s_muted && (onElapsed < BUZZER_MAX_ON_MS);
    alertSetBuzzer(buzz);
}

void alertUpdate(const DetectorStatus& st, uint32_t holdMs) {
    uint32_t now = millis();

    // Pure decay timer: active as long as *something* matched within the
    // last holdMs, regardless of whether that device is still in the
    // tracked table right now. Not a per-device latch - any qualifying
    // receive resets the same countdown, purely to stop on/off flicker.
    bool active = (st.lastHitMs != 0) && (now - st.lastHitMs <= holdMs);

    if (!active) {
        if (s_state != ALERT_CLEAR) {
            s_state    = ALERT_CLEAR;
            s_outputOn = false;
            s_muted    = false;   // mute doesn't carry over to the next alert
            digitalWrite(PIN_LED, LOW);
            alertSetBuzzer(false);
        }
        return;
    }

    if (s_state == ALERT_CLEAR) {
        // Entering alert: start on the "on" phase so the first flash is immediate.
        s_state      = ALERT_ACTIVE;
        s_phaseStart = now;
        s_outputOn   = true;
    }

    s_period = rssiToPeriod(st.bestRssi);
    uint32_t halfPeriod = s_period / 2;
    uint32_t elapsed    = now - s_phaseStart;

    if (elapsed >= halfPeriod) {
        s_outputOn   = !s_outputOn;
        s_phaseStart = now;
        elapsed      = 0;
    }

    driveOutputs(s_outputOn, elapsed);
}

void alertMute() {
    if (s_state == ALERT_ACTIVE) s_muted = true;
}

AlertState alertState()     { return s_state; }
uint32_t   alertPeriodMs()  { return s_period; }
bool       alertOutputOn()  { return s_outputOn; }
