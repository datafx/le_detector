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

7. **No confidence tiers — alert or no alert. Revised 2026-09-02.**
   Originally: OUI hit on a non-randomised address = MEDIUM; seen
   `HIGH_CONF_HITS` (3) times AND vendor is `SPEC_LE_ONLY` = HIGH;
   `SPEC_BROAD` vendors capped at MEDIUM forever. Removed: raised while
   discussing the CradlePoint false-positive problem — a low-confidence hit
   either is real gear or it isn't, and the driver reacts to it either way,
   so a MEDIUM/HIGH split wasn't buying anything operationally. It also
   turned out to already be dead code: `bestConfidence` was computed in
   `detector.cpp` but never read by `alert.cpp`, `ui.cpp`, or `main.cpp` —
   no display, no behavior gating, ever. The project's actual answer to "this
   vendor is unreliable" had already become binary per-vendor
   include/exclude (see the `EXCLUDE_VENDOR_*` toggles below and the
   CradlePoint default flip), which supersedes what the confidence tier was
   trying to do. `Confidence` enum, `TrackedDevice.confidence`,
   `TrackedDevice.hits` (only existed to feed `HIGH_CONF_HITS`), and
   `DetectorStatus.bestConfidence` are gone. `Specificity`
   (`SPEC_LE_ONLY`/`SPEC_BROAD`) **stays**, but is now compile-time curation
   metadata only (see `oui_table.h`) — it informs which `SPEC_BROAD` vendors
   are worth an exclude toggle if they turn out to be a false-positive
   source, the way CradlePoint did; it no longer drives any runtime state.

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

- **Reset-loop bursts, root cause probably EN-pin noise (not brownout).**
  Reported 2026-09-01 as "sometimes boots twice"; first pass through this
  item (below) treated it as an occasional double-reset, but a live capture
  caught the real event and it's worse than that framing suggested.

  Live evidence, captured while the board was idle and connected to this PC:
  a burst of **~135 rapid resets in well under 20 seconds** (counted via
  repetitions of the ROM's `entry 0x400805e4` boot line, corrupted/overlapping
  in the raw capture because the resets were faster than one boot banner
  could finish printing), then it recovered on its own — a follow-up 15s
  capture immediately after showed zero further resets, fully stable.
  `journalctl -k` showed no USB disconnect/reconnect around the event, so
  the CP2102/USB link and the board's power stayed up throughout; this was
  the ESP32 itself resetting, not a cable or power interruption. Every
  *clean* (non-corrupted) reset-reason string captured — both during this
  event's edges and in earlier isolated attempts — read `POWERON_RESET`,
  never `RTCWDT_BROWN_OUT_RESET` and never a watchdog/software-reset code;
  no panic or Guru Meditation output anywhere.

  `POWERON_RESET` is specifically what the ROM reports for an EN-pin
  transition. No power loss + a rapid burst of EN-triggered resets +
  spontaneous recovery points at **EN-pin electrical noise**, not the
  brownout hypothesis from the original pass: EN is deliberately just tied
  to its onboard pull-up with no external decoupling capacitor (hardware
  notes above), sitting on a breadboard-on-console rig — a high-impedance
  net in exactly the kind of setup that's easy for a noise transient to
  glitch. Not proven (no way to directly scope the EN pin from here), but
  well-supported by everything observed so far.

  **Next step, not yet done: add a small capacitor (100nF-1µF ceramic)
  between EN and GND** — the standard fix for this exact symptom on ESP32
  dev boards, and cheap/low-risk to try. If bursts continue afterward,
  that would point back toward the brownout/power-supply hypothesis instead
  (try a different power source/cable at that point). Diagnostic technique
  for next time, if needed: `python3` + `pyserial` with `dtr=False,
  rts=False` on open (a plain `cat`/`stty` open was found to hold the board
  in reset the whole time it's open, producing a solid stream of null bytes
  instead of real boot output); read `ser.in_waiting` immediately on open in
  case a banner is already sitting in the kernel's buffer from a prior
  event, then poll for a longer window to see if it's still active.

---

## Next iteration — planned changes (not yet designed)

Requested 2026-09-01 after the first real-world test drive: some local PD
picked up correctly, some local PD missed entirely, and false positives from
what looked like school buses and Royal Truck Equipment (plausibly SPEC_BROAD
vendors like CradlePoint/Sierra Wireless/Motorola — common in fleet
telematics and pupil-transportation WiFi, not just LE gear). Each item below
needs to be locked individually before code, per the rule at the top of this
file — do not implement any of these by silently picking a default.

