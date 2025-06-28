#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

// --- Animation Modes ---
enum class OXFP_AnimMode : uint8_t {
    ColorBounce = 0,
    Breathing,
    Chase,
    RGBFade,
    Blinking,
    Alternating,
    FireFlicker,
    Animation_Count // Not a mode; counts modes for UI
};

// --- Main Control Modes ---
enum class OXFP_Mode : uint8_t {
    Stock = 0,   // Default (use OXFP_orig pin logic)
    Static,      // User color choices for "Green", "Red", "Orange"
    Animation    // Runs one of the animation modes
};

// --- Color structure (packed RGB) ---
struct OXFP_RGB {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

struct OXFP_Config {
    OXFP_Mode mode = OXFP_Mode::Stock;              // Stock/Static/Animation
    uint8_t brightness = 128;                       // 0-255 global LED brightness
    // Static mode colors (for both LEDs)
    OXFP_RGB greenColor = {0, 255, 0};              // Default Xbox green
    OXFP_RGB redColor   = {255, 0, 0};              // Default red
    OXFP_RGB orangeColor= {255, 128, 0};            // Default amber/orange

    // Animation mode
    OXFP_AnimMode animMode = OXFP_AnimMode::ColorBounce;
    OXFP_RGB animColorA    = {0, 128, 255};  // LED 0
    OXFP_RGB animColorB    = {255, 0, 128};  // LED 1
    uint8_t animSpeed      = 5;
};

namespace OXFP_config {
    void begin(AsyncWebServer& server);         // Attach web routes/UI
    void loadPreferences();                     // Load settings from NVS
    void savePreferences();                     // Save settings to NVS
    void resetPreferences();                    // Reset to defaults
    void applyConfig();                         // Call from loop, applies selected mode/animation
    void preview(const OXFP_Config& tmp);       // Temporarily preview a given config
    const OXFP_Config& getConfig();             // Get current config
}

