// OXFP_config.cpp — full file
#include "OXFP_config.h"
#include <Preferences.h>
#include "OXFP_orig.h"
#include <ArduinoJson.h>
#include <Arduino.h>
#include <math.h>

namespace {
    Preferences prefs;
    OXFP_Config config;

    // Preview state
    bool inPreview = false;
    OXFP_Config previewConfig;
    unsigned long previewTimer = 0;

    // Shared animation state (used by custom handlers)
    unsigned long lastAnimUpdate = 0;
    uint8_t animFrame = 0;

    // Brightness scaling helper
    uint32_t rgbWithBrightness(const OXFP_RGB& c, uint8_t brightness) {
        return leds.Color(
            (uint8_t)((c.r * brightness) / 255),
            (uint8_t)((c.g * brightness) / 255),
            (uint8_t)((c.b * brightness) / 255)
        );
    }

    // Render for Static mode that still reflects Xbox error inputs with assigned colors
    void renderStaticWithErrorAwareness(const OXFP_Config& c) {
        // Console OFF => blank
        if (OXFP_orig::consoleIsOff()) {
            leds.clear(); leds.show(); return;
        }

        bool LG, LR, RG, RR;
        OXFP_orig::readInputLines(LG, LR, RG, RR);

        // If no error is active, show the configured static color for both pixels
        const bool err = OXFP_orig::errorActive();
        if (!err) {
            const uint32_t col = rgbWithBrightness(c.greenColor, c.brightness);
            leds.setPixelColor(0, col);
            leds.setPixelColor(1, col);
            leds.show();
            return;
        }

        // Error visible: per-side mapping using assigned colors
        auto pick = [&](bool g, bool r)->uint32_t {
            if (g && r) return rgbWithBrightness(c.orangeColor, c.brightness);
            if (r)      return rgbWithBrightness(c.redColor,    c.brightness);
            if (g)      return rgbWithBrightness(c.greenColor,  c.brightness);
            return leds.Color(0,0,0);
        };
        leds.setPixelColor(0, pick(LG, LR)); // LEFT
        leds.setPixelColor(1, pick(RG, RR)); // RIGHT
        leds.show();
    }

    void endPreview() { inPreview = false; }

