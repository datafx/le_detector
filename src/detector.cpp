/*
    LE Gear Detector - scanning core

    BLE scan setup / GAP callback structure and the WiFi promiscuous handler are
    adapted from nyanBOX (axon_detector.cpp, device_scout.cpp)
    https://github.com/jbohack/nyanBOX
    Copyright (c) 2025 jbohack - MIT License
    SPDX-License-Identifier: MIT

    Changes from the original:
      - OUI match generalised from a single hard-coded prefix to a table
      - BLE scan defaults to PASSIVE (original is ACTIVE, which transmits)
      - WiFi handler no longer discards frames with an empty SSID
      - BLE random/resolvable addresses are rejected (their OUI bits are random)
      - std::vector device list replaced with a fixed table + LRU eviction
      - WiFi handler also parses the SSID IE from beacon/probe request/probe
        response frames and matches it against a separate watchlist,
        independent of OUI - not present in the original at all
*/

#include "detector.h"
#include "config.h"
#include "ssid_table.h"

#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"

static TrackedDevice s_devices[MAX_TRACKED];
static uint8_t       s_channel = 1;
static uint32_t      s_lastHop = 0;
static uint16_t      s_seenTotal = 0;
static uint32_t      s_lastHitMs = 0;

static bool s_wifiUp = false;
static bool s_bleUp  = false;

// ---------------------------------------------------------------------------
// Device table
// ---------------------------------------------------------------------------

// vendor/category come straight from either an OuiEntry or an SsidEntry -
// taking them by value instead of an OuiEntry* lets both matchers share this
// one recordMatch path. `ssid` is optional (nullptr/omitted for OUI and BLE
// matches, which have none) and is copied, not stored by pointer, since it
// points at a stack buffer that doesn't outlive the calling frame.
static void recordMatch(const uint8_t mac[6], int8_t rssi,
                        const char* vendor, GearCategory category,
                        Source src, const char* ssid = nullptr) {
    uint32_t now = millis();
    s_lastHitMs = now;

    // Existing entry?
    for (uint8_t i = 0; i < MAX_TRACKED; i++) {
        if (!s_devices[i].used) continue;
        if (memcmp(s_devices[i].mac, mac, 6) != 0) continue;

        TrackedDevice& d = s_devices[i];
        // EMA so a single weak/strong frame doesn't swing the flash rate
        d.rssi = (int16_t)((rssi * RSSI_EMA_NUM +
                            d.rssi * (RSSI_EMA_DEN - RSSI_EMA_NUM)) / RSSI_EMA_DEN);
        d.lastSeen = now;
        return;
    }

    // New entry - take a free slot, else evict the least recently seen.
    uint8_t slot = 0xFF;
    for (uint8_t i = 0; i < MAX_TRACKED; i++) {
        if (!s_devices[i].used) { slot = i; break; }
    }
    if (slot == 0xFF) {
        uint32_t oldest = 0xFFFFFFFF;
        for (uint8_t i = 0; i < MAX_TRACKED; i++) {
            if (s_devices[i].lastSeen < oldest) { oldest = s_devices[i].lastSeen; slot = i; }
        }
    }

    TrackedDevice& d = s_devices[slot];
    memcpy(d.mac, mac, 6);
    d.rssi      = rssi;
    d.firstSeen = now;
    d.lastSeen  = now;
    d.vendor    = vendor;
    d.category  = category;
    d.source    = src;
    if (ssid) {
        strncpy(d.ssid, ssid, sizeof(d.ssid) - 1);
        d.ssid[sizeof(d.ssid) - 1] = '\0';
    } else {
        d.ssid[0] = '\0';
    }
    d.used      = true;
}

void detectorExpire() {
    uint32_t now = millis();
    for (uint8_t i = 0; i < MAX_TRACKED; i++) {
        if (!s_devices[i].used) continue;
        if (now - s_devices[i].lastSeen > DEVICE_TTL_MS) {
            s_devices[i].used = false;
        }
    }
}

