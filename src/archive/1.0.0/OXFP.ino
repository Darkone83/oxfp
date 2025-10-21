#include <Arduino.h>
#include "led_stat.h"
#include "wifimgr.h"
#include "oxfp_orig.h"
#include "OXFP_config.h"
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

    OXFP_config::applyConfig(); // Main config logic (handles preview, animation, etc)
    OXFP_orig::loop();          // (Handles Xbox input fallback if in stock mode)

    delay(10);
}
