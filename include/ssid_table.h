// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <stdint.h>
#include "oui_table.h"   // GearCategory

// How a pattern is matched against a parsed SSID. Both are plain byte
// comparisons (no regex/glob engine, no dynamic allocation) - matches the
// OUI table's "as simple as the job needs" philosophy.
//
// SSID_MATCH_PREFIX   - pattern must match the start of the SSID, e.g.
//                       "IBR900-" matches "IBR900-1A2B3C".
// SSID_MATCH_CONTAINS - pattern may appear anywhere in the SSID.
enum SsidMatchMode : uint8_t {
    SSID_MATCH_PREFIX = 0,
    SSID_MATCH_CONTAINS
};

struct SsidEntry {
    const char*   pattern;   // case-insensitive, plain bytes, no wildcards
    SsidMatchMode mode;
    const char*   vendor;
    GearCategory  category;
};

// Case-insensitive lookup against the watchlist. `ssid` must be NUL-terminated
// (the caller already bounds-checks and copies the raw IE bytes into a fixed
// buffer before calling this). Returns nullptr when nothing matches, which
// includes an empty string - there's nothing to match against a wildcard
// probe or a hidden-SSID beacon.
const SsidEntry* ssidLookup(const char* ssid);

uint16_t ssidTableSize();
