// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <Arduino.h>
#include "oui_table.h"

// SRC_PROBE is its own value (not lumped into SRC_WIFI) because a probe
// request SSID is weaker evidence than a beacon/probe-response SSID or an
// OUI hit: it's a client leaking a network it previously joined, not proof
// that network's AP is nearby now. Beacon/probe-response SSID matches stay
// tagged SRC_WIFI - same evidentiary weight as an OUI hit on those frames.
enum Source : uint8_t { SRC_BLE = 0, SRC_WIFI, SRC_PROBE };

struct TrackedDevice {
    uint8_t      mac[6];
    int16_t      rssi;        // EMA-smoothed
    uint32_t     firstSeen;
    uint32_t     lastSeen;
    const char*  vendor;
    GearCategory category;
    Source       source;
    char         ssid[33];    // NUL-terminated; empty when match wasn't SSID-based
    bool         used;
};

void detectorInit();

// Phase control - only one radio stack is up at a time.
void detectorStartWifiPhase();
void detectorStopWifiPhase();
void detectorStartBlePhase();
void detectorStopBlePhase();

void detectorHopChannel();      // call from loop during the WiFi phase
void detectorExpire();          // drop entries older than DEVICE_TTL_MS

// Summary of the current threat picture, for the UI and the alert engine.
struct DetectorStatus {
    uint8_t              activeCount;   // matched devices currently in the table
    int16_t              bestRssi;      // strongest matched device
    const TrackedDevice* best;          // nullptr when nothing is matched
    uint32_t             lastHitMs;     // millis() of the most recent match
};

DetectorStatus detectorStatus();

uint8_t  detectorChannel();
uint16_t detectorSeenTotal();   // every frame/advert we looked at, matched or not
