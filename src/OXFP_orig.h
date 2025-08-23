#pragma once

#include <Adafruit_NeoPixel.h>

extern Adafruit_NeoPixel leds;

namespace OXFP_orig {
    void begin();
    void loop();

    // Replace custom rendering (nullptr -> stock behavior)
    void ledCustomOverride(void (*handler)(void));

    // When true, stock mirroring preempts custom handler during error (e.g., FRAG)
    void setPreemptOnError(bool enable);

    // Runtime toggle: copy LEFT → RIGHT when no custom handler is active (i.e., Stock; you may also
    // enable it for Static in your config). Ignored during Animation so both channels stay independent.
    void setCopyLeftToRight(bool enable);

    // Query helpers (HIGH = Xbox asserts)
    void readInputLines(bool& leftGreen, bool& leftRed, bool& rightGreen, bool& rightRed);
    bool consoleIsOff();     // all lines LOW for debounce window
    bool errorActive();      // any RED or AMBER on either side

    // Call this instead of leds.show(); shows internal ring and fills entire external strips:
    // LEFT pixel color -> all pixels on left strip, RIGHT pixel color -> all pixels on right strip.
    void showMirrored();
}
