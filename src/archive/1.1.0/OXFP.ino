#include <Arduino.h>
#include "led_stat.h"
#include "wifimgr.h"
#include "OXFP_orig.h"        // fix case to match header
#include "OXFP_config.h"
#include "OXFP_udp.h"         // <- UDP control module
#include <ESPmDNS.h>

void setup() {
    Serial.begin(115200);
    delay(1000);

    LedStat::begin();
    WiFiMgr::begin();
    OXFP_orig::begin();

    // Attach the config UI to the main server from WiFiMgr
    OXFP_config::begin(WiFiMgr::getServer());
    OXFP_config::loadPreferences();
    OXFP_config::applyConfig(); // Initial apply

    // Prepare UDP (disabled until Wi-Fi is up)
    OXFP_udp::begin(32123, /*enable=*/false);
}

void loop() {
    LedStat::loop();
    WiFiMgr::loop();

    // Bring up mDNS and UDP once Wi-Fi is connected
    static bool mdnsStarted = false;
    static bool udpEnabled = false;
    const bool connected = WiFiMgr::isConnected();

    if (connected && !mdnsStarted) {
        if (MDNS.begin("oxfp")) {
            Serial.println("[mDNS] Started: http://oxfp.local/");
            // Advertise a UDP service for discovery by tools/scripts
            MDNS.addService("oxfp", "udp", OXFP_udp::port());
            MDNS.addServiceTxt("oxfp", "udp", "ver", "udp-1.0");
            MDNS.addServiceTxt("oxfp", "udp", "caps", "json");
            mdnsStarted = true;
        } else {
            Serial.println("[mDNS] mDNS start failed");
        }
    }

    // Auto-enable/disable UDP listener with network state
    if (connected && !udpEnabled) {
        OXFP_udp::setEnabled(true);
        udpEnabled = true;
        Serial.printf("[UDP] Enabled on %u\n", OXFP_udp::port());
    } else if (!connected && udpEnabled) {
        OXFP_udp::setEnabled(false);
        udpEnabled = false;
        Serial.println("[UDP] Disabled (no Wi-Fi)");
    }

    // Handle incoming UDP control packets (non-blocking)
    OXFP_udp::loop();

    // Main LED logic
    OXFP_config::applyConfig(); // Handles preview, animation, static/error, etc.
    OXFP_orig::loop();          // Stock behavior / input sampling

    delay(10);
}
