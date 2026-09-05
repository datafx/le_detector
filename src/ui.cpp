// SPDX-License-Identifier: GPL-3.0-only
#include "ui.h"
#include "config.h"
#include "alert.h"
#include "oui_table.h"
#include "ssid_table.h"
#include <U8g2lib.h>
#include <Wire.h>

static U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

void uiInit() {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    u8g2.begin();
    u8g2.setFont(u8g2_font_6x10_tr);
}

void uiBootScreen() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(0, 10, "LE GEAR DETECTOR");
    u8g2.drawHLine(0, 13, 128);

    char line[32];
    u8g2.setFont(u8g2_font_5x8_tr);
    snprintf(line, sizeof(line), "OUI entries: %u", (unsigned)ouiTableSize());
    u8g2.drawStr(0, 23, line);
    snprintf(line, sizeof(line), "SSID entries: %u", (unsigned)ssidTableSize());
    u8g2.drawStr(0, 32, line);
    snprintf(line, sizeof(line), "BLE scan: %s", BLE_SCAN_PASSIVE ? "passive" : "ACTIVE");
    u8g2.drawStr(0, 41, line);
    snprintf(line, sizeof(line), "WiFi ch 1-%u", (unsigned)WIFI_MAX_CHANNEL);
    u8g2.drawStr(0, 50, line);
    u8g2.drawStr(0, 60, "Starting...");
    u8g2.sendBuffer();
}

void uiRender(const DetectorStatus& st, const char* radioLabel) {
    u8g2.clearBuffer();
    char line[32];

    // --- header: state + which radio is currently listening ---
    bool alerting = (alertState() == ALERT_ACTIVE);
    u8g2.setFont(u8g2_font_6x10_tr);
    if (alerting) {
        // invert the header bar so an alert is unmistakable at a glance
        u8g2.drawBox(0, 0, 128, 13);
        u8g2.setDrawColor(0);
        u8g2.drawStr(2, 10, "** ALERT **");
    } else {
        u8g2.drawStr(0, 10, "ALL CLEAR");
        u8g2.drawHLine(0, 13, 128);
    }

    // Radio/mode indicator sits on the header bar, so it has to be drawn in
    // the inverted colour while alerting or it would be white-on-white.
    // Left-aligned at a fixed x (clear of "** ALERT **", which ends at ~68)
    // rather than right-aligned - AUTO mode's label swaps its trailing word
    // ("AUTO: WiFi" <-> "AUTO: BLE") every phase, and right-aligning made
    // the whole string visibly jump left/right each time. Left-aligned, the
    // shared "AUTO: " prefix stays planted and only the tail width changes.
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(70, 10, radioLabel);
    u8g2.setDrawColor(1);

    if (st.best != nullptr) {
        // --- vendor name, as large as fits: try biggest font first, drop
        // down a tier only if the name is too wide for the display. Short
        // names (e.g. "AXON") end up much larger than long ones (e.g.
        // "Sierra Wireless") instead of everything sharing one small size.
        static const uint8_t* const kVendorFonts[] = {
            u8g2_font_helvB24_tr,
            u8g2_font_helvB18_tr,
            u8g2_font_helvB14_tr,
            u8g2_font_helvB10_tr,
            u8g2_font_6x10_tr,   // guaranteed-fit fallback
        };
        const int kVendorFontCount = sizeof(kVendorFonts) / sizeof(kVendorFonts[0]);
        const int kVendorMaxWidthPx = 126;

        int fontIdx = kVendorFontCount - 1;
        for (int i = 0; i < kVendorFontCount; i++) {
            u8g2.setFont(kVendorFonts[i]);
            if (u8g2.getStrWidth(st.best->vendor) <= kVendorMaxWidthPx) {
                fontIdx = i;
                break;
            }
        }
        u8g2.setFont(kVendorFonts[fontIdx]);

        // Vertically center in the band between the header and the secondary
        // line below (shrunk from the full 36px down to 26px to make room for
        // that line - the RSSI bar's own position is unaffected).
        const int bandTop = 14, bandH = 26;
        int textH = u8g2.getAscent() - u8g2.getDescent();
        int y = bandTop + (bandH - textH) / 2 + u8g2.getAscent();
        u8g2.drawStr(0, y, st.best->vendor);

        // --- secondary line: what actually matched ---
        // A matched SSID is more specific than the vendor name alone (e.g.
        // "IBR900-1A2B3C" says which router, not just "some CradlePoint
        // device") so it takes priority over the source tag whenever
        // present. Probe vs. beacon/probe-response is called out explicitly
        // because they're not equally trustworthy: a beacon means that AP is
        // transmitting right now, a probe means some client is leaking a
        // network it joined before - not proof that network is here. OUI
        // hits with no SSID fall back to a plain source tag so a WiFi OUI
        // hit still reads differently from a BLE one.
        u8g2.setFont(u8g2_font_5x8_tr);
        if (st.best->ssid[0] != '\0') {
            const char* how = (st.best->source == SRC_PROBE) ? "probe" : "beacon";
            snprintf(line, sizeof(line), "SSID %s: %s", how, st.best->ssid);
        } else {
            snprintf(line, sizeof(line), "via %s",
                    (st.best->source == SRC_BLE) ? "BLE" : "WiFi OUI");
        }
        u8g2.drawStr(0, 48, line);

        // --- signal strength bar ---
        int16_t r = st.bestRssi;
        if (r < RSSI_WEAK)   r = RSSI_WEAK;
        if (r > RSSI_STRONG) r = RSSI_STRONG;
        int barW = (int)(((int32_t)r - RSSI_WEAK) * 100 /
                         ((int32_t)RSSI_STRONG - RSSI_WEAK));
        u8g2.drawFrame(0, 50, 102, 9);
        if (barW > 0) u8g2.drawBox(1, 51, barW, 7);

        u8g2.setFont(u8g2_font_5x8_tr);
        snprintf(line, sizeof(line), "%ddB", (int)st.bestRssi);
        u8g2.drawStr(104, 58, line);
    } else {
        u8g2.setFont(u8g2_font_5x8_tr);
        u8g2.drawStr(0, 30, "No matched gear");
        snprintf(line, sizeof(line), "Watchlist: %u OUI", (unsigned)ouiTableSize());
        u8g2.drawStr(0, 42, line);
        snprintf(line, sizeof(line), "%u SSID pattern%s", (unsigned)ssidTableSize(),
                ssidTableSize() == 1 ? "" : "s");
        u8g2.drawStr(0, 53, line);
    }

    u8g2.sendBuffer();
}
