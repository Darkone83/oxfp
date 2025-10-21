#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

// --- Animation Modes (stable IDs; keep in sync with UI/UDP) ---
enum class OXFP_AnimMode : uint8_t {
    ColorBounce   = 0,
    Breathing     = 1,
    Chase         = 2,
    RGBFade       = 3,
    Blinking      = 4,
    Alternating   = 5,
    FireFlicker   = 6,
    Plasma        = 7,   // NEW
    Heartbeat     = 8,   // NEW
    OpposedBreath = 9,   // NEW
    Sparkle       = 10,  // NEW
    Animation_Count       // Not a mode; count sentinel
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
    OXFP_Mode mode = OXFP_Mode::Stock;      // Stock/Static/Animation
    uint8_t brightness = 128;               // 1-255 global LED brightness

    // Static mode colors (used for error mapping too)
    OXFP_RGB greenColor  = {0, 255, 0};     // Default Xbox green
    OXFP_RGB redColor    = {255, 0, 0};     // Default red
    OXFP_RGB orangeColor = {255, 128, 0};   // Default amber/orange

    // Animation mode
    OXFP_AnimMode animMode = OXFP_AnimMode::ColorBounce;
    OXFP_RGB animColorA    = {0, 128, 255}; // LED 0
    OXFP_RGB animColorB    = {255, 0, 128}; // LED 1
    uint8_t animSpeed      = 5;             // 1..10
};

namespace OXFP_config {
    // Attach web routes/UI
    void begin(AsyncWebServer& server);

    // Persistence (NVS)
    void loadPreferences();
    void savePreferences();
    void resetPreferences();

    // Apply currently selected mode/animation (call from loop or when state changes)
    void applyConfig();

    // Temporarily preview a given config (auto-reverts after internal timeout)
    void preview(const OXFP_Config& tmp);

    // Read-only access to the current saved/live config snapshot
    const OXFP_Config& getConfig();

    // --- NEW: Setter for programmatic updates (e.g., UDP) ---
    // Replaces the current in-RAM config with |newCfg|, applies it immediately,
    // and optionally persists it to NVS when alsoSave == true.
    void setConfig(const OXFP_Config& newCfg, bool alsoSave = false);
}