DetectorStatus detectorStatus() {
    DetectorStatus st = {};
    st.bestRssi  = -127;
    st.best      = nullptr;
    st.lastHitMs = s_lastHitMs;

    for (uint8_t i = 0; i < MAX_TRACKED; i++) {
        if (!s_devices[i].used) continue;
        st.activeCount++;
        if (s_devices[i].rssi > st.bestRssi) {
            st.bestRssi = s_devices[i].rssi;
            st.best     = &s_devices[i];
        }
    }
    return st;
}

uint8_t  detectorChannel()   { return s_channel; }
uint16_t detectorSeenTotal() { return s_seenTotal; }

// ---------------------------------------------------------------------------
// WiFi promiscuous sniffing
// ---------------------------------------------------------------------------

// Pulls the SSID information element out of a beacon / probe request / probe
// response frame. Per 802.11, SSID (element ID 0x00) is always the first IE
// following the frame's fixed fields, so this is a direct offset check, not
// a general IE walk - probe requests have no fixed fields between the
// 24-byte MAC header and the IEs, beacons/probe responses have 12
// (8-byte timestamp + 2-byte interval + 2-byte capability info).
//
// Every length is checked against `len` before being read - the length byte
// inside the frame is attacker/interference-controlled and is never trusted
// on its own. Returns false (no bytes emitted) for a missing/malformed IE, a
// zero-length SSID (wildcard probe or hidden-network beacon - nothing to
// match either way), or an SSID longer than the 32-byte spec maximum.
static bool extractSsid(const uint8_t* frame, int len, uint8_t subtype,
                        const uint8_t** ssidBytes, uint8_t* ssidLen) {
    int ieOffset = (subtype == 0x40) ? 24 : 36;
    if (len < ieOffset + 2) return false;
    if (frame[ieOffset] != 0x00) return false;        // not the SSID IE
    uint8_t l = frame[ieOffset + 1];
    if (l == 0 || l > 32) return false;
    if (len < ieOffset + 2 + (int)l) return false;    // length byte not trusted

    *ssidBytes = &frame[ieOffset + 2];
    *ssidLen   = l;
    return true;
}

static void IRAM_ATTR wifiSnifferCb(void* buff, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_MGMT) return;

    const wifi_promiscuous_pkt_t* ppkt = (wifi_promiscuous_pkt_t*)buff;
    const uint8_t* frame = ppkt->payload;
    int len = ppkt->rx_ctrl.sig_len;
    if (len < 24) return;   // shorter than a full mgmt header - nothing usable

    uint8_t subtype = frame[0] & 0xF0;
    // 0x80 beacon, 0x40 probe request, 0x50 probe response
    if (subtype != 0x80 && subtype != 0x40 && subtype != 0x50) return;

    // addr2 = transmitter address
    const uint8_t* mac = &frame[10];
    if (mac[0] & 0x01) return;   // multicast/broadcast, not a real transmitter

    s_seenTotal++;

    // OUI match - meaningless on a randomised address, so skip it there.
    if (!(mac[0] & 0x02)) {
        const OuiEntry* oui = ouiLookup(mac);
        if (oui) recordMatch(mac, ppkt->rx_ctrl.rssi, oui->vendor, oui->category, SRC_WIFI);
    }

    // SSID match - runs regardless of address randomisation. A directed
    // probe request leaks its target SSID in cleartext as an 802.11 protocol
    // requirement, not a configuration choice, so it survives the same MAC
    // rotation that defeats OUI matching.
    const uint8_t* ssidBytes;
    uint8_t ssidLen;
    if (extractSsid(frame, len, subtype, &ssidBytes, &ssidLen)) {
        char ssid[33];
        memcpy(ssid, ssidBytes, ssidLen);
        ssid[ssidLen] = '\0';

        const SsidEntry* hit = ssidLookup(ssid);
        if (hit) {
            // Probe requests are weaker evidence (a client leaking a
            // previously-joined network, not proof the AP is here now) -
            // beacons/probe responses mean the AP itself is transmitting.
            Source src = (subtype == 0x40) ? SRC_PROBE : SRC_WIFI;
            recordMatch(mac, ppkt->rx_ctrl.rssi, hit->vendor, hit->category, src, ssid);
        }
    }
}

