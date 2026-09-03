# LE Gear Detector — ESP32-WROOM-32U

Passive BLE + WiFi scanner. Watches for MAC OUIs on a vendor watchlist and
signals hits on an LED and buzzer, flashing faster as the signal strengthens.

## Build

    pio run -t upload
    pio device monitor

## Wiring

| Signal | Pin | Header |
|---|---|---|
| OLED SDA | GPIO21 | J3 pin 6 |
| OLED SCL | GPIO22 | J3 pin 3 |
| Alert LED (+270R) | GPIO25 | J2 pin 9 |
| Buzzer SIG | GPIO26 | J2 pin 10 |

Power via micro-USB. EN is left unwired.

## Parts list

| Part | Notes |
|---|---|
| ESP32-WROOM-32U DevKitC (38-pin) | The `-U` variant — external antenna via U.FL, not the PCB-antenna `-D` variant |
| SSD1306 128x64 OLED, I2C | Any standard 4-pin (GND/VCC/SCL/SDA) module |
| LED, any color | + 270R resistor, GPIO25 to GND |
| Active buzzer module, 3.3–5V | Must be an *active* module (built-in oscillator, just needs a logic level), not passive/piezo. Check `BUZZER_ACTIVE_LOW` in `config.h` against your specific module — some trigger on LOW (see Gotchas below) |
| U.FL → RP-SMA pigtail | Connects the WROOM-32U's onboard U.FL to an external antenna |
| Alfa APA-M25 (8 dBi directional panel) or equivalent 2.4 GHz antenna | RP-SMA connector |
| Breadboard + jumper wires | Or a PCB — see the EN-pin note below if you're laying one out |
| Micro-USB cable | Power + programming; no external supply needed |

No 5V rail needed anywhere — the buzzer runs off the same 3V3 the DevKitC
outputs on J2 pin 1, and everything else is 3.3V logic.

**Building a PCB instead of breadboarding?** Add a 100nF–1µF ceramic
capacitor between EN and GND. The reference build runs EN bare (just its
onboard pull-up) and hit a burst of spurious resets that pointed at EN-pin
noise on the breadboard's high-impedance wiring — see the reset-loop note in
`CLAUDE.md` for the full writeup. The cap is cheap, standard practice on
ESP32 designs, and worth including on a PCB layout regardless of whether
that was the actual cause on the breadboard.

## Watchlist

65 IEEE-registered prefixes across 40+ law-enforcement equipment vendors (63
compiled in by default — see below). Any match on the list alerts the same
way — there's no confidence tier, since a weak signal still needs a reaction.
Each entry is tagged by how exclusively its vendor sells to LE (**LE-only**
vs **broad**, meaning it also ships on large volumes of civilian gear) purely
as curation info: broad vendors that turn out to be a false-positive source
get compiled out entirely (see the `EXCLUDE_VENDOR_*` toggles in
`oui_table.cpp` — CradlePoint is off by default for exactly this reason)
rather than downgraded.

Supports /24 (MA-L), /28 (MA-M) and /36 (MA-S) assignments. Prefixes shown with
a trailing `N_` are sub-/24 blocks where only the high nibble of the next byte is
significant. Keep the array sorted ascending; lookup is a binary search. Run
`test/test_oui.cpp` after edits.

Don't want a specific entry? Just comment out its row in `oui_table.cpp` —
removing a row can't break the sort order, so no rebuild flag or macro is
needed. Want to add one locally before submitting it upstream? Drop it in
`user_oui_table.cpp` instead, which is unsorted and just appended to.

### Complete list