    // Unified renderer for both preview and saved config
    void renderFromConfig(const OXFP_Config& c) {
        // Console OFF always blanks (regardless of mode)
        if (OXFP_orig::consoleIsOff()) {
            OXFP_orig::setPreemptOnError(false);
            OXFP_orig::ledCustomOverride([]{
                leds.clear(); leds.show();
            });
            return;
        }

        switch (c.mode) {
            case OXFP_Mode::Stock: {
                // Pure mirroring of Xbox lines (OG colors + blink patterns)
                OXFP_orig::setPreemptOnError(false);
                OXFP_orig::ledCustomOverride(nullptr);
                return;
            }

            case OXFP_Mode::Static: {
                // Show static color normally; if Xbox errors, reflect with assigned colors.
                OXFP_orig::setPreemptOnError(false); // Static handles errors itself
                OXFP_orig::ledCustomOverride([]{
                    const auto& cc = inPreview ? previewConfig : config;
                    renderStaticWithErrorAwareness(cc);
                });
                return;
            }

            case OXFP_Mode::Animation: {
                // If an error is active, interrupt animation with stock mirroring.
                OXFP_orig::setPreemptOnError(true);  // allow runtime preemption while animating
                if (OXFP_orig::errorActive()) {
                    OXFP_orig::ledCustomOverride(nullptr);
                    return;
                }

                // Otherwise run the selected animation
                OXFP_orig::ledCustomOverride([]{
                    const auto& c = inPreview ? previewConfig : config;
                    const unsigned long now = millis();
                    const uint8_t s = c.animSpeed ? c.animSpeed : 1;

                    const uint32_t cA = rgbWithBrightness(c.animColorA, c.brightness);
                    const uint32_t cB = rgbWithBrightness(c.animColorB, c.brightness);

                    switch (c.animMode) {
                        case OXFP_AnimMode::ColorBounce: {
                            if (now - lastAnimUpdate > (400 / s)) {
                                animFrame ^= 1;
                                lastAnimUpdate = now;
                            }
                            leds.setPixelColor(animFrame,     cA);
                            leds.setPixelColor(!animFrame,    cB);
                            leds.show();
                            break;
                        }

                        case OXFP_AnimMode::Breathing: {
                            if (now - lastAnimUpdate > 16) {
                                lastAnimUpdate = now;
                                animFrame++;
                            }
                            const float b = (sinf(animFrame / 12.0f) + 1.0f) * 0.5f; // 0..1
                            const uint8_t bright = (uint8_t)(c.brightness * b);
                            leds.setPixelColor(0, rgbWithBrightness(c.animColorA, bright));
                            leds.setPixelColor(1, rgbWithBrightness(c.animColorB, bright));
                            leds.show();
                            break;
                        }

                        case OXFP_AnimMode::Chase: {
                            if (now - lastAnimUpdate > (200 / s)) {
                                animFrame = (animFrame + 1) & 0x01; // 0,1,0,1...
                                lastAnimUpdate = now;
                            }
                            leds.setPixelColor(animFrame,   cA);
                            leds.setPixelColor(!animFrame,  cB);
                            leds.show();
                            break;
                        }

                        case OXFP_AnimMode::RGBFade: {
                            if (now - lastAnimUpdate > (16 * (11 - s))) {
                                animFrame++;
                                lastAnimUpdate = now;
                            }
                            const float x = (animFrame & 127) / 128.0f;
                            const uint8_t r = (uint8_t)(sinf(2.0f * PI * x) * 127 + 128);
                            const uint8_t g = (uint8_t)(sinf(2.0f * PI * x + 2.09f) * 127 + 128);
                            const uint8_t b = (uint8_t)(sinf(2.0f * PI * x + 4.19f) * 127 + 128);
                            const uint32_t col = rgbWithBrightness({r,g,b}, c.brightness);
                            leds.setPixelColor(0, col);
                            leds.setPixelColor(1, col);
                            leds.show();
                            break;
                        }

                        case OXFP_AnimMode::Blinking: {
                            if (now - lastAnimUpdate > (400 / s)) {
                                animFrame ^= 1;
                                lastAnimUpdate = now;
                            }
                            leds.setPixelColor(0, animFrame ? cA : 0);
                            leds.setPixelColor(1, animFrame ? cB : 0);
                            leds.show();
                            break;
                        }

                        case OXFP_AnimMode::Alternating: {
                            if (now - lastAnimUpdate > (350 / s)) {
                                animFrame ^= 1;
                                lastAnimUpdate = now;
                            }
                            leds.setPixelColor(animFrame,    cA);
                            leds.setPixelColor(!animFrame,   cB);
                            leds.show();
                            break;
                        }

                        case OXFP_AnimMode::FireFlicker: {
                            if (now - lastAnimUpdate > 70) {
                                lastAnimUpdate = now;
                                const uint8_t r1 = 180 + (uint8_t)random(0, 75);
                                const uint8_t g1 =  50 + (uint8_t)random(0, 60);
                                const uint8_t b1 = (uint8_t)random(0, 16);
                                const uint8_t r2 = 180 + (uint8_t)random(0, 75);
                                const uint8_t g2 =  50 + (uint8_t)random(0, 60);
                                const uint8_t b2 = (uint8_t)random(0, 16);
                                leds.setPixelColor(0, rgbWithBrightness({r1,g1,b1}, c.brightness));
                                leds.setPixelColor(1, rgbWithBrightness({r2,g2,b2}, c.brightness));
                                leds.show();
                            }
                            break;
                        }

                        default: break;
                    }
                });
                return;
            }

            default: {
                OXFP_orig::setPreemptOnError(false);
                OXFP_orig::ledCustomOverride(nullptr);
                return;
            }
        }
    }
} // namespace