void detectorHopChannel() {
    uint32_t now = millis();
    if (now - s_lastHop < CHANNEL_HOP_MS) return;
    s_channel = (s_channel >= WIFI_MAX_CHANNEL) ? 1 : (s_channel + 1);
    esp_wifi_set_channel(s_channel, WIFI_SECOND_CHAN_NONE);
    s_lastHop = now;
}

void detectorStartWifiPhase() {
    if (s_wifiUp) return;

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    esp_wifi_set_ps(WIFI_PS_NONE);

    wifi_promiscuous_filter_t flt = {};
    flt.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT;
    esp_wifi_set_promiscuous_filter(&flt);
    esp_wifi_set_promiscuous_rx_cb(&wifiSnifferCb);
    esp_wifi_set_promiscuous(true);

    s_channel = 1;
    esp_wifi_set_channel(s_channel, WIFI_SECOND_CHAN_NONE);
    s_lastHop = millis();
    s_wifiUp = true;
}

void detectorStopWifiPhase() {
    if (!s_wifiUp) return;
    esp_wifi_set_promiscuous(false);
    esp_wifi_stop();
    delay(50);
    esp_wifi_deinit();
    delay(50);
    s_wifiUp = false;
}

// ---------------------------------------------------------------------------
// BLE scanning
// ---------------------------------------------------------------------------

static esp_ble_scan_params_t s_bleScanParams = {
    .scan_type          = BLE_SCAN_TYPE_PASSIVE,   // overwritten in start()
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval      = 0x100,
    .scan_window        = 0xA0,
    .scan_duplicate     = BLE_SCAN_DUPLICATE_DISABLE
};

static void processBleResult(esp_ble_gap_cb_param_t* p) {
    // Only public addresses carry a real OUI. Random / resolvable-private
    // addresses have randomised vendor bits, so matching them is meaningless.
    if (p->scan_rst.ble_addr_type != BLE_ADDR_TYPE_PUBLIC) return;

    s_seenTotal++;

    const uint8_t* mac = p->scan_rst.bda;
    const OuiEntry* oui = ouiLookup(mac);
    if (oui) recordMatch(mac, p->scan_rst.rssi, oui->vendor, oui->category, SRC_BLE);
}

static void gapCb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param) {
    switch (event) {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
        if (param->scan_param_cmpl.status == ESP_BT_STATUS_SUCCESS) {
            esp_ble_gap_start_scanning(0);   // 0 = scan until told to stop
        }
        break;
    case ESP_GAP_BLE_SCAN_RESULT_EVT:
        if (param->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {
            processBleResult(param);
        }
        break;
    default:
        break;
    }
}

void detectorStartBlePhase() {
    if (s_bleUp) return;

    if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE) {
        esp_bt_mem_release(ESP_BT_MODE_CLASSIC_BT);   // BLE only, reclaim the RAM
    }
    if (!btStarted()) { btStart(); delay(50); }

    if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_UNINITIALIZED) {
        esp_bluedroid_init();
        delay(50);
    }
    if (esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED) {
        esp_bluedroid_enable();
        delay(50);
    }

    s_bleScanParams.scan_type = BLE_SCAN_PASSIVE ? BLE_SCAN_TYPE_PASSIVE
                                                 : BLE_SCAN_TYPE_ACTIVE;
    esp_ble_gap_register_callback(gapCb);
    esp_ble_gap_set_scan_params(&s_bleScanParams);   // start happens in the callback
    s_bleUp = true;
}

void detectorStopBlePhase() {
    if (!s_bleUp) return;
    esp_ble_gap_stop_scanning();
    delay(30);
    if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_ENABLED) {
        esp_bluedroid_disable();
        delay(30);
    }
    if (esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_UNINITIALIZED) {
        esp_bluedroid_deinit();
        delay(30);
    }
    if (btStarted()) { btStop(); delay(30); }
    s_bleUp = false;
}

void detectorInit() {
    memset(s_devices, 0, sizeof(s_devices));
    s_seenTotal = 0;
    s_lastHitMs = 0;
}
