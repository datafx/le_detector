// SPDX-License-Identifier: GPL-3.0-only
//
// Staging table for OUIs you're testing locally before submitting them
// upstream as a PR into oui_table.cpp. Same OuiEntry struct/fields as the
// main table, so promoting a row you're happy with is just cutting it out
// of here and pasting it into OUI_TABLE in oui_table.cpp at the correct
// sorted position (see that file's "MUST STAY SORTED" note), then adding a
// matching case to test/test_oui.cpp.
//
// Unlike the main table, this one does NOT need to be kept sorted - it's
// linear-scanned as a fallback only after the main table's binary search
// comes up empty, so add entries in whatever order is convenient.
//
// Confidence note: an unvetted self-added entry is capped at MEDIUM
// (SPEC_BROAD) unless you explicitly set SPEC_LE_ONLY - same reasoning as
// the main table's decision 7 (see CLAUDE.md): repetition alone shouldn't
// earn HIGH confidence for something nobody's reviewed yet.

#include "oui_table.h"

static const OuiEntry USER_OUI_TABLE[] = {
    // { { 0x00,0x11,0x22,0x00,0x00 }, 24, "Example Vendor", CAT_OTHER, SPEC_BROAD },
};

static const uint16_t USER_OUI_COUNT = sizeof(USER_OUI_TABLE) / sizeof(USER_OUI_TABLE[0]);

static bool userMatches(const OuiEntry& e, const uint8_t mac[6]) {
    for (uint8_t i = 0; i < 3; i++) {
        if (mac[i] != e.prefix[i]) return false;
    }
    if (e.bits == 24) return true;
    if (e.bits == 28) return (mac[3] & 0xF0) == (e.prefix[3] & 0xF0);
    if (e.bits == 36) return mac[3] == e.prefix[3] &&
                             (mac[4] & 0xF0) == (e.prefix[4] & 0xF0);
    return false;
}

const OuiEntry* ouiUserLookup(const uint8_t mac[6]) {
    for (uint16_t i = 0; i < USER_OUI_COUNT; i++) {
        if (userMatches(USER_OUI_TABLE[i], mac)) return &USER_OUI_TABLE[i];
    }
    return nullptr;
}