// ---------------- Preferences ----------------
void OXFP_config::loadPreferences() {
    prefs.begin("oxfp", true);
    config.mode        = (OXFP_Mode)prefs.getUChar("mode", (uint8_t)OXFP_Mode::Stock);
    config.brightness  = prefs.getUChar("bright", 128);

    config.greenColor  = { prefs.getUChar("gr",0),   prefs.getUChar("gg",255), prefs.getUChar("gb",0) };
    config.redColor    = { prefs.getUChar("rr",255), prefs.getUChar("rg",0),   prefs.getUChar("rb",0) };
    config.orangeColor = { prefs.getUChar("or",255), prefs.getUChar("og",128), prefs.getUChar("ob",0) };

    config.animMode    = (OXFP_AnimMode)prefs.getUChar("anim", 0);
    config.animColorA  = { prefs.getUChar("arA",0),   prefs.getUChar("agA",128), prefs.getUChar("abA",255) };
    config.animColorB  = { prefs.getUChar("arB",255), prefs.getUChar("agB",0),   prefs.getUChar("abB",128) };
    config.animSpeed   = prefs.getUChar("spd", 5);
    prefs.end();
}
void OXFP_config::savePreferences() {
    prefs.begin("oxfp", false);
    prefs.putUChar("mode",   (uint8_t)config.mode);
    prefs.putUChar("bright", config.brightness);

    prefs.putUChar("gr", config.greenColor.r);  prefs.putUChar("gg", config.greenColor.g);  prefs.putUChar("gb", config.greenColor.b);
    prefs.putUChar("rr", config.redColor.r);    prefs.putUChar("rg", config.redColor.g);    prefs.putUChar("rb", config.redColor.b);
    prefs.putUChar("or", config.orangeColor.r); prefs.putUChar("og", config.orangeColor.g); prefs.putUChar("ob", config.orangeColor.b);

    prefs.putUChar("anim", (uint8_t)config.animMode);
    prefs.putUChar("arA", config.animColorA.r); prefs.putUChar("agA", config.animColorA.g); prefs.putUChar("abA", config.animColorA.b);
    prefs.putUChar("arB", config.animColorB.r); prefs.putUChar("agB", config.animColorB.g); prefs.putUChar("abB", config.animColorB.b);
    prefs.putUChar("spd", config.animSpeed);
    prefs.end();
}
void OXFP_config::resetPreferences() {
    config = OXFP_Config(); // defaults from struct
    savePreferences();
}
const OXFP_Config& OXFP_config::getConfig() { return config; }

// ---------------- Lifecycle ----------------
void OXFP_config::preview(const OXFP_Config& tmp) {
    inPreview = true;
    previewConfig = tmp;
    previewTimer = millis();
}

void OXFP_config::applyConfig() {
    // Auto-exit preview after 8s
    if (inPreview && (millis() - previewTimer > 8000)) {
        endPreview();
    }
    renderFromConfig(inPreview ? previewConfig : config);
}

// ---------------- Web UI & API ----------------
static String configToJson(const OXFP_Config& c) {
    StaticJsonDocument<384> doc;
    doc["mode"] = (uint8_t)c.mode;
    doc["brightness"] = c.brightness;

    JsonArray greenArr = doc.createNestedArray("greenColor");
    greenArr.add(c.greenColor.r);
    greenArr.add(c.greenColor.g);
    greenArr.add(c.greenColor.b);

    JsonArray redArr = doc.createNestedArray("redColor");
    redArr.add(c.redColor.r);
    redArr.add(c.redColor.g);
    redArr.add(c.redColor.b);

    JsonArray orangeArr = doc.createNestedArray("orangeColor");
    orangeArr.add(c.orangeColor.r);
    orangeArr.add(c.orangeColor.g);
    orangeArr.add(c.orangeColor.b);

    doc["animMode"] = (uint8_t)c.animMode;

    JsonArray animArrA = doc.createNestedArray("animColorA");
    animArrA.add(c.animColorA.r);
    animArrA.add(c.animColorA.g);
    animArrA.add(c.animColorA.b);

    JsonArray animArrB = doc.createNestedArray("animColorB");
    animArrB.add(c.animColorB.r);
    animArrB.add(c.animColorB.g);
    animArrB.add(c.animColorB.b);

    doc["animSpeed"] = c.animSpeed;

    String out;
    serializeJson(doc, out);
    return out;
}

