// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <Arduino.h>
#include "oui_table.h"

enum Confidence : uint8_t {
    CONF_NONE = 0,
    CONF_MEDIUM,   // OUI hit on a public address
    CONF_HIGH      // OUI hit, public address, seen repeatedly
};

enum Source : uint8_t { SRC_BLE = 0, SRC_WIFI };

struct TrackedDevice {
    uint8_t      mac[6];
    int16_t      rssi;        // EMA-smoothed
    uint32_t     firstSeen;
    uint32_t     lastSeen;
    uint16_t     hits;
    const char*  vendor;
    GearCategory category;
    Specificity  specificity;
    Confidence   confidence;
    Source       source;
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
    Confidence           bestConfidence;
    const TrackedDevice* best;          // nullptr when nothing is matched
    uint32_t             lastHitMs;     // millis() of the most recent match
};

DetectorStatus detectorStatus();

uint8_t  detectorChannel();
uint16_t detectorSeenTotal();   // every frame/advert we looked at, matched or not