| Prefix | Vendor | Category | Specificity |
|---|---|---|---|
| `00:00:C3` | Harris | Radio | broad |
| `00:04:7D` | Motorola Sol | Radio | broad |
| `00:06:EC` | Harris | Radio | broad |
| `00:08:B8` | EF Johnson | Radio | LE-only |
| `00:09:BC` | Utility Inc | Body/car cam | LE-only |
| `00:0A:3E` | EADS Telecom | Radio | broad |
| `00:0D:4F` | Kenwood | Radio | broad |
| `00:0D:CA` | Tait | Radio | broad |
| `00:0E:06` | Team Simoco | Radio | broad |
| `00:12:E0` | Codan | Radio | broad |
| `00:14:3E` | AirLink | Veh router | broad |
| `00:14:91` | Codan Radio | Radio | broad |
| `00:16:ED` | Utility Inc | Body/car cam | LE-only |
| `00:17:28` | Selex Comms | Radio | broad |
| `00:17:3D` | Neology | Plate reader | broad |
| `00:17:F3` | Harris | Radio | broad |
| `00:18:85` | Motorola Sol | Radio | broad |
| `00:1A:08` | Simoco | Radio | broad |
| `00:1C:3C` | Seon Design | Body/car cam | LE-only |
| `00:1D:96` | WatchGuard Vid | Body/car cam | LE-only |
| `00:1E:96` | Sepura | Radio | broad |
| `00:1F:92` | Motorola Sol | Radio | broad |
| `00:1F:9C` | Havis | Veh router | broad |
| `00:22:AF` | Safety Vision | Body/car cam | broad |
| `00:23:B9` | Airbus D&S | Radio | broad |
| `00:23:BD` | Digital Ally | Body/car cam | LE-only |
| `00:24:39` | Digital Barr | Body/car cam | LE-only |
| `00:24:E6` | In Motion Tech | Veh router | broad |
| `00:25:DF` | Axon | Body/car cam | LE-only |
| `00:26:B3` | Thales | Radio | broad |
| `00:30:44` | CradlePoint | Veh router | broad |
| `00:30:7E` | Redflex | Plate reader | LE-only |
| `00:90:C7` | Icom | Radio | broad |
| `00:A0:D5` | Sierra Wireless | Veh router | broad |
| `00:BF:15` | Genetec | Plate reader | broad |
| `00:E0:1C` | CradlePoint | Veh router | broad |
| `08:3C:03:0_` | Federal Signal | Lightbar | LE-only |
| `0C:BF:15` | Genetec | Plate reader | broad |
| `10:74:6F` | Motorola Sol | Radio | broad |
| `1C:82:59:D_` | Stalker Radar | Radar | LE-only |
| `28:A3:31` | Sierra Wireless | Veh router | broad |
| `38:73:EA:0_` | L3 MobileVis | Body/car cam | LE-only |
| `48:46:8D` | Zepcam | Body/car cam | LE-only |
| `4C:CC:34` | Motorola Sol | Radio | broad |
| `50:13:9D` | Sierra Wireless | Veh router | broad |
| `58:94:CF` | Vertex Std LMR | Radio | broad |
| `58:E8:76:C_` | Kustom Signals | Radar | LE-only |
| `64:69:BC` | Hytera | Radio | broad |
| `64:CE:6E` | Sierra Wireless | Veh router | broad |
| `68:DA:73:B_` | Gamber-Johnson | Veh router | broad |
| `6C:18:11` | Decatur Elec | Radar | LE-only |
| `70:B3:D5:1C:5_` | ELSAG | Plate reader | LE-only |
| `70:B3:D5:88:A_` | Perceptics | Plate reader | LE-only |
| `84:DB:2F` | Sierra Wireless | Veh router | broad |
| `9C:06:6E` | Hytera | Radio | broad |
| `9C:83:BF` | PRO-VISION | Body/car cam | LE-only |
| `9C:86:2B` | Motorola Sol | Radio | broad |
| `A8:C0:EA` | Pepwave | Veh router | broad |
| `B8:E2:8C` | Motorola Sol | Radio | broad |
| `BC:AD:90` | Kymeta | Veh router | broad |
| `CC:93:4A` | Sierra Wireless | Veh router | broad |
| `D4:13:F8` | Peplink | Veh router | broad |
| `E0:DA:DC` | JVC Kenwood | Radio | broad |
| `E4:1E:0A:B_` | Safety Vision | Body/car cam | broad |
| `FC:01:9E` | VieVu | Body/car cam | LE-only |

### Provenance

Every prefix is an IEEE MA-L/MA-M/MA-S assignment, verified against the
registrant's street address (not vendor-name guesses) via an IEEE registry
sweep for known LE equipment makers.

### Deliberately excluded

- **Dell, Panasonic / Panasonic Connect** — Toughbook/laptop MDTs are ubiquitous
  patrol gear, but these prefixes sit on tens of millions of consumer machines,
  so a hit carries no information.
- **WatchGuard Technologies** (`00:01:21`, `00:90:7F`) — Seattle firewall vendor,
  unrelated to WatchGuard Video (Plano TX, police video), which *is* included.
- **Coban SRL** (Italy) — not COBAN Technologies (Houston police video), which has
  no IEEE assignment.
- **TAIT Global LLC** (Lititz PA) — not Tait Electronics (NZ radios), which is
  included.
- Peplink/Pepwave and similar vehicle-router vendors are included but kept at the
  broad tier for the same reason as Sierra Wireless/CradlePoint.

### Still absent from IEEE entirely

Whelen, Code 3, SoundOff Signal, MPH Industries, Getac, and COBAN Technologies
(Houston) have no IEEE assignments — most lightbars and many radar heads aren't
IP devices at all, so there's nothing to match on.


## Known limits

- **One radio.** WiFi and BLE cannot run at once. The firmware alternates
  (`WIFI_PHASE_MS` / `BLE_PHASE_MS`); during each window it is blind to the other.
- **MAC randomisation.** Only public BLE addresses and non-randomised WiFi MACs
  carry a real OUI. Randomised addresses are skipped — their vendor bits are
  meaningless. This limits how much is detectable regardless of watchlist size.
- Repeated WiFi/BLE stack init+deinit every few seconds is necessary given the
  single shared radio, but watch for heap fragmentation on long runs.

## Tuning

Everything lives in `include/config.h`: phase lengths, channel dwell,
flash-rate endpoints (`RSSI_WEAK`/`RSSI_STRONG`, `FLASH_PERIOD_SLOW`/`FAST`),
alert hold time, and `BUZZER_ENABLED` for silent bench testing.

## License

GPLv3 (see `LICENSE`), matching the companion `rf_stalker` and
`police_oui_watchlist` projects.

`src/detector.cpp` and `src/main.cpp` carry their own
`SPDX-License-Identifier: MIT` line for portions adapted from third-party
code — see the header comment in each file for attribution, what was
adapted, and what changed. Everything else in this repo is GPLv3.