void OXFP_config::begin(AsyncWebServer& server) {
    server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request){
        String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>OXFP LED Config</title>
    <meta name="viewport" content="width=320,initial-scale=1">
    <style>
        body {background:#111;color:#EEE;font-family:sans-serif;}
        .container {max-width:340px;margin:24px auto;background:#222;padding:2em;border-radius:8px;box-shadow:0 0 16px #0008;}
        h2 {margin-bottom:1em;}
        label {display:block;margin-top:.5em;margin-bottom:.1em;}
        input[type=color],input[type=range] {vertical-align:middle;}
        select,button,input {margin:.6em 0;padding:.4em;border-radius:6px;border:1px solid #444;}
        .btn-primary {background:#299a2c;color:white;}
        .btn-reset {background:#a22;color:white;}
        .btn-preview {background:#265aa5;color:white;}
        .row {margin-bottom:1em;}
    </style>
</head>
<body>
    <div class="container">
        <h2>OXFP LED Configuration</h2>
        <form id="configForm">
            <label>Mode:</label>
            <select id="mode" onchange="updateVisibility()">
                <option value="0">Stock</option>
                <option value="1">Static</option>
                <option value="2">Animation</option>
            </select>
            <div class="row">
                <label>Brightness:</label>
                <input type="range" id="brightness" min="1" max="255" value="128" oninput="bval.innerText=this.value">
                <span id="bval">128</span>
            </div>
            <div id="staticBlock" style="display:none">
                <label>Green Color:</label>
                <input type="color" id="greenColor">
                <label>Red Color:</label>
                <input type="color" id="redColor">
                <label>Orange Color:</label>
                <input type="color" id="orangeColor">
            </div>
            <div id="animBlock" style="display:none">
                <label>Animation:</label>
                <select id="animMode" onchange="updateAnimUI()">
                    <option value="0">Color Bounce</option>
                    <option value="1">Breathing/Pulse</option>
                    <option value="2">Chase</option>
                    <option value="3">RGB Fade</option>
                    <option value="4">Blinking</option>
                    <option value="5">Alternating</option>
                    <option value="6">Fire/Flicker</option>
                </select>
                <div id="animColorBlockA">
                    <label>Animation Color LED 0:</label>
                    <input type="color" id="animColorA">
                </div>
                <div id="animColorBlockB">
                    <label>Animation Color LED 1:</label>
                    <input type="color" id="animColorB">
                </div>
                <div id="animSpeedBlock">
                    <label>Speed:</label>
                    <input type="range" id="animSpeed" min="1" max="10" value="5" oninput="aval.innerText=this.value">
                    <span id="aval">5</span>
                </div>
            </div>
            <div class="row">
                <button type="button" class="btn-preview" onclick="previewConfig()">Preview</button>
                <button type="button" class="btn-primary" onclick="saveConfig()">Save</button>
                <button type="button" class="btn-reset" onclick="resetConfig()">Reset</button>
            </div>
        </form>
        <div id="status"></div>
    </div>
<script>
let config = {};
function rgb2hex(r,g,b){return "#"+((1<<24)|(r<<16)|(g<<8)|b).toString(16).slice(1);}
function hex2rgb(hex){let n=parseInt(hex.slice(1),16);return [n>>16&255,n>>8&255,n&255];}

function updateVisibility() {
    let mode = +document.getElementById('mode').value;
    document.getElementById('staticBlock').style.display = (mode==1) ? '' : 'none';
    document.getElementById('animBlock').style.display = (mode==2) ? '' : 'none';
}
function updateAnimUI() {
    let anim = +document.getElementById('animMode').value;
    // Show/hide color pickers and speed slider as needed
    let showColors = ![3,6].includes(anim);
    document.getElementById('animColorBlockA').style.display = showColors ? '' : 'none';
    document.getElementById('animColorBlockB').style.display = showColors ? '' : 'none';
    document.getElementById('animSpeedBlock').style.display = (anim==6) ? 'none' : '';
}
function fillForm() {
    document.getElementById('mode').value = config.mode;
    document.getElementById('brightness').value = config.brightness;
    bval.innerText = config.brightness;
    document.getElementById('greenColor').value = rgb2hex(...config.greenColor);
    document.getElementById('redColor').value = rgb2hex(...config.redColor);
    document.getElementById('orangeColor').value = rgb2hex(...config.orangeColor);
    document.getElementById('animMode').value = config.animMode;
    document.getElementById('animColorA').value = rgb2hex(...config.animColorA);
    document.getElementById('animColorB').value = rgb2hex(...config.animColorB);
    document.getElementById('animSpeed').value = config.animSpeed;
    aval.innerText = config.animSpeed;
    updateVisibility();
    updateAnimUI();
}
function fetchConfig() {
    fetch('/api/ledconfig').then(r=>r.json()).then(j=>{
        config=j;
        fillForm();
    });
}
function gatherConfig() {
    let c = {
        mode:+document.getElementById('mode').value,
        brightness:+document.getElementById('brightness').value,
        greenColor: hex2rgb(document.getElementById('greenColor').value),
        redColor: hex2rgb(document.getElementById('redColor').value),
        orangeColor: hex2rgb(document.getElementById('orangeColor').value),
        animMode:+document.getElementById('animMode').value,
        animColorA: hex2rgb(document.getElementById('animColorA').value),
        animColorB: hex2rgb(document.getElementById('animColorB').value),
        animSpeed:+document.getElementById('animSpeed').value
    };
    return c;
}
function previewConfig() {
    let c = gatherConfig();
    fetch('/api/ledpreview', {
        method:'POST',
        headers: {'Content-Type':'application/json'},
        body:JSON.stringify(c)
    });
}
function saveConfig() {
    let c = gatherConfig();
    fetch('/api/ledsave', {
        method:'POST',
        headers: {'Content-Type':'application/json'},
        body:JSON.stringify(c)
    }).then(r=>r.text()).then(t=>{
        document.getElementById('status').innerText=t;
        setTimeout(()=>document.getElementById('status').innerText='', 1500);
        fetchConfig();
    });
}
function resetConfig() {
    fetch('/api/ledreset', {method:'POST'}).then(()=>fetchConfig());
}
fetchConfig();
</script>
</body>
</html>
        )rawliteral";
        request->send(200, "text/html", html);
    });

    server.on("/api/ledconfig", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "application/json", configToJson(config));
    });

    server.on(
        "/api/ledpreview", HTTP_POST,
        [](AsyncWebServerRequest *request){},
        NULL,
        [](AsyncWebServerRequest *request, uint8_t* data, size_t len, size_t, size_t){
            StaticJsonDocument<384> doc;
            deserializeJson(doc, data, len);
            OXFP_Config tmp;
            tmp.mode = (OXFP_Mode)doc["mode"].as<uint8_t>();
            tmp.brightness = doc["brightness"].as<uint8_t>();

            auto arr = doc["greenColor"].as<JsonArray>();
            tmp.greenColor = { arr[0], arr[1], arr[2] };

            arr = doc["redColor"].as<JsonArray>();
            tmp.redColor = { arr[0], arr[1], arr[2] };

            arr = doc["orangeColor"].as<JsonArray>();
            tmp.orangeColor = { arr[0], arr[1], arr[2] };

            tmp.animMode = (OXFP_AnimMode)doc["animMode"].as<uint8_t>();

            auto arrA = doc["animColorA"].as<JsonArray>();
            tmp.animColorA = { arrA[0], arrA[1], arrA[2] };

            auto arrB = doc["animColorB"].as<JsonArray>();
            tmp.animColorB = { arrB[0], arrB[1], arrB[2] };

            tmp.animSpeed = doc["animSpeed"].as<uint8_t>();

            OXFP_config::preview(tmp);
            request->send(200, "text/plain", "Previewing");
        }
    );

    server.on(
        "/api/ledsave", HTTP_POST,
        [](AsyncWebServerRequest *request){},
        NULL,
        [](AsyncWebServerRequest *request, uint8_t* data, size_t len, size_t, size_t){
            StaticJsonDocument<384> doc;
            deserializeJson(doc, data, len);

            config.mode = (OXFP_Mode)doc["mode"].as<uint8_t>();
            config.brightness = doc["brightness"].as<uint8_t>();

            auto arr = doc["greenColor"].as<JsonArray>();
            config.greenColor = { arr[0], arr[1], arr[2] };

            arr = doc["redColor"].as<JsonArray>();
            config.redColor = { arr[0], arr[1], arr[2] };

            arr = doc["orangeColor"].as<JsonArray>();
            config.orangeColor = { arr[0], arr[1], arr[2] };

            config.animMode = (OXFP_AnimMode)doc["animMode"].as<uint8_t>();

            auto arrA = doc["animColorA"].as<JsonArray>();
            config.animColorA = { arrA[0], arrA[1], arrA[2] };

            auto arrB = doc["animColorB"].as<JsonArray>();
            config.animColorB = { arrB[0], arrB[1], arrB[2] };

            config.animSpeed = doc["animSpeed"].as<uint8_t>();

            OXFP_config::savePreferences();
            inPreview = false;
            OXFP_config::applyConfig();
            request->send(200, "text/plain", "Config saved!");
        }
    );

    server.on("/api/ledreset", HTTP_POST, [](AsyncWebServerRequest *request){
        OXFP_config::resetPreferences();
        inPreview = false;
        OXFP_config::applyConfig();
        request->send(200, "text/plain", "Reset to defaults.");
    });
}
