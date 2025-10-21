#include "OXFP_orig.h"
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

// ==========================
// Optional self-test (compile-time)
// ==========================
// #define OXFP_SELFTEST 1
#ifndef OXFP_SELFTEST
#define OXFP_SELFTEST 0
#endif

// ====== XBOX INPUT LINES (HIGH = ON) ======
#define PIN_LGI     19  // Left  Green
#define PIN_RGI     15  // Right Green
#define PIN_LRI     18  // Left  Red
#define PIN_RRI     21  // Right Red

// ====== INTERNAL RING ======
#define WS2812_PIN      5
#define NUM_LEDS        2
#define PIXEL_LEFT      0
#define PIXEL_RIGHT     1

Adafruit_NeoPixel leds(NUM_LEDS, WS2812_PIN, NEO_GRB + NEO_KHZ800);

// ====== EXTERNAL MIRROR OUTPUTS (mirrors LED0 to GPIO4, LED1 to GPIO3) ======
#ifndef EXT_MIRROR_LEFT_PIN
#define EXT_MIRROR_LEFT_PIN   4
#endif
#ifndef EXT_MIRROR_RIGHT_PIN
#define EXT_MIRROR_RIGHT_PIN  3
#endif

#ifndef EXT_MIRROR_LEFT_COUNT
#define EXT_MIRROR_LEFT_COUNT   30
#endif
#ifndef EXT_MIRROR_RIGHT_COUNT
#define EXT_MIRROR_RIGHT_COUNT  30
#endif

static Adafruit_NeoPixel extLeft (EXT_MIRROR_LEFT_COUNT,  EXT_MIRROR_LEFT_PIN,  NEO_GRB + NEO_KHZ800);
static Adafruit_NeoPixel extRight(EXT_MIRROR_RIGHT_COUNT, EXT_MIRROR_RIGHT_PIN, NEO_GRB + NEO_KHZ800);

namespace {
  void (*customHandler)(void) = nullptr;
  bool preemptOnError = false;
  bool preemptOnNonIdle = false;     // NEW: preempt when OG is not steady idle-green
  bool copyLeftToRight = false;      // runtime mirror for stock (no custom active)

  bool preemptActive = false;        // NEW: set true for the current frame when stock preempts custom

  uint32_t lastAnyHighMs = 0;
  const uint32_t offDebounceMs = 300;

  // Global brightness (caps everything, including stock + mirrors)
  uint8_t gBrightness = 255;

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
  }

  inline void mirrorFillStrip(Adafruit_NeoPixel& s, uint32_t color) {
    const uint16_t n = s.numPixels();
    for (uint16_t i = 0; i < n; ++i) s.setPixelColor(i, color);
    s.show();
  }

  inline bool isIdleGreen(bool LG, bool LR, bool RG, bool RR) {
    // Idle == steady green both sides (G=1, R=0 each side)
    return (LG && RG && !LR && !RR);
  }

#if OXFP_SELFTEST
  // Simple state machine to prove both channels are driven
  uint8_t st_state = 0;
  uint32_t st_last = 0;
  const uint32_t ST_STEP_MS = 800; // duration per step
  void runSelfTest() {
    const uint32_t now = millis();
    if (now - st_last > ST_STEP_MS) {
      st_last = now;
      st_state = (st_state + 1) % 4; // 0:clear, 1:left, 2:right, 3:both
    }
    uint32_t leftC  = 0;
    uint32_t rightC = 0;
    switch (st_state) {
      case 0: leftC = 0; rightC = 0; break;                     // both off
      case 1: leftC = leds.Color(0,255,0); rightC = 0; break;   // left green
      case 2: leftC = 0; rightC = leds.Color(255,0,255); break; // right purple
      case 3: leftC = leds.Color(0,255,0); rightC = leds.Color(255,0,255); break; // both
    }
    leds.setPixelColor(PIXEL_LEFT,  leftC);
    leds.setPixelColor(PIXEL_RIGHT, rightC);
  }
#endif
}

namespace OXFP_orig {

void begin() {
  pinMode(PIN_LGI, INPUT_PULLDOWN);
  pinMode(PIN_RGI, INPUT_PULLDOWN);
  pinMode(PIN_LRI, INPUT_PULLDOWN);
  pinMode(PIN_RRI, INPUT_PULLDOWN);

  leds.begin();
  leds.setBrightness(gBrightness);
  leds.clear();
  leds.show();

  extLeft.begin();
  extLeft.setBrightness(gBrightness);
  extLeft.clear();
  extLeft.show();

  extRight.begin();
  extRight.setBrightness(gBrightness);
  extRight.clear();
  extRight.show();
}

void showMirrored() {
  // Show the internal 2-pixel ring first (so getPixelColor reads latest buffer)
  leds.show();

  // Read the two independent pixel colors
  uint32_t leftCol  = leds.getPixelColor(PIXEL_LEFT);
  uint32_t rightCol = leds.getPixelColor(PIXEL_RIGHT);

  // Clone LEFT->RIGHT during stock preemption (animation mode letting OG through),
  // OR when running stock with copyLeftToRight enabled and no custom handler.
  if (preemptActive) {
    rightCol = leftCol;                      // clone for this frame
  } else if ((customHandler == nullptr) && copyLeftToRight) {
    rightCol = leftCol;                      // stock mirror
  }

  // Write back (ensures both internal pixels reflect same values we mirror)
  leds.setPixelColor(PIXEL_LEFT,  leftCol);
  leds.setPixelColor(PIXEL_RIGHT, rightCol);
  leds.show();

  // Fill entire external strips with their respective colors
  mirrorFillStrip(extLeft,  leftCol);
  mirrorFillStrip(extRight, rightCol);

  // Only valid for this frame
  preemptActive = false;
}

void loop() {
#if OXFP_SELFTEST
  runSelfTest();
  showMirrored();
  return;
#endif

  preemptActive = false; // default each tick

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
  const bool idle    = isIdleGreen(LG, LR, RG, RR);

  // Preempt custom during error/FRAG OR for any non-idle blinking states if requested
  if (customHandler && ((preemptOnError && isError) || (preemptOnNonIdle && !idle))) {
    preemptActive = true;            // triggers L->R clone in showMirrored()
    renderStock(LG, LR, RG, RR);     // stock hues from OG lines
    showMirrored();
    return;
  }

  // Custom or stock
  if (customHandler) {
    customHandler();                 // user/animation draws
    showMirrored();
    return;
  }
  renderStock(LG, LR, RG, RR);       // plain stock
  showMirrored();
}

void ledCustomOverride(void (*handler)(void)) { customHandler = handler; }
void setPreemptOnError(bool enable) { preemptOnError = enable; }
void setPreemptOnNonIdle(bool enable) { preemptOnNonIdle = enable; }
void setCopyLeftToRight(bool enable) { copyLeftToRight = enable; }

void setGlobalBrightness(uint8_t b) {
  gBrightness = b ? b : 1;           // avoid 0 internal clamp oddities
  leds.setBrightness(gBrightness);
  extLeft.setBrightness(gBrightness);
  extRight.setBrightness(gBrightness);
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
