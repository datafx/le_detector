# CLAUDE.md — LE Gear Detector

Session start: **"Read CLAUDE.md and continue."**

Decisions get locked before code, one item at a time. If something in
"Open decisions" is unresolved, resolve it explicitly before writing code that
depends on it. Don't silently pick a default.

---

## What this is

A windshield-mounted passive detector on an ESP32-WROOM-32U. It scans BLE
advertisements and 802.11 management frames, matches transmitter MAC OUIs
against a vendor watchlist, and signals hits on an OLED, an LED, and a buzzer.

Derived in part from **nyanBOX** (<https://github.com/jbohack/nyanBOX>),
Copyright (c) 2025 jbohack, MIT. Specifically the BLE GAP scan structure from
`axon_detector.cpp` and the WiFi promiscuous handler + channel hop from
`device_scout.cpp`. Attribution headers are in `src/detector.cpp` and
`src/main.cpp` — keep them.

Companion to two existing projects: `rf_stalker` (P25/RF proximity detector on
the uConsole) and `police_oui_watchlist` (Scapy-based OUI alerter). This is the
standalone embedded version of the same idea.

---

## Status

Builds, flashes, and has been through the full bring-up sequence below on real
hardware: boot screen, POST, phase indicator, frame counter, a spoofed-MAC
alert test (WiFi, Axon OUI), and flash-rate scaling with RSSI. All passed.

Two fixes were needed to get there, both already applied: the default
`esp32dev` partition table doesn't leave enough room for this firmware (see
Gotchas), and this board's buzzer module turned out to be active-low (see
Gotchas, `BUZZER_ACTIVE_LOW`).

---

## Hardware

ESP32-WROOM-32U DevKitC (38-pin), external 2.4 GHz antenna via U.FL → RP-SMA
pigtail to an Alfa APA-M25 8 dBi directional panel.

| Signal | GPIO | Header | Notes |
|---|---|---|---|
| OLED SDA | 21 | J3 pin 6 | SSD1306 128x64 I2C |
| OLED SCL | 22 | J3 pin 3 | |
| Alert LED | 25 | J2 pin 9 | via 270R to GND |
| Buzzer SIG | 26 | J2 pin 10 | active module, 3.3–5V |
| 3V3 | — | J2 pin 1 | **output**, feeds the + rail |
| GND | — | J2 p14 / J3 p1 | |

- Powered from the micro-USB port; onboard regulator drives 3V3.
- **EN is deliberately unwired.** It has an onboard pull-up. Tying it to 3V3
  breaks the USB auto-reset/bootloader circuit and stresses the reset transistor.
- Buzzer runs on the 3V3 rail (module is rated 3.3–5V), so nothing needs 5V.

Breadboard wiring diagram: `esp32_breadboard_wiring.pdf`.

---

## Locked decisions

1. **Framework**: Arduino-ESP32 under PlatformIO, calling raw `esp_wifi.h` /
   `esp_gap_ble_api.h` directly. Matches the nyanBOX reference and gives u8g2.

2. **One radio, time-sliced.** The ESP32 has a single 2.4 GHz radio. WiFi and
   BLE cannot both be up. The firmware fully tears one stack down before
   starting the other (`WIFI_PHASE_MS` 3500 / `BLE_PHASE_MS` 3000). This is what
   nyanBOX does too. Consequence: during each window we are blind to the other
   band. Do not "fix" this by trying to run both — it isn't possible.

3. **BLE scan is PASSIVE.** nyanBOX ships `BLE_SCAN_TYPE_ACTIVE`, which
   transmits scan requests. For a receive-only detector that's wrong. Flip via
   `BLE_SCAN_PASSIVE` in config.h if ever needed.

4. **Only non-randomised addresses are matched.** BLE: public addresses only
   (`BLE_ADDR_TYPE_PUBLIC`). WiFi: the locally-administered bit (`mac[0] & 0x02`)
   is rejected. Randomised addresses have random vendor bits, so OUI matching
   them is meaningless. This is a hard ceiling on what's detectable and is
   **not** a bug to fix.

