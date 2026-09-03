// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <stdint.h>

// Broad equipment class, shown on the display.
enum GearCategory : uint8_t {
    CAT_UNKNOWN = 0,
    CAT_BODYCAM,     // body-worn / in-car video
    CAT_RADIO,       // LMR portable & mobile radios
    CAT_VEHICLE,     // in-car router / modem / MDT
    CAT_LIGHTBAR,    // lightbars, sirens
    CAT_RADAR,       // traffic radar / lidar
    CAT_ALPR,        // licence plate readers / traffic enforcement cams
    CAT_OTHER
};

// How exclusively this vendor sells to law enforcement.
//
// SPEC_LE_ONLY - effectively sells nowhere else; a hit means something on its own.
// SPEC_BROAD   - real LE supplier, but the same prefix appears on huge volumes of
//                civilian gear (warehouse scanners, IoT modems, utility radios).
//
// Curation metadata only - there is no runtime confidence tier anymore (every
// watchlist match alerts the same way). This column exists to help a maintainer
// judge which SPEC_BROAD vendors are worth an EXCLUDE_VENDOR_* toggle (see
// oui_table.cpp) if they turn out to be a false-positive source, the way
// CradlePoint did.
enum Specificity : uint8_t {
    SPEC_BROAD = 0,
    SPEC_LE_ONLY
};

struct OuiEntry {
    // Bytes beyond the assignment length are zero and ignored.
    //   bits == 24 -> compare prefix[0..2]            (MA-L)
    //   bits == 28 -> ... plus high nibble of byte 3  (MA-M)
    //   bits == 36 -> ... plus byte 3, plus high nibble of byte 4 (MA-S)
    uint8_t      prefix[5];
    uint8_t      bits;        // 24, 28 or 36
    const char*  vendor;
    GearCategory category;
    Specificity  specificity;
};

// Returns nullptr when the MAC's prefix is not on the watchlist.
const OuiEntry* ouiLookup(const uint8_t mac[6]);

uint16_t ouiTableSize();
