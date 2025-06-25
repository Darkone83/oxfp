#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>

namespace OXFP_config {
    enum Mode : uint8_t { Static = 0, Animation = 1 };

    enum Animation : uint8_t {
        Pulse = 0,
        Fade,
        Rainbow,
        DualRainbow,
        ColorChase,
        Sparkle,
        AnimationCount // keep this last!
    };

    struct Settings {
        Mode mode;
        uint8_t static_right_red;
        uint8_t static_right_green;
        uint8_t static_right_orange;
        uint8_t static_left_red;
        uint8_t static_left_green;
        uint8_t static_left_orange;
        uint8_t animation_id;     // which effect
        uint8_t animation_color_a; // main color (palette index)
        uint8_t animation_color_b; // secondary/accent color (palette index, used for some modes)
        uint8_t brightness; // 0-255
    };

    void begin(AsyncWebServer& server);
    void loop();
    const Settings& getSettings();
    void setSettings(const Settings& s);
    bool isActive();
}
