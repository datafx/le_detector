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
static const uint8_t PIN_BOOT    = 0;    // onboard BOOT button, active-low,
                                          // has an onboard pull-up. Only a
                                          // strapping pin during power-on/reset;
                                          // free to use as a normal input once
                                          // setup() is running.

// ---------------------------------------------------------------------------
// Input (BOOT button)
//
// One button, two gestures: tap = mute the current alert, hold = cycle band
// mode. Debounced in loop() (not an ISR) since it's read in a moving vehicle.
// ---------------------------------------------------------------------------
static const uint32_t BUTTON_DEBOUNCE_MS = 40;
static const uint32_t BUTTON_HOLD_MS     = 1000;  // extend if 1s proves too easy to hit accidentally

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
static const uint32_t WIFI_PHASE_MS       = 3500;  // ~1 full sweep of ch 1-11, plus slack
static const uint32_t BLE_PHASE_MS        = 3000;
static const uint32_t CHANNEL_HOP_MS      = 250;   // 11 ch * 250 ms = 2.75 s
// US 2.4GHz WiFi is FCC-licensed on channels 1-11 only; 12-13 are ETSI-region
// channels no US-market AP will legally beacon on, so scanning them is wasted
// dwell time.
static const uint8_t  WIFI_MAX_CHANNEL    = 11;

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
// Pure decay timer: resets to full duration on every qualifying receive
// (any matched device, not just the one that originally tripped the alert),
// never a latch. Dual mode needs enough slack to survive a full WiFi<->BLE
// round trip (~7s theoretical) plus missed-frame margin; single mode only
// has to smooth over BLE's intermittent duty-cycled advertising.
static const uint32_t DUAL_MODE_HOLD_MS   = 11000;
static const uint32_t SINGLE_MODE_HOLD_MS = 5500;

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
