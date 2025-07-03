#include "oxfp_orig.h"
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#define PIN_LGI     15
#define PIN_RGI     19
#define PIN_LRI     18
#define PIN_RRI     14
#define WS2812_PIN  5
#define NUM_LEDS    2

Adafruit_NeoPixel leds(NUM_LEDS, WS2812_PIN, NEO_GRB + NEO_KHZ800);

namespace {
    void (*customHandler)(void) = nullptr;
}

static void updateLedsFromInputs() {
    // Active low logic
    bool leftGreenOn  = (digitalRead(PIN_LGI) == LOW);
    bool leftRedOn    = (digitalRead(PIN_LRI) == LOW);
    bool rightGreenOn = (digitalRead(PIN_RGI) == LOW);
    bool rightRedOn   = (digitalRead(PIN_RRI) == LOW);

    // Debug print all states
    Serial.printf("LGI:%d RGI:%d LRI:%d RRI:%d\n", leftGreenOn, rightGreenOn, leftRedOn, rightRedOn);

if (!leftGreenOn && !leftRedOn && !rightGreenOn && !rightRedOn) {
    leds.setPixelColor(0, 0);
    leds.setPixelColor(1, 0);
    leds.show();
    return;
}

    // RIGHT LED (Pixel 0)
    uint32_t rightColor = 0;
    if (rightGreenOn && rightRedOn)
        rightColor = leds.Color(255, 128, 0);      // Orange
    else if (rightGreenOn)
        rightColor = leds.Color(0, 255, 0);        // Green
    else if (rightRedOn)
        rightColor = leds.Color(255, 0, 0);        // Red
    else
        rightColor = leds.Color(0, 0, 0);          // Off

    // LEFT LED (Pixel 1)
    uint32_t leftColor = 0;
    if (leftGreenOn && leftRedOn)
        leftColor = leds.Color(255, 128, 0);       // Orange
    else if (leftGreenOn)
        leftColor = leds.Color(0, 255, 0);         // Green
    else if (leftRedOn)
        leftColor = leds.Color(255, 0, 0);         // Red
    else
        leftColor = leds.Color(0, 0, 0);           // Off

    leds.setPixelColor(0, leftColor); // Pixel 0 = RIGHT
    leds.setPixelColor(1, rightColor);  // Pixel 1 = LEFT
    leds.show();
}

namespace OXFP_orig {

void begin() {
    Serial.begin(115200);
    delay(100); // Allow time for Serial to init

    pinMode(PIN_LGI, INPUT);
    pinMode(PIN_RGI, INPUT);
    pinMode(PIN_LRI, INPUT);
    pinMode(PIN_RRI, INPUT);
    leds.begin();
    leds.clear();
    leds.show();
    Serial.println("[OXFP_orig] begin() complete");
}

void loop() {
    if (customHandler) {
        customHandler();
        return;
    }
    updateLedsFromInputs();
}

void ledCustomOverride(void (*handler)(void)) {
    customHandler = handler;
}

} // namespace OXFP_orig
