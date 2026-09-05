// SPDX-License-Identifier: GPL-3.0-only
#include "ssid_table.h"
#include <string.h>   // strlen, for SSID_MATCH_PREFIX_AND_CONTAINS

// ---------------------------------------------------------------------------
// SSID WATCHLIST
//
// Independent signal from the OUI table: a client configured for a hidden
// network must send directed probe requests containing that SSID in
// cleartext (802.11 protocol requirement, not a config choice), and it
// survives MAC address randomisation. Also matched against beacon and probe
// response frames, where it's strictly better evidence than a bare OUI hit -
// e.g. a CradlePoint OUI means "some CradlePoint router," but an
// "IBR900-"/"IBR1700-" SSID means specifically which model, which is enough
// to tell a fleet router from a school bus hotspot on OUI alone.
//
// Populate this from field capture (WiGLE surveys, on-site SSID logging) -
// real rows should come from an actual observed network name, not a guess.
// No registry equivalent to the OUI table's IEEE lookup exists for SSIDs.
//
// Comment out any row to disable it - same as the OUI table, this is a plain
// linear scan with no sort order to preserve, so removing or adding a row
// can't break anything else in the table.
//
// Case-insensitive. Keep patterns as specific as the real observed SSID
// allows - a short/generic pattern (e.g. bare "MDT") risks matching consumer
// SSIDs the same way an overly broad OUI does.
//
// Three match modes (see SsidMatchMode in ssid_table.h): PREFIX, CONTAINS,
// and PREFIX_AND_CONTAINS - the last requires `pattern` at the start of the
// SSID AND `contains` somewhere in what's left after that prefix, the
// two-fragment-with-a-gap shape a glob would write as "IBR*-mobile". Still
// no literal wildcard character or regex engine - just a third fixed shape.
//
// *** THIS TABLE CURRENTLY SHIPS WITH ONE LIVE ROW THAT IS TEST/DEMO DATA,
// *** NOT A REAL VENDOR SIGNATURE - see the row itself below. Everything
// *** else in this file is the real, empty-by-default watchlist design.
static const SsidEntry SSID_TABLE[] = {
    // TEST/DEMO ONLY - NOT A REAL VENDOR SIGNATURE. Same purpose as bring-up
    // sequence step 5's "add the OUI of a device you own" for the OUI table:
    // spoof this by renaming a phone hotspot or a saved WiFi profile to
    // "LEDET-TEST" (or anything starting with it) to exercise the
    // display/alert path end to end without needing real LE gear nearby.
    // The vendor string below is deliberately what it is - it's exactly
    // what would render on the OLED if this fires, so "TEST/DEMO" needs to
    // be unmistakable there too, not just in this comment. Remove this row
    // before relying on this table for anything but a demo/bring-up.
    { "LEDET-TEST", SSID_MATCH_PREFIX, "TEST/DEMO SSID - not real gear", CAT_OTHER, nullptr },

    // Example rows showing the field layout - uncomment and edit with a
    // pattern actually seen in the field. The fifth field is only meaningful
    // for SSID_MATCH_PREFIX_AND_CONTAINS (see ssid_table.h) - pass nullptr
    // for the other two modes:
    // { "IBR900-",  SSID_MATCH_PREFIX,   "CradlePoint IBR900",  CAT_VEHICLE, nullptr },
    // { "IBR1700-", SSID_MATCH_PREFIX,   "CradlePoint IBR1700", CAT_VEHICLE, nullptr },
    // Two-fragment example: starts with "IBR", and "-mobile" appears
    // somewhere after that prefix - the shape a glob would write as
    // "IBR*-mobile":
    // { "IBR", SSID_MATCH_PREFIX_AND_CONTAINS, "CradlePoint IBR (mobile)", CAT_VEHICLE, "-mobile" },
};

static const uint16_t SSID_TABLE_SIZE =
    sizeof(SSID_TABLE) / sizeof(SSID_TABLE[0]);

static char lowerAscii(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
}

// True if `pattern` matches the bytes starting at `text` (text may continue
// past where pattern ends - this is also the prefix-match primitive).
static bool ciMatchAt(const char* text, const char* pattern) {
    while (*pattern) {
        if (*text == '\0') return false;
        if (lowerAscii(*text) != lowerAscii(*pattern)) return false;
        text++;
        pattern++;
    }
    return true;
}

static bool ciContains(const char* text, const char* pattern) {
    if (*pattern == '\0') return false;
    for (const char* p = text; *p; p++) {
        if (ciMatchAt(p, pattern)) return true;
    }
    return false;
}

const SsidEntry* ssidLookup(const char* ssid) {
    if (!ssid || ssid[0] == '\0') return nullptr;

    for (uint16_t i = 0; i < SSID_TABLE_SIZE; i++) {
        const SsidEntry& e = SSID_TABLE[i];
        bool hit;
        switch (e.mode) {
        case SSID_MATCH_PREFIX:
            hit = ciMatchAt(ssid, e.pattern);
            break;
        case SSID_MATCH_CONTAINS:
            hit = ciContains(ssid, e.pattern);
            break;
        case SSID_MATCH_PREFIX_AND_CONTAINS:
        default:
            // `contains` is searched only in what's left after `pattern` -
            // it can't be satisfied by bytes inside the prefix itself.
            hit = e.contains && ciMatchAt(ssid, e.pattern) &&
                  ciContains(ssid + strlen(e.pattern), e.contains);
            break;
        }
        if (hit) return &e;
    }
    return nullptr;
}

uint16_t ssidTableSize() { return SSID_TABLE_SIZE; }