5. **SSID filter removed.** nyanBOX drops frames with an empty SSID. We key on
   OUI, not SSID, so that filter would lose beacons for no reason.

6. **Fixed device table, LRU eviction.** 48 entries, no dynamic allocation
   (nyanBOX uses `std::vector`). RSSI is EMA-smoothed 1/3 so one stray frame
   doesn't swing the flash rate.

7. **Confidence**: OUI hit on a non-randomised address = MEDIUM. Seen
   `HIGH_CONF_HITS` (3) times AND vendor is `SPEC_LE_ONLY` = HIGH.
   `SPEC_BROAD` vendors (Motorola, Sierra Wireless, Harris, Hytera, CradlePoint,
   Tait) are **capped at MEDIUM forever** — they sell huge volumes of civilian
   gear, and repetition doesn't turn a barcode scanner into a police radio.

11. **Prefix lengths.** The table supports /24 (MA-L), /28 (MA-M) and /36
    (MA-S). Kustom Signals, Stalker, Federal Signal, Gamber-Johnson and Safety
    Vision hold only /28s; ELSAG and Perceptics only /36s inside the shared
    `70:B3:D5` block — which hosts hundreds of unrelated companies, so 3-byte
    matching there would be catastrophic for false alerts. Lookup binary-searches
    the first 3 bytes, then walks the run applying the full mask. Unit-tested.

13. **Excluded as too broad**: Dell and Panasonic. Toughbook/laptop MDTs are
    ubiquitous in patrol cars, but those prefixes sit on tens of millions of
    consumer machines — a hit carries no information. Same reasoning for
    Peplink, Jenoptik, Sensys Networks, TYT, Zetron, Codan, Simoco.

12. **Name-collision exclusions** (documented in `oui_table.cpp`): WatchGuard
    Technologies (Seattle firewalls) vs WatchGuard Video (Plano, police video);
    Coban SRL (Italy) vs COBAN Technologies (Houston); TAIT Global LLC
    (Lititz PA) vs Tait Electronics (NZ radios). Don't "helpfully" re-add these.

8. **Alert model**: LED and buzzer driven from one shared phase variable so they
   cannot drift apart. Flash period scales linearly with the strongest active
   RSSI — weak = slow, strong = fast. 4 s self-clearing hold prevents chattering
   on intermittent BLE adverts.

9. **nRFBox dependencies stripped**: RF24 radios, 5-button nav, `display_mirror`,
   `sleep_manager`, `level_system`. None of that hardware exists here.

10. **No serial logging / survey mode.** Explicitly rejected — this is a
    standalone alerter, not a capture device.

14. **Licence: GPLv3.** Matches the companion `rf_stalker` and
    `police_oui_watchlist` projects. `src/detector.cpp` and `src/main.cpp`
    keep their `SPDX-License-Identifier: MIT` line for the nyanBOX-derived
    portions (attribution preserved per the header comment); everything else
    is GPLv3. See the README's License section.

---

## Open decisions

- **Watchlist coverage.** 65 entries across 40+ vendors (18 LE-only, 29 broad),
  all IEEE registry assignments, address-verified. Still absent from the IEEE
  data entirely: Whelen, Code 3, SoundOff Signal, MPH Industries, EF Johnson,
  BK Technologies, Getac, COBAN Technologies (Houston) — most lightbars and
  many radar heads aren't IP devices at all. 20 entries were cross-confirmed
  against nyanBOX's own (XOR-obfuscated) firmware list; see
  tools/decrypt_nyan_ouis.py and the README Provenance section.

- **Phase timing under real driving conditions.** 3.5 s WiFi + 3 s BLE plus
  ~200–400 ms of stack transitions means a ~7 s revisit interval. At 65 mph
  that's ~670 ft between looks at either band. Untested; may need shortening.

---

## Layout