- **~~Revisit decision 10 (no logging)~~ — scrapped, decision 10 stands.**
  Considered a bounded in-RAM alert-history ring buffer, reviewable on-device
  via the BOOT button, to close the diagnosability gap. Rejected 2026-09-01:
  not wanted in the end product, and it was pulling in real complexity —
  specifically, fitting a third button gesture (enter/browse history)
  alongside mute and mode-cycle on one button raised a genuine safety
  concern (see note under the mute item below) that wasn't worth solving for
  a feature nobody wants long-term. The actual diagnosability problem —
  screen too small/hard to see on the current temporary mount — is being
  solved directly by repositioning the device in the vehicle for better
  visibility, not by adding on-device logging. Do not re-propose this.

- **Band-select option: WiFi only / BLE only / both. Implemented 2026-09-01**
  (`main.cpp`: `BandMode` enum, `applyBandMode()`/`cycleBandMode()`, NVS via
  `Preferences` namespace `"ledet"` key `"bandMode"`). Decision 2 (one radio,
  time-sliced) still holds — this doesn't run both bands at once, it just
  optionally skips one phase of the alternation. Mechanism decided: runtime
  control via hold-1s on the BOOT button, cycling WiFi-only → BLE-only →
  both → ... **Persists across power cycles via NVS** (Arduino `Preferences`
  library, wrapping the existing 20 KB `nvs` partition already reserved in
  `huge_app.csv` — currently unused, no partition table change needed). This
  is a single stored preference value, not a history/capture feature —
  doesn't reopen decision 10.

  Deliberately **not** a `config.h` constant like the real tunables
  (`BUZZER_ACTIVE_LOW` etc.) — a constant there would imply editing it
  changes behavior on every boot, but it would only ever apply once, on a
  blank NVS, and silently do nothing on every boot after (NVS wins). Instead,
  the one-time bootstrap value (**both**) is just the inline default arg to
  the NVS read, e.g. `preferences.getUChar("bandMode", BAND_BOTH)` — an
  implementation detail, not an exposed setting.

  **Header mode indicator added 2026-09-01** so a BOOT-hold actually shows
  feedback: fixed left-aligned position in the header bar (x=70, clear of
  ALL CLEAR/** ALERT **), reading `AUTO: WiFi` / `AUTO: BLE` (alternating
  with the phase in dual mode — relabeled from `BOTH:` after the first pass)
  or a static `WiFi ONLY` / `BLE ONLY` in single-band mode (`main.cpp`
  resolves the label, `uiRender()` takes it as a `const char*`). Originally
  right-aligned, but that made the whole label visibly jump left/right every
  phase switch in dual mode since the two variants differ in width —
  left-aligned keeps the shared `AUTO: ` prefix planted and only the
  trailing word's width changes.

- **Alert hold: pure decay timer, never a latch, duration depends on band
  mode. Implemented 2026-09-01** as `DUAL_MODE_HOLD_MS = 11000` /
  `SINGLE_MODE_HOLD_MS = 5500` in `config.h`; `main.cpp` resolves which one
  applies each loop and passes it into `alertUpdate(st, holdMs)`, which now
  drops the old `activeCount > 0` requirement so the decay is purely
  time-based. Starting values per the caveat below — not field-verified yet.
  Previously a 4 s self-clearing hold (decision 8) prevented chattering
  on intermittent BLE adverts. Problem: in dual-band mode, the ~7 s WiFi+BLE
  revisit cycle can outlast the hold, dropping an alert for a device that's
  still there but simply wasn't in view during the last phase window.
  Clarified 2026-09-01: this is **not** a latch with a revisit-check — no
  phase/origin tracking needed at all. It's a single countdown that resets to
  its full duration on **every** qualifying receive, regardless of whether
  it's the same device/OUI that originally tripped it or a different one
  (this is purely to stop on/off flicker, not per-device tracking). The alert
  stays active exactly as long as the countdown hasn't hit zero; no separate
  "clear" check, no indefinite hold — it always decays.

  - **Dual mode (WiFi+BLE alternating):** `DUAL_MODE_HOLD_MS` ≈ **10 s+**.
    Deliberately generous — well past the ~7 s theoretical round trip (rest
    of current phase + full opposite phase + transition overhead) — to cover
    missed frames/packets during the jump between bands, not just the
    minimum needed to survive one clean round trip.
  - **Single mode (WiFi-only or BLE-only):** `SINGLE_MODE_HOLD_MS` ≈ 5–6 s,
    since there's no band-hop blind window to cover — this is purely to
    smooth over BLE's intermittent duty-cycled advertising (WiFi beacons from
    actual APs/routers are ~100 ms and rarely need this much slack).

  Both are starting values, not field-verified — same caveat as the
  phase-timing item above; expect to tune after a real test drive. A mode
  change mid-countdown (via BOOT hold) doesn't need special-case handling —
  the countdown just keeps running and is refreshed or expires independent of
  which band is currently active.

- **Temporary mute. Implemented 2026-09-01** (`main.cpp`: `pollButton()`
  debounces `PIN_BOOT` and distinguishes tap vs. hold; `alertMute()` in
  `alert.cpp`). **Implementation note, not explicitly locked before coding:**
  mute silences the buzzer only — the LED keeps flashing — on the reasoning
  that "mute" is an audio term and losing the visual cue on a windshield
  alerter would undercut the point of the device. Flag if that's not what was
  intended; swapping to silence both outputs is a one-line change in
  `alert.cpp`'s `driveOutputs()`. Input hardware decided: the onboard **BOOT button
  (GPIO0)** on the DevKitC — reachable on the current breadboard-on-console
  setup, already wired with a pull-up, no new hardware needed. GPIO0 only
  matters as a strapping pin during power-on/reset (must not be held down
  then, or the board enters download mode instead of booting); after
  `setup()` it's a free digital input, active-low. Needs debounce in `loop()`
  since it's in a moving vehicle (vibration risk), not an ISR — no conflict
  with the WiFi IRAM callback.

  Gesture scheme decided: **single press = mute, press-and-hold 1s = cycle
  band mode** (WiFi only / BLE only / both). One button covers both for now;
  a second button is an option to revisit later if the gestures turn out to
  be hard to hit reliably while driving, not something to add preemptively.

  Mute semantics decided: mute silences only the **currently active** alert,
  not a fixed duration and not a global "quiet mode." If that alert clears
  (device ages out / hold expires per decision 8) and a new alert condition
  fires afterward — same device seen again or a different one — alerting
  resumes normally. Mute does not persist across alert instances.

  Accidental-mode-change risk (a slow mute tap crossing the 1s hold
  threshold) was raised while a third gesture (history review) was still in
  the mix — with the plain two-gesture scheme (tap = mute, hold = mode-cycle)
  it's not a real concern. No mitigation needed now; if 1s turns out too
  short in practice, just extend the hold threshold.

- **WiFi channel range: drop 12–13. Implemented 2026-09-01**
  (`WIFI_MAX_CHANNEL` 13 → 11 in `config.h`). Raised 2026-09-01 — user noticed the
  header only ever seemed to show low channel numbers and asked whether
  hopping was actually covering all 13. It is (confirmed by reading
  `detectorHopChannel()`/`config.h`); the appearance is a perception
  artifact, not a bug — 250 ms/channel is too fast to read while driving,
  and the eye tends to catch the display right as it flips from `BLE` to
  `WiFi`, which is always freshly reset to ch1. Separately: US 2.4GHz WiFi
  is FCC-licensed on channels 1–11 only; 12–13 are ETSI-region channels no
  US-market AP will legally beacon on. Recommendation locked in discussion,
  not yet applied: `WIFI_MAX_CHANNEL` 13 → 11 in `config.h`. Shortens the
  full sweep from 3.25s to 2.75s inside the existing 3.5s `WIFI_PHASE_MS`
  window (harmless — channels 1–2 just get one bonus extra look per phase).

- **Header display: drop the WiFi channel number. Implemented 2026-09-01**
  (`ui.cpp`: static `"WiFi"`/`"BLE"` label instead of `WiFi chNN`). Confirmed
  on hardware after flashing — much more readable at a glance. Bring-up
  sequence step 3 updated to note `detectorChannel()` still exists for a
  future re-bring-up if needed, just not wired into the display by default.

- **Per-vendor compile-time exclude toggles. Implemented 2026-09-01**
  (`oui_table.cpp`: `EXCLUDE_VENDOR_CRADLEPOINT` / `_SIERRA_WIRELESS` /
  `_MOTOROLA`, all commented out by default, each relevant row wrapped in
  `#ifndef EXCLUDE_VENDOR_<NAME>`). Raised alongside the false positives from
  the first real-world test drive — CradlePoint (and potentially other
  `SPEC_BROAD` vendors like Sierra Wireless, Motorola) showing up on school
  buses and commercial vehicles doing fleet telematics, not LE gear. Decided:
  don't strip these from the shared table, since they're legitimately used
  by LE too and useful for the maintainer's own testing — instead, give
  users a way to opt individual vendors out before they compile. Table sort
  order is unaffected either way (removing rows can't break the binary
  search); verified with `-DEXCLUDE_VENDOR_CRADLEPOINT` that table size drops
  and lookups for that vendor return `nullptr`. Only these three vendors got
  toggles (the ones actually named as suspected false-positive sources) —
  not all 40+ table vendors; copy the same `#ifndef` pattern for another
  vendor if one shows up as a problem later.

  **CradlePoint flipped to excluded by default, 2026-09-02.** A second
  real-world test drive reported zero true positives on PD vehicles and a
  steady stream of false positives — school buses and cellular PTZ cameras
  on the turnpike, both ordinary CradlePoint fleet-router customers.
  `#define EXCLUDE_VENDOR_CRADLEPOINT` in `oui_table.cpp` is now uncommented
  (table size 65 → 63; verified via `test/test_oui.cpp`, all 25 assertions
  still pass). Sierra Wireless and Motorola are untouched — no false-positive
  reports against them specifically yet. This is a shipped-default change,
  not just a build-time knob: someone who wants CradlePoint back in must
  comment the `#define` back out (see the block comment above the toggles
  in `oui_table.cpp` for the "only if you've confirmed it locally" guidance)
  rather than opt into excluding it. If Sierra Wireless or Motorola produce
  the same kind of report, apply the same reasoning rather than leaving them
  included on the theory that CradlePoint was a one-off.

- **User-addable OUI staging table. Implemented 2026-09-01**
  (`src/user_oui_table.cpp`), for someone testing an OUI locally before
  submitting it upstream as a PR. Same `OuiEntry` struct as the main table,
  ships as an empty array with one commented-out example row showing the
  field layout — see that file's header comment for the full promotion
  workflow. `ouiLookup()` in `oui_table.cpp` now falls back to
  `ouiUserLookup()` (linear scan, no sorting needed) whenever the main
  table's binary search comes up empty; `detector.cpp` and every other
  caller are unchanged, they still just call `ouiLookup()`. Staged entries
  should default to `SPEC_BROAD` unless the user is confident the vendor is
  genuinely LE-only — curation metadata now, not a confidence tier, per the
  revised decision 7. Verified: uncommenting the example row makes
  `ouiLookup()` find it via the fallback path; existing `test/test_oui.cpp`
  suite still passes unchanged (the staging table is empty by default, so
  it never intercepts anything).

- **Comment-out-to-disable documented directly in `oui_table.cpp`.
  Implemented 2026-09-02.** Raised alongside the confidence-tier removal —
  wanted an easy way to enable/disable individual OUIs pre-compile "without
  doing anything crazy." Turned out this already worked: commenting out any
  single row in the main table cannot break the sort order or the binary
  search (removing an element leaves the rest sorted), so no new toggle
  machinery was needed — the gap was purely that it wasn't documented. Added
  a block comment above `OUI_TABLE` in `oui_table.cpp` saying so explicitly,
  and pointed the README at the same thing. The `EXCLUDE_VENDOR_*` macros
  remain for a different purpose — flipping a vendor off *by default* for
  everyone who builds the project (like the CradlePoint change above) — not
  a replacement for hand-commenting a row you personally don't want.

---

## Future ideas (beyond next iteration)

Deliberately kept separate from "Next iteration — planned changes" above —
these are things discussed and worth keeping, but not queued for the current
round of work. Same rule applies when they do get picked up: lock each open
sub-decision before writing code.

- **SSID matching from beacon frames and/or probe requests**, as a signal
  independent of OUI matching. Discussed 2026-09-01. Not a reversal of
  decision 5 (that was about not *dropping* frames with an empty SSID, not
  about never using SSID content).

  - **Beacon SSID** = the network name an AP broadcasts. Matching this against
    a watchlist (e.g. a known agency network-naming pattern) is additive
    coverage independent of whether that AP's OUI happens to be in the
    watchlist at all.
  - **Probe request SSID** = a client device leaking a network it previously
    joined, not evidence the network is nearby now. Modern phone OSes
    (iOS/Android, post ~2014-2017) mostly suppress directed probes now
    (randomized MAC + wildcard/no-SSID probes), so this signal increasingly
    only catches older/embedded hardware, not phones. Lower, shrinking value
    versus beacon-SSID matching — worth doing but not the priority half of
    this feature if only one gets built first.
  - **Match semantics locked**: prefix-only (trailing wildcard), e.g.
    `PSP-troop*` matches `PSP-troop7`, `PSP-troop8`, etc. Deliberately not
    full glob (no leading/embedded wildcards) — narrower implementation
    (plain `strncmp` against the pattern length, no regex/glob engine, no
    dynamic allocation), matches the only concrete example raised, and easy
    to extend later if a real case needs more than a trailing wildcard.
  - **Case sensitivity locked**: case-sensitive exact-byte match. No
    normalization needed.
  - **Table shape**: a separate flat list of literal prefix strings, linear
    scan per parsed SSID (not the OUI table's sorted/masked/binary-search
    structure — that machinery is for MAC prefixes specifically, and a small
    curated SSID list doesn't need it).
  - **Still open, not yet locked**: (1) where the match runs — inline in the
    `IRAM_ATTR` WiFi promiscuous callback (cheap, but that callback has an
    explicit "keep it short" rule in Gotchas) vs. deferred to the main loop;
    (2) whether an SSID hit alerts the same as an OUI hit or needs its own,
    lower-trust handling — decision 7 no longer has a MEDIUM/HIGH scheme to
    slot this into (removed 2026-09-02), so this needs a fresh answer, not a
    reuse of the old one.

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
  user_oui_table.cpp  empty-by-default staging table for testing an OUI
                      locally before submitting it as a PR (see below)
```

All tuning lives in `config.h`. Prefer changing constants there over editing
logic.

---

## Testing the OUI table off-target

`src/oui_table.cpp` and `src/user_oui_table.cpp` have no Arduino dependency,
so they compile and run on the host. Worth re-running after any table edit:

```bash
g++ -std=c++11 -o /tmp/test_oui test/test_oui.cpp src/oui_table.cpp src/user_oui_table.cpp -I include
/tmp/test_oui
```

Covers the anchor entry, first/last rows, an unknown prefix, the /28 and /36
in-block / out-of-block boundaries, and disambiguation of two vendors sharing
the `70:B3:D5` block. 25 assertions, all passing as of handoff.

To test a per-vendor exclude toggle, add `-DEXCLUDE_VENDOR_SIERRA_WIRELESS`
(or `_MOTOROLA`) to the `g++` line and confirm `ouiTableSize()` drops and
that vendor's lookups return `nullptr`. CradlePoint is excluded by default
now (see above) — its `#define` lives directly in `oui_table.cpp`, not a
build flag, so to test it *re-included* comment that line back out instead.
To test the user staging table, uncomment a row in `user_oui_table.cpp` and
confirm `ouiLookup()` finds it via the fallback path.

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
   `WiFi` and `BLE`. Static labels as of 2026-09-01 (previously `WiFi chNN`
   cycling 1→13; the live channel number read as "stuck" while driving more
   than it conveyed useful info). If it sticks on one label, a stack failed to
   init. `detectorChannel()` in `detector.cpp` still tracks the real channel
   (drives `esp_wifi_set_channel`) even though it's no longer displayed — wire
   it back into `uiRender()` temporarily if a future bring-up needs to see it
   cycling to confirm the hop logic itself.
4. **Frames seen counter** — `detectorSeenTotal()` in `detector.cpp`, no longer
   shown on the idle screen (dropped 2026-09-01 in the display redesign below,
   in favor of a bigger vendor name). Should climb steadily anywhere near
   WiFi. If it stays at 0, promiscuous mode isn't running. The counter itself
   is still tracked internally; if a future bring-up needs to see it again,
   temporarily wire it back into `uiRender()` for that session rather than
   restoring it to the shipped UI permanently.
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
