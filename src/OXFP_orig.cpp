#include "oxfp_orig.h"
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#define PIN_LGI     1
#define PIN_RGI     2
#define PIN_LRI     3
#define PIN_RRI     4
#define WS2812_PIN  5
#define NUM_LEDS    2

Adafruit_NeoPixel leds(NUM_LEDS, WS2812_PIN, NEO_GRB + NEO_KHZ800);

namespace {
    void (*customHandler)(void) = nullptr;
    bool bootSequenceDone = false;
    bool noInputDetected = false;
    unsigned long lastWhiteFlash = 0;
    bool whiteOn = false;
}

static void checkPanelInput() {
    bool leftGreen  = !digitalRead(PIN_LGI);
    bool rightGreen = !digitalRead(PIN_RGI);
    bool leftRed    = !digitalRead(PIN_LRI);
    bool rightRed   = !digitalRead(PIN_RRI);

    if (!leftGreen && !rightGreen && !leftRed && !rightRed) {
        noInputDetected = true;
    } else {
        noInputDetected = false;
    }
}

static void flashWhiteIfNoInput() {
    unsigned long now = millis();
    if (noInputDetected) {
        if (now - lastWhiteFlash >= 2000) {
            whiteOn = !whiteOn;
            lastWhiteFlash = now;
        }
        if (whiteOn) {
            leds.setPixelColor(0, leds.Color(128, 128, 128));
            leds.setPixelColor(1, leds.Color(128, 128, 128));
        } else {
            leds.setPixelColor(0, 0);
            leds.setPixelColor(1, 0);
        }
        leds.show();
    }
}

static void updateLedsFromInputs() {
    bool leftGreen  = !digitalRead(PIN_LGI);
    bool rightGreen = !digitalRead(PIN_RGI);
    bool leftRed    = !digitalRead(PIN_LRI);
    bool rightRed   = !digitalRead(PIN_RRI);

    uint32_t rightColor = 0;
    if (rightGreen && rightRed)   rightColor = leds.Color(255, 128, 0);
    else if (rightGreen)          rightColor = leds.Color(0, 255, 0);
    else if (rightRed)            rightColor = leds.Color(255, 0, 0);
    else                          rightColor = leds.Color(0, 0, 0);

    uint32_t leftColor = 0;
    if (leftGreen && leftRed)     leftColor = leds.Color(255, 128, 0);
    else if (leftGreen)           leftColor = leds.Color(0, 255, 0);
    else if (leftRed)             leftColor = leds.Color(255, 0, 0);
    else                          leftColor = leds.Color(0, 0, 0);

    leds.setPixelColor(0, rightColor);
    leds.setPixelColor(1, leftColor);
    leds.show();
}

namespace OXFP_orig {

void begin() {
    pinMode(PIN_LGI, INPUT_PULLUP);
    pinMode(PIN_RGI, INPUT_PULLUP);
    pinMode(PIN_LRI, INPUT_PULLUP);
    pinMode(PIN_RRI, INPUT_PULLUP);
    leds.begin();
    leds.clear();
    leds.show();
    bootSequenceDone = false;
    noInputDetected = false;
    lastWhiteFlash = millis();
    whiteOn = false;
    Serial.println("[OXFP_orig] begin() complete");
}

void loop() {
    checkPanelInput();

    if (customHandler) {
        customHandler();   // This takes priority, including during "no input"
        return;
    }

    if (noInputDetected) {
        flashWhiteIfNoInput();
        return;
    }

    updateLedsFromInputs();
}

void ledCustomOverride(void (*handler)(void)) { // Renamed function
    customHandler = handler;
}

} // namespace OXFP_orig