```
platformio.ini
include/
  config.h        all tuning: pins, phase timing, flash rate, thresholds
  detector.h      TrackedDevice, DetectorStatus, phase control
  alert.h         LED/buzzer state machine
  ui.h            OLED
  oui_table.h     OuiEntry, GearCategory, lookup
src/
  main.cpp        setup/loop, WiFi<->BLE phase machine
  detector.cpp    BLE GAP callback, WiFi promiscuous handler, device table
  alert.cpp       RSSI -> flash period, synced LED+buzzer drive
  ui.cpp          status screen
  oui_table.cpp   THE WATCHLIST
```

All tuning lives in `config.h`. Prefer changing constants there over editing
logic.

---

## Testing the OUI table off-target

`src/oui_table.cpp` has no Arduino dependency, so it compiles and runs on the
host. Worth re-running after any table edit:

```bash
g++ -std=c++11 -o /tmp/test_oui test/test_oui.cpp src/oui_table.cpp -I include
/tmp/test_oui
```

Covers the anchor entry, first/last rows, an unknown prefix, the /28 and /36
in-block / out-of-block boundaries, and disambiguation of two vendors sharing
the `70:B3:D5` block. 25 assertions, all passing as of handoff.

## Build

```bash
pio run                  # compile
pio run -t upload        # flash
pio device monitor       # 115200
```

If upload fails: hold **BOOT**, tap **EN**, release BOOT. Should not normally be
needed — that's the whole reason EN is left unwired.

---

## Bring-up sequence

Validate in this order; don't skip ahead when something fails.

1. **Boot screen** — confirms OLED wiring, I2C address, u8g2. Shows the OUI
   table size, so you can tell at a glance whether you flashed the table you
   meant to.
2. **Power-on self test** — LED + buzzer pulse ~120 ms in `setup()`. Confirms
   both outputs and that they fire together.
3. **Radio phase indicator** — top-right of the display should alternate between
   `WiFi chNN` (cycling 1→13) and `BLE`. If it sticks, a stack failed to init.
4. **Frames seen counter** — on the idle screen. Should climb steadily anywhere
   near WiFi. If it stays at 0, promiscuous mode isn't running.
5. **Alert path** — with only Axon in the table you likely won't trip it
   naturally. To test the LED/buzzer/display path, temporarily add the OUI of a
   device you own (phone hotspot, laptop) as a `CAT_OTHER` entry, confirm the
   alert behaves, then remove it.
6. **Flash-rate scaling** — walk the test device closer/further and confirm the
   flash speeds up as RSSI rises.

---

## Gotchas

- `esp_wifi_init`/`deinit` and bluedroid init/deinit run every few seconds.
  nyanBOX does the same, but watch heap over long runs
  (`ESP.getFreeHeap()`); fragmentation is the plausible failure mode for a
  device meant to run for hours in a car.
- The WiFi promiscuous callback is marked `IRAM_ATTR` and runs in the WiFi task.
  Keep it short. Don't add `Serial.print` or anything blocking to it.
- `DetectorStatus.best` points into the static device array. Valid within one
  loop iteration only; don't cache it across calls to `detectorExpire()`.
- `esp_ble_gap_start_scanning(0)` scans until explicitly stopped. The scan is
  started from the GAP callback after params are set, not inline.
- OLED header bar inverts during an alert; anything drawn on it needs
  `setDrawColor(0)` or it's white-on-white. Already bitten once.
- The default `esp32dev` partition table reserves space for dual-OTA and only
  gives ~1.3MB to the app, which this firmware exceeds. `platformio.ini` sets
  `board_build.partitions = huge_app.csv` (single ~3MB app partition, no OTA,
  which this project doesn't need).
- Some active buzzer modules trigger on LOW, not HIGH. If the buzzer drones
  continuously at idle and only goes quiet during the POST pulse, flip
  `BUZZER_ACTIVE_LOW` in `config.h`. All buzzer writes go through
  `alertSetBuzzer()` in `alert.cpp` so this is a one-line fix.
