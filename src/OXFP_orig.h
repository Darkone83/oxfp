#pragma once

#include <Adafruit_NeoPixel.h>

extern Adafruit_NeoPixel leds;

namespace OXFP_orig {
    void begin();
    void loop();
    void ledCustomOverride(void (*handler)(void)); // Renamed here
}
