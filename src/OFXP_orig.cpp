#include "oxfp_orig.h"
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

// --- Pin config ---
#define PIN_LGI  1  // IO1  (LGI)
#define PIN_RGI  2  // IO2  (RGI)
#define PIN_LRI  3  // IO3  (LRI)
#define PIN_RRI  4  // IO4  (RRI)
#define WS2812_PIN 5  // IO5 ("RGB")
#define NUM_LEDS 2   // Both LEDs chained

// --- Shared NeoPixel object (extern'ed in .h) ---
Adafruit_NeoPixel leds(NUM_LEDS, WS2812_PIN, NEO_GRB + NEO_KHZ800);

static void (*customHandler)(void) = nullptr;

void updateLedsFromInputs() {
    // Read inputs
    bool leftGreen  = digitalRead(PIN_LGI);
    bool rightGreen = digitalRead(PIN_RGI);
    bool leftRed    = digitalRead(PIN_LRI);
    bool rightRed   = digitalRead(PIN_RRI);

    // Right LED logic (LED 0)
    uint32_t rightColor = 0;
    if (rightGreen && rightRed)   rightColor = leds.Color(255, 128, 0); // Orange
    else if (rightGreen)          rightColor = leds.Color(0, 255, 0);   // Green
    else if (rightRed)            rightColor = leds.Color(255, 0, 0);   // Red
    else                          rightColor = leds.Color(0, 0, 0);     // Off

    // Left LED logic (LED 1)
    uint32_t leftColor = 0;
    if (leftGreen && leftRed)     leftColor = leds.Color(255, 128, 0);  // Orange
    else if (leftGreen)           leftColor = leds.Color(0, 255, 0);    // Green
    else if (leftRed)             leftColor = leds.Color(255, 0, 0);    // Red
    else                          leftColor = leds.Color(0, 0, 0);      // Off

    leds.setPixelColor(0, rightColor); // LED 0: Right
    leds.setPixelColor(1, leftColor);  // LED 1: Left
    leds.show();
}

namespace OXFP_orig {

void begin() {
    pinMode(PIN_LGI, INPUT);
    pinMode(PIN_RGI, INPUT);
    pinMode(PIN_LRI, INPUT);
    pinMode(PIN_RRI, INPUT);
    leds.begin();
    leds.clear();
    leds.show();
}

void loop() {
    if (customHandler) {
        customHandler();
    } else {
        updateLedsFromInputs();
    }
}

void setCustomHandler(void (*handler)(void)) {
    customHandler = handler;
}

} // namespace OXFP_orig
