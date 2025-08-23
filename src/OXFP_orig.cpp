#include "OXFP_orig.h"
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

// ====== XBOX INPUT LINES (HIGH = ON). Avoid GPIO19/20 on ESP32-S3 if using native USB. ======
#define PIN_LGI     19  // Left  Green
#define PIN_RGI     15  // Right Green
#define PIN_LRI     18  // Left  Red
#define PIN_RRI     21  // Right Red 20 old pin

// ====== INTERNAL RING ======
#define WS2812_PIN      5
#define NUM_LEDS        2
#define PIXEL_LEFT      0
#define PIXEL_RIGHT     1

Adafruit_NeoPixel leds(NUM_LEDS, WS2812_PIN, NEO_GRB + NEO_KHZ800);

// ====== EXTERNAL MIRROR OUTPUTS (fixed to ONE pixel each) ======
#ifndef EXT_MIRROR_LEFT_PIN
#define EXT_MIRROR_LEFT_PIN   4    // mirrors LED 0 (left)
#endif
#ifndef EXT_MIRROR_RIGHT_PIN
#define EXT_MIRROR_RIGHT_PIN  3    // mirrors LED 1 (right)
#endif

static Adafruit_NeoPixel extLeft (1, EXT_MIRROR_LEFT_PIN,  NEO_GRB + NEO_KHZ800);
static Adafruit_NeoPixel extRight(1, EXT_MIRROR_RIGHT_PIN, NEO_GRB + NEO_KHZ800);

namespace {
  void (*customHandler)(void) = nullptr;
  bool preemptOnError = false;

  // --- Mirroring workaround flags (mutually exclusive) ---
  // Default ON: mirror RIGHT from LEFT to mask a flaky right input.
  bool mirrorRfromL = true;
  bool mirrorLfromR = false;

  uint32_t lastAnyHighMs = 0;
  const uint32_t offDebounceMs = 300;

  inline bool lineOn(uint8_t pin) { return digitalRead(pin) == HIGH; }

  inline uint32_t colOff()   { return leds.Color(0,0,0); }
  inline uint32_t colGreen() { return leds.Color(0,255,0); }
  inline uint32_t colRed()   { return leds.Color(255,0,0); }
  inline uint32_t colAmber() { return leds.Color(255,128,0); }

  inline void renderStock(bool LG, bool LR, bool RG, bool RR) {
    uint32_t left  = (LG&&LR) ? colAmber() : LG ? colGreen() : LR ? colRed() : colOff();
    uint32_t right = (RG&&RR) ? colAmber() : RG ? colGreen() : RR ? colRed() : colOff();
    leds.setPixelColor(PIXEL_LEFT,  left);
    leds.setPixelColor(PIXEL_RIGHT, right);
    // mirrored show is called by caller
  }

  inline void mirrorOnePixel(Adafruit_NeoPixel& s, uint32_t color) {
    s.setPixelColor(0, color);
    s.show();
  }
}

namespace OXFP_orig {

void begin() {
  pinMode(PIN_LGI, INPUT_PULLDOWN);
  pinMode(PIN_RGI, INPUT_PULLDOWN);
  pinMode(PIN_LRI, INPUT_PULLDOWN);
  pinMode(PIN_RRI, INPUT_PULLDOWN);

  leds.begin();
  leds.setBrightness(255);
  leds.clear();
  leds.show();

  extLeft.begin();  extLeft.clear();  extLeft.show();
  extRight.begin(); extRight.clear(); extRight.show();
}

void showMirrored() {
  // Read current buffer colors
  uint32_t left  = leds.getPixelColor(PIXEL_LEFT);
  uint32_t right = leds.getPixelColor(PIXEL_RIGHT);

  // Apply mirroring workaround before showing
  if (mirrorRfromL) {
    right = left;
  } else if (mirrorLfromR) {
    left = right;
  }

  // Write back and show internal
  leds.setPixelColor(PIXEL_LEFT,  left);
  leds.setPixelColor(PIXEL_RIGHT, right);
  leds.show();

  // Mirror to external single-pixel outputs on GPIO4 (LED0) and GPIO3 (LED1)
  mirrorOnePixel(extLeft,  left);
  mirrorOnePixel(extRight, right);
}

void loop() {
  // Always sample inputs so we can decide about preemption & off blanking
  const bool LG = lineOn(PIN_LGI);
  const bool LR = lineOn(PIN_LRI);
  const bool RG = lineOn(PIN_RGI);
  const bool RR = lineOn(PIN_RRI);
  const bool anyHigh = LG || LR || RG || RR;
  if (anyHigh) lastAnyHighMs = millis();

  // Console OFF: always blank
  if (!anyHigh && (millis() - lastAnyHighMs) > offDebounceMs) {
    leds.clear();
    showMirrored();
    return;
  }

  const bool isError = (LR || RR || (LG && LR) || (RG && RR));

  // Preempt custom during error (for animations)
  if (customHandler && preemptOnError && isError) {
    renderStock(LG, LR, RG, RR);
    showMirrored();
    return;
  }

  // Custom or stock
  if (customHandler) { customHandler(); return; }
  renderStock(LG, LR, RG, RR);
  showMirrored();
}

void ledCustomOverride(void (*handler)(void)) { customHandler = handler; }
void setPreemptOnError(bool enable) { preemptOnError = enable; }

// Workaround controls (mutually exclusive toggles)
void setMirrorRightFromLeft(bool enable) {
  mirrorRfromL = enable;
  if (enable) mirrorLfromR = false;
}
void setMirrorLeftFromRight(bool enable) {
  mirrorLfromR = enable;
  if (enable) mirrorRfromL = false;
}

// --- Query helpers ---
void readInputLines(bool& leftGreen, bool& leftRed, bool& rightGreen, bool& rightRed) {
  leftGreen  = lineOn(PIN_LGI);
  leftRed    = lineOn(PIN_LRI);
  rightGreen = lineOn(PIN_RGI);
  rightRed   = lineOn(PIN_RRI);
  if (leftGreen || leftRed || rightGreen || rightRed) lastAnyHighMs = millis();
}

bool consoleIsOff() {
  bool LG = lineOn(PIN_LGI), LR = lineOn(PIN_LRI), RG = lineOn(PIN_RGI), RR = lineOn(PIN_RRI);
  if (LG || LR || RG || RR) { lastAnyHighMs = millis(); return false; }
  return (millis() - lastAnyHighMs) > offDebounceMs;
}

bool errorActive() {
  bool LG = lineOn(PIN_LGI), LR = lineOn(PIN_LRI), RG = lineOn(PIN_RGI), RR = lineOn(PIN_RRI);
  return (LR || RR || (LG && LR) || (RG && RR));
}

} // namespace OXFP_orig
