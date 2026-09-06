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
// *** THIS TABLE NOW SHIPS WITH REAL FIELD-VERIFIED ENTRIES (Pennsylvania
// *** State Police, added 2026-09-06 - see the PSP block below for full
// *** provenance) ALONGSIDE ONE LIVE TEST/DEMO ROW (LEDET-TEST) THAT IS NOT
// *** A REAL VENDOR SIGNATURE. Each row's own comment says which is which.
static const SsidEntry SSID_TABLE[] = {
    // -----------------------------------------------------------------
    // PENNSYLVANIA STATE POLICE (PSP) - added 2026-09-06 from WiGLE survey
    // data cross-checked against known PSP barracks coordinates.
    //
    // Three independent barracks, three different troops, 282 km apart:
    //   Hamburg  (Troop L)        40.5641, -76.0025
    //   Dunmore  (Scranton area)  41.4357, -75.6151
    //   Bedford  (Troop G)        40.0063, -78.3855
    // All observations were 48-132 m from the barracks building, all on
    // Cisco hardware. Cross-site BSSID correlation (see per-row notes)
    // confirms a single statewide deployment, not coincidental naming.
    //
    // KNOWN LIMITATION: these are fixed APs at barracks buildings, not
    // vehicle-mounted. On the road, a hit here means a cruiser's in-car
    // client is directedly probing for one of these SSIDs while out of
    // range of the real AP - whether PSP in-car systems actually do that is
    // UNTESTED in the field. Until verified, treat a hit on this block as a
    // barracks-proximity indicator, not confirmed mobile detection.
    //
    // All four rows are PREFIX, deliberately - do not change any to
    // CONTAINS. A CONTAINS "PSP" rule was tested against the same survey
    // data and matched PSPPetCenter, PSPRETAIL, PSP Neighbor (a residence),
    // and BPSPictureU, all within 730 m of the Hamburg barracks. The full
    // prefix string, not the shared "PSP" fragment, is what makes these
    // safe to alert on.

    // Confirmed at all three sites above. Highest-value row: MVR is Mobile
    // Video Recorder, PSP's in-car video system, so cruisers are configured
    // for it by definition. Two Cisco OUIs seen across sites - 1C:FC:17 and
    // 78:F1:C6 - both appearing at multiple barracks (two procurement
    // batches mixed statewide, not two separate networks). Last observed
    // 2026-08-25 and 2026-09-05 - current as of this writing.
    { "PSP-MVR",  SSID_MATCH_PREFIX, "PSP in-car video",   CAT_BODYCAM, nullptr },

    // Confirmed at all three sites above; tightest cross-site correlation
    // of the four rows. Dunmore: 20:37:06:6D:69:7D / 20:37:06:A4:2C:4D.
    // Bedford: 20:37:06:6D:72:0D / 20:37:06:A4:38:4D. Same two 20:37:06
    // sub-ranges at both sites, both registered 2013.
    { "PSP-TEST", SSID_MATCH_PREFIX, "PSP facility",       CAT_OTHER,   nullptr },

    // Single-site only (Hamburg) - lower confidence than PSP-MVR/PSP-TEST
    // above, which were each confirmed at three independent barracks. Not
    // yet cross-checked against a second site.
    { "PSPWLAN",  SSID_MATCH_PREFIX, "PSP facility",       CAT_OTHER,   nullptr },

    // Single-site only (Hamburg), same caveat as PSPWLAN above - not yet
    // cross-checked against a second site. Likely a Cisco Unified
    // Communications (voice/VoIP) SSID, going by the name.
    { "PSP_UC",   SSID_MATCH_PREFIX, "PSP facility voice", CAT_OTHER,   nullptr },
    // -----------------------------------------------------------------

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
