// SPDX-License-Identifier: GPL-3.0-only
#include "ui.h"
#include "config.h"
#include "alert.h"
#include "oui_table.h"
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
    u8g2.drawStr(0, 26, line);
    snprintf(line, sizeof(line), "BLE scan: %s", BLE_SCAN_PASSIVE ? "passive" : "ACTIVE");
    u8g2.drawStr(0, 36, line);
    snprintf(line, sizeof(line), "WiFi ch 1-%u", (unsigned)WIFI_MAX_CHANNEL);
    u8g2.drawStr(0, 46, line);
    u8g2.drawStr(0, 60, "Starting...");
    u8g2.sendBuffer();
}

void uiRender(const DetectorStatus& st, bool wifiPhase) {
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

    // Radio indicator sits on the header bar, so it has to be drawn in the
    // inverted colour while alerting or it would be white-on-white.
    u8g2.setFont(u8g2_font_5x8_tr);
    if (wifiPhase) snprintf(line, sizeof(line), "WiFi ch%02u", (unsigned)detectorChannel());
    else           snprintf(line, sizeof(line), "BLE");
    u8g2.drawStr(88, 10, line);
    u8g2.setDrawColor(1);

    if (st.best != nullptr) {
        // --- vendor / category ---
        snprintf(line, sizeof(line), "%s", st.best->vendor);
        u8g2.setFont(u8g2_font_6x10_tr);
        u8g2.drawStr(0, 26, line);

        u8g2.setFont(u8g2_font_5x8_tr);
        snprintf(line, sizeof(line), "%s  %s",
                 categoryName(st.best->category),
                 st.best->source == SRC_WIFI ? "WiFi" : "BLE");
        u8g2.drawStr(0, 36, line);

        // --- signal strength bar ---
        int16_t r = st.bestRssi;
        if (r < RSSI_WEAK)   r = RSSI_WEAK;
        if (r > RSSI_STRONG) r = RSSI_STRONG;
        int barW = (int)(((int32_t)r - RSSI_WEAK) * 100 /
                         ((int32_t)RSSI_STRONG - RSSI_WEAK));
        u8g2.drawFrame(0, 40, 102, 9);
        if (barW > 0) u8g2.drawBox(1, 41, barW, 7);

        snprintf(line, sizeof(line), "%ddB", (int)st.bestRssi);
        u8g2.drawStr(104, 48, line);

        // --- confidence + count ---
        // "LE" marks a vendor that sells essentially only to law enforcement;
        // "gen" marks a broad-market vendor that also ships civilian gear.
        snprintf(line, sizeof(line), "%s %s  %u dev",
                 st.bestConfidence == CONF_HIGH ? "HIGH" : "MED ",
                 st.best->specificity == SPEC_LE_ONLY ? "LE " : "gen",
                 (unsigned)st.activeCount);
        u8g2.drawStr(0, 60, line);
    } else {
        u8g2.setFont(u8g2_font_5x8_tr);
        u8g2.drawStr(0, 30, "No matched gear");
        snprintf(line, sizeof(line), "Frames seen: %u", (unsigned)detectorSeenTotal());
        u8g2.drawStr(0, 42, line);
        snprintf(line, sizeof(line), "Watchlist: %u OUI", (unsigned)ouiTableSize());
        u8g2.drawStr(0, 54, line);
    }

    u8g2.sendBuffer();
}
