#pragma once
#include <stdint.h>
#include <Adafruit_NeoPixel.h>

// Expose the internal 2-pixel ring instance so other TUs can use leds.Color(), numPixels(), etc.
extern Adafruit_NeoPixel leds;

namespace OXFP_orig {
  void begin();
  void loop();
  void showMirrored();

  void ledCustomOverride(void (*handler)(void));
  void setPreemptOnError(bool enable);
  void setCopyLeftToRight(bool enable);

  // NEW: preempt animations on any non-idle OG state (blink/frag) and global brightness control
  void setPreemptOnNonIdle(bool enable);
  void setGlobalBrightness(uint8_t b);

  // Query helpers
  void readInputLines(bool& leftGreen, bool& leftRed, bool& rightGreen, bool& rightRed);
  bool consoleIsOff();
  bool errorActive();
}
