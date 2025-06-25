#pragma once

#include <Adafruit_NeoPixel.h> // Required for type

namespace OXFP_orig {
    void begin();
    void loop();
    void setCustomHandler(void (*handler)(void)); // For custom effect overrides
}

// Expose the global WS2812 object to other modules (such as OXFP_config)
extern Adafruit_NeoPixel leds;
