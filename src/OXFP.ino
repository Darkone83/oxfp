#include <Arduino.h>
#include "led_stat.h"
#include "wifimgr.h"
#include "oxfp_orig.h"
#include "oxfp_config.h"
#include <ESPmDNS.h>

void setup() {
    Serial.begin(115200);
    delay(1000); // Give time for serial monitor

    LedStat::begin();
    WiFiMgr::begin();
    OXFP_orig::begin();
    OXFP_config::begin(WiFiMgr::getServer());

    // mDNS will be started in loop() after WiFi connects
}

void loop() {
    LedStat::loop();
    WiFiMgr::loop();

    static bool mdnsStarted = false;
    if (WiFiMgr::isConnected() && !mdnsStarted) {
        if (MDNS.begin("oxfp")) {
            Serial.println("[mDNS] Started: http://oxfp.local/");
            mdnsStarted = true;
        } else {
            Serial.println("[mDNS] mDNS start failed");
        }
    }

    // --- LED Logic Selection ---
    if (OXFP_config::isActive()) {
        OXFP_config::loop();  // Web config/animation/static mode is active: override!
    } else {
        OXFP_orig::loop();    // Stock Xbox behavior
    }

    delay(10); // Avoid starving CPU
}
