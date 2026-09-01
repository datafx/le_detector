// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------------
// Hardware (matches the breadboard wiring diagram)
// ---------------------------------------------------------------------------
static const uint8_t PIN_I2C_SDA = 21;   // OLED SDA  (J3 pin 6)
static const uint8_t PIN_I2C_SCL = 22;   // OLED SCL  (J3 pin 3)
static const uint8_t PIN_LED     = 25;   // alert LED (J2 pin 9)
static const uint8_t PIN_BUZZER  = 26;   // active buzzer SIG (J2 pin 10)

// ---------------------------------------------------------------------------
// Radio scheduling
//
// The ESP32 has ONE 2.4 GHz radio shared between WiFi and BLE. nyanBOX handles
// this by fully tearing one stack down before bringing the other up, and we do
// the same. That means detection is time-sliced: during the BLE window we are
// blind to WiFi and vice versa.
//
// Shorter windows = tighter revisit interval (better at highway speed) but more
// time lost to stack init/teardown, which costs ~100-200 ms each way.
// ---------------------------------------------------------------------------
static const uint32_t WIFI_PHASE_MS       = 3500;  // ~1 full sweep of ch 1-13
static const uint32_t BLE_PHASE_MS        = 3000;
static const uint32_t CHANNEL_HOP_MS      = 250;   // 13 ch * 250 ms = 3.25 s
static const uint8_t  WIFI_MAX_CHANNEL    = 13;

// Passive = receive only, we never transmit. nyanBOX ships ACTIVE (which sends
// scan requests); set this false only if you specifically want active scanning.
static const bool BLE_SCAN_PASSIVE = true;

// ---------------------------------------------------------------------------
// Device tracking
// ---------------------------------------------------------------------------
static const uint8_t  MAX_TRACKED       = 48;    // fixed table, LRU eviction
static const uint32_t DEVICE_TTL_MS     = 10000; // drop entry if unseen this long
static const uint8_t  RSSI_EMA_NUM      = 1;     // EMA smoothing: new weight
static const uint8_t  RSSI_EMA_DEN      = 3;     //   value = (new*1 + old*2)/3

// A device must be seen this many times before it can reach HIGH confidence.
static const uint8_t  HIGH_CONF_HITS    = 3;

// ---------------------------------------------------------------------------
// Alert behaviour
//
// LED and buzzer are driven from a single phase so they are always in sync.
// Flash rate scales with the strongest active signal: weak = slow, strong = fast.
// ---------------------------------------------------------------------------
static const uint32_t ALERT_HOLD_MS     = 4000;  // self-clearing active horizon

static const int8_t   RSSI_WEAK         = -95;   // maps to the slowest flash
static const int8_t   RSSI_STRONG       = -45;   // maps to the fastest flash
static const uint32_t FLASH_PERIOD_SLOW = 1000;  // ms, full on+off cycle
static const uint32_t FLASH_PERIOD_FAST = 150;

// Buzzer follows the LED exactly, but capped so a fast flash chirps instead of
// droning. Set to a large value to make it perfectly 1:1 with the LED.
static const uint32_t BUZZER_MAX_ON_MS  = 70;

// Set false to run the LED silently (bench testing without the noise).
static const bool BUZZER_ENABLED = true;

// Many cheap "active buzzer" modules trigger on a LOW signal, not HIGH
// (PNP driver stage). Bring-up on this board found the buzzer droning
// continuously at idle and only going silent on the brief HIGH POST pulse -
// flip this if yours is wired the other way.
static const bool BUZZER_ACTIVE_LOW = true;

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------
static const uint32_t UI_REFRESH_MS = 200;
