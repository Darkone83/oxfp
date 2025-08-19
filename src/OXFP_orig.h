#pragma once

#include <Adafruit_NeoPixel.h>

extern Adafruit_NeoPixel leds;

namespace OXFP_orig {
    void begin();
    void loop();

    // Replace custom rendering (nullptr -> stock mirroring)
    void ledCustomOverride(void (*handler)(void));

    // When true, stock mirroring preempts custom handler during error (e.g., FRAG)
    void setPreemptOnError(bool enable);

    // Query helpers (HIGH = Xbox asserts)
    void readInputLines(bool& leftGreen, bool& leftRed, bool& rightGreen, bool& rightRed);
    bool consoleIsOff();     // all lines LOW for debounce window
    bool errorActive();      // any RED or AMBER on either side

    // Call this instead of leds.show(); mirrors LED0 to GPIO4, LED1 to GPIO3 (1 pixel each)
    void showMirrored();
}
