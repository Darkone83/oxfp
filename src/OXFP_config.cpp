// OXFP_config.cpp — full file (older codebase, no mirroring; PROGMEM UI; chunk-safe POST; root untouched)
#include "OXFP_config.h"
#include <Preferences.h>
#include "OXFP_orig.h"
#include <ArduinoJson.h>
#include <Arduino.h>
#include <math.h>
#include <pgmspace.h>

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

    // Track last applied mode to safely reset animation state when switching modes
    bool modeInit = false;
    OXFP_Mode lastMode = OXFP_Mode::Stock;
    OXFP_AnimMode lastAnimMode = (OXFP_AnimMode)0;

    inline void resetAnimState() {
        lastAnimUpdate = 0;
        animFrame = 0;
    }
    inline void ensureAnimStateFor(OXFP_Mode m, OXFP_AnimMode a) {
        if (!modeInit || m != lastMode || (m == OXFP_Mode::Animation && a != lastAnimMode)) {
            resetAnimState();
            modeInit = true;
            lastMode = m;
            lastAnimMode = a;
        }
    }

    // Brightness scaling helper
    uint32_t rgbWithBrightness(const OXFP_RGB& c, uint8_t brightness) {
        return leds.Color(
            (uint8_t)((c.r * brightness) / 255),
            (uint8_t)((c.g * brightness) / 255),
            (uint8_t)((c.b * brightness) / 255)
        );
    }

    // Simple RGB mix helper
    inline OXFP_RGB mixRGB(const OXFP_RGB& a, const OXFP_RGB& b, float t) {
        if (t < 0.f) t = 0.f; else if (t > 1.f) t = 1.f;
        OXFP_RGB o;
        o.r = (uint8_t)(a.r + (b.r - a.r) * t + 0.5f);
        o.g = (uint8_t)(a.g + (b.g - a.g) * t + 0.5f);
        o.b = (uint8_t)(a.b + (b.b - a.b) * t + 0.5f);
        return o;
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

    // ===== Modernized UI (PROGMEM) =====
    static const char INDEX_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>OXFP LED Config</title>
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <style>
    :root{
      --bg:#0e0f12; --panel:#171a1f; --muted:#99a2b2; --text:#e8ebf1;
      --accent:#52d273; --accent-2:#5aa9ff; --danger:#ff5964; --warn:#ffaf40;
      --border:#242833; --shadow:0 8px 24px rgba(0,0,0,.35);
      --radius:12px;
    }
    *{box-sizing:border-box}
    body{margin:0;background:var(--bg);color:var(--text);font:14px/1.4 system-ui,Segoe UI,Roboto,Helvetica,Arial}
    .wrap{max-width:720px;margin:32px auto;padding:0 16px}
    .card{background:var(--panel);border:1px solid var(--border);border-radius:var(--radius);box-shadow:var(--shadow);padding:20px}
    h1{font-size:20px;margin:0 0 12px}
    .row{display:grid;grid-template-columns:160px 1fr;gap:12px;align-items:center;margin:10px 0}
    label{color:var(--muted)}
    select,input,button{border-radius:10px;border:1px solid var(--border);background:#11151a;color:var(--text)}
    select,input[type=number],input[type=text]{padding:10px}
    input[type=range]{width:100%}
    input[type=color]{width:48px;height:32px;border-radius:6px;padding:0;border:0;background:transparent}
    .grid{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:10px}
    .hint{color:var(--muted);font-size:12px;margin-top:2px}
    .btns{display:flex;gap:10px;margin-top:14px}
    .btn{padding:10px 14px;cursor:pointer}
    .primary{background:var(--accent);border-color:transparent;color:#08120a;font-weight:600}
    .ghost{background:transparent}
    .warn{background:var(--accent-2);border-color:transparent;color:#08101a;font-weight:600}
    .danger{background:var(--danger);border-color:transparent;color:white}
    .badge{display:inline-block;padding:2px 8px;border-radius:999px;background:#12151b;border:1px solid var(--border);color:var(--muted);font-size:12px;margin-left:8px}
    .section-title{display:flex;align-items:center;gap:8px;margin-top:8px}
  </style>
</head>
<body>
<div class="wrap">
  <div class="card">
    <h1>OXFP LED Configuration <span id="liveMode" class="badge">—</span></h1>

    <div class="row">
      <label for="mode">Mode</label>
      <select id="mode" onchange="updateVisibility();updateBadge();">
        <option value="0">Stock (OG behavior)</option>
        <option value="1">Static (with error colors)</option>
        <option value="2">Animation</option>
      </select>
    </div>

    <div class="row">
      <label for="brightness">Brightness</label>
      <div>
        <input type="range" id="brightness" min="1" max="255" value="128" oninput="bval.textContent=this.value">
        <div class="hint">Value: <b id="bval">128</b></div>
      </div>
    </div>

    <div id="staticBlock" class="section" style="display:none">
      <div class="section-title"><strong>Static Colors</strong></div>
      <div class="grid">
        <div>
          <label>Green</label>
          <input type="color" id="greenColor">
        </div>
        <div>
          <label>Red</label>
          <input type="color" id="redColor">
        </div>
        <div>
          <label>Orange</label>
          <input type="color" id="orangeColor">
        </div>
      </div>
      <div class="hint">Used for OG-status mapping when in Static mode (and for error visibility).</div>
    </div>

    <div id="animBlock" class="section" style="display:none">
      <div class="section-title"><strong>Animation</strong></div>
      <div class="row">
        <label for="animMode">Pattern</label>
        <select id="animMode" onchange="updateAnimUI()">
          <option value="0">Color Bounce</option>
          <option value="1">Breathing / Pulse</option>
          <option value="2">Chase</option>
          <option value="3">RGB Fade</option>
          <option value="4">Blinking</option>
          <option value="5">Alternating</option>
          <option value="6">Fire / Flicker</option>
          <option value="7">Plasma</option>
          <option value="8">Heartbeat</option>
          <option value="9">Opposed Breathing</option>
          <option value="10">Sparkle</option>
        </select>
      </div>

      <div class="grid" id="animColorRow">
        <div>
          <label>Anim Color A</label>
          <input type="color" id="animColorA">
        </div>
        <div>
          <label>Anim Color B</label>
          <input type="color" id="animColorB">
        </div>
      </div>

      <div class="row" id="animSpeedRow">
        <label for="animSpeed">Speed</label>
        <div>
          <input type="range" id="animSpeed" min="1" max="10" value="5" oninput="aval.textContent=this.value">
          <div class="hint">Value: <b id="aval">5</b></div>
        </div>
      </div>
    </div>

    <div class="btns">
      <button class="btn warn"    type="button" onclick="previewConfig()">Preview</button>
      <button class="btn primary" type="button" onclick="saveConfig()">Apply & Save</button>
      <button class="btn ghost"   type="button" onclick="resetConfig()">Reset to Defaults</button>
    </div>

    <div id="status" class="hint" style="margin-top:10px;"></div>
  </div>
</div>

<script>
let config = {};

function rgb2hex(r,g,b){return "#"+((1<<24)|(r<<16)|(g<<8)|b).toString(16).slice(1);}
function hex2rgb(hex){let n=parseInt(hex.slice(1),16);return [n>>16&255,n>>8&255,n&255];}

function updateBadge(){
  const m = +document.getElementById('mode').value;
  const el = document.getElementById('liveMode');
  el.textContent = (m===0 ? 'Stock' : m===1 ? 'Static' : 'Animation');
}

function updateVisibility() {
  let mode = +document.getElementById('mode').value;
  document.getElementById('staticBlock').style.display = (mode==1) ? '' : 'none';
  document.getElementById('animBlock').style.display   = (mode==2) ? '' : 'none';
}

function updateAnimUI() {
  let anim = +document.getElementById('animMode').value;
  // RGB Fade (3) and Fire (6) generate colors; hide pickers there. Others use the two pickers.
  let showColors = ![3,6].includes(anim);
  document.getElementById('animColorRow').style.display = showColors ? '' : 'none';
  // Fire ignores speed slider
  document.getElementById('animSpeedRow').style.display = (anim==6) ? 'none' : '';
}

function fillForm() {
  document.getElementById('mode').value = config.mode;
  document.getElementById('brightness').value = config.brightness;
  bval.textContent = config.brightness;

  document.getElementById('greenColor').value  = rgb2hex(...config.greenColor);
  document.getElementById('redColor').value    = rgb2hex(...config.redColor);
  document.getElementById('orangeColor').value = rgb2hex(...config.orangeColor);

  document.getElementById('animMode').value = config.animMode;
  document.getElementById('animColorA').value = rgb2hex(...config.animColorA);
  document.getElementById('animColorB').value = rgb2hex(...config.animColorB);
  document.getElementById('animSpeed').value = config.animSpeed;
  aval.textContent = config.animSpeed;

  updateVisibility();
  updateAnimUI();
  updateBadge();
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
</html>)rawliteral";

    // ========= Unified renderer =========
    void renderFromConfig(const OXFP_Config& c) {
        // Console OFF always blanks (regardless of mode)
        if (OXFP_orig::consoleIsOff()) {
            OXFP_orig::setPreemptOnError(false);
            OXFP_orig::ledCustomOverride([]{
                leds.clear(); leds.show();
            });
            return;
        }

        // Reset animation state on mode/anim changes to avoid stale indexes (fixes Color Bounce)
        ensureAnimStateFor(c.mode, c.animMode);

        switch (c.mode) {
            case OXFP_Mode::Stock: {
                OXFP_orig::setPreemptOnError(false);
                OXFP_orig::ledCustomOverride(nullptr);
                return;
            }
            case OXFP_Mode::Static: {
                OXFP_orig::setPreemptOnError(false);
                OXFP_orig::ledCustomOverride([]{
                    const auto& cc = inPreview ? previewConfig : config;
                    renderStaticWithErrorAwareness(cc);
                });
                return;
            }
            case OXFP_Mode::Animation: {
                // FULL OVERRIDE: ignore SMC/error signals while animating
                OXFP_orig::setPreemptOnError(false);

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
                            uint8_t idx = (animFrame & 0x01);
                            leds.setPixelColor(idx,        cA);
                            leds.setPixelColor(idx ^ 0x01, cB);
                            leds.show();
                            break;
                        }
                        case OXFP_AnimMode::Breathing: {
                            if (now - lastAnimUpdate > 16) { lastAnimUpdate = now; animFrame++; }
                            const float b = (sinf(animFrame / 12.0f) + 1.0f) * 0.5f;
                            const uint8_t bright = (uint8_t)(c.brightness * b);
                            leds.setPixelColor(0, rgbWithBrightness(c.animColorA, bright));
                            leds.setPixelColor(1, rgbWithBrightness(c.animColorB, bright));
                            leds.show();
                            break;
                        }
                        case OXFP_AnimMode::Chase: {
                            if (now - lastAnimUpdate > (200 / s)) {
                                animFrame = (animFrame + 1) & 0x01;
                                lastAnimUpdate = now;
                            }
                            uint8_t idx = (animFrame & 0x01);
                            leds.setPixelColor(idx,        cA);
                            leds.setPixelColor(idx ^ 0x01, cB);
                            leds.show();
                            break;
                        }
                        case OXFP_AnimMode::RGBFade: {
                            if (now - lastAnimUpdate > (16 * (11 - s))) { animFrame++; lastAnimUpdate = now; }
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
                            if (now - lastAnimUpdate > (400 / s)) { animFrame ^= 1; lastAnimUpdate = now; }
                            const bool on = (animFrame & 0x01);
                            leds.setPixelColor(0, on ? cA : 0);
                            leds.setPixelColor(1, on ? cB : 0);
                            leds.show();
                            break;
                        }
                        case OXFP_AnimMode::Alternating: {
                            if (now - lastAnimUpdate > (350 / s)) { animFrame ^= 1; lastAnimUpdate = now; }
                            uint8_t idx = (animFrame & 0x01);
                            leds.setPixelColor(idx,        cA);
                            leds.setPixelColor(idx ^ 0x01, cB);
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
                        // 7: Plasma
                        case (OXFP_AnimMode)7: {
                            const float time = now * (0.0015f * s);
                            const int N = leds.numPixels();
                            const float denom = (N > 1) ? float(N - 1) : 1.f;
                            for (int i = 0; i < N; ++i) {
                                float x = float(i) / denom;
                                float v =
                                    sinf(6.28318f * (x * 1.00f + 0.20f) + time * 1.30f) +
                                    sinf(6.28318f * (x * 2.30f - 0.10f) - time * 1.70f) +
                                    sinf(6.28318f * (x * 0.70f + 0.33f) + time * 0.90f);
                                v = (v + 3.0f) / 6.0f;
                                OXFP_RGB blended = mixRGB(c.animColorA, c.animColorB, v);
                                leds.setPixelColor(i, rgbWithBrightness(blended, c.brightness));
                            }
                            leds.show();
                            break;
                        }
                        // 8: Heartbeat
                        case (OXFP_AnimMode)8: {
                            const uint16_t stepMs = 16;
                            if (now - lastAnimUpdate > stepMs) { lastAnimUpdate = now; animFrame++; }
                            const uint16_t T = (uint16_t)(64 - (s - 1) * 4);
                            uint8_t u = animFrame % T;
                            auto tri = [](int x, int center, int width)->float {
                                int d = abs(x - center);
                                if (d > width) return 0.f;
                                return 1.f - (float)d / (float)width;
                            };
                            float b = max(tri(u, 4, 4), tri(u, 16, 6));
                            b = max(b, 0.08f);
                            uint8_t bright = (uint8_t)(c.brightness * b);
                            leds.setPixelColor(0, rgbWithBrightness(c.animColorA, bright));
                            leds.setPixelColor(1, rgbWithBrightness(c.animColorB, bright));
                            leds.show();
                            break;
                        }
                        // 9: OpposedBreath
                        case (OXFP_AnimMode)9: {
                            if (now - lastAnimUpdate > 16) { lastAnimUpdate = now; animFrame++; }
                            float t = animFrame / (12.0f * (1.0f + (10 - s) * 0.1f));
                            float b0 = (sinf(t) + 1.f) * 0.5f;
                            float b1 = (sinf(t + PI) + 1.f) * 0.5f;
                            leds.setPixelColor(0, rgbWithBrightness(c.animColorA, (uint8_t)(c.brightness * b0)));
                            leds.setPixelColor(1, rgbWithBrightness(c.animColorB, (uint8_t)(c.brightness * b1)));
                            leds.show();
                            break;
                        }
                        // 10: Sparkle
                        case (OXFP_AnimMode)10: {
                            const uint16_t interval = (uint16_t)max(60, 260 - s * 20);
                            static uint8_t lit = 0; // 0=none, 1=left, 2=right
                            if (now - lastAnimUpdate > interval) {
                                lastAnimUpdate = now;
                                lit = (uint8_t)random(0, 3);
                            }
                            leds.setPixelColor(0, (lit == 1) ? cA : 0);
                            leds.setPixelColor(1, (lit == 2) ? cB : 0);
                            leds.show();
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

// Programmatic setter (used by UDP and future integrations)
void OXFP_config::setConfig(const OXFP_Config& newCfg, bool alsoSave) {
    // Cancel any active preview so the new config isn't overridden
    inPreview = false;
    previewTimer = 0;

    // Replace live config and apply immediately
    config = newCfg;
    if (alsoSave) {
        savePreferences();
    }
    applyConfig();
}


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
    // IMPORTANT: Do NOT register "/" here — root is owned by your Wi-Fi portal.

    // Serve UI from PROGMEM (no heap copy)
    server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", INDEX_HTML);
    });

    // Optional cache/CORS headers
    DefaultHeaders::Instance().addHeader("Cache-Control", "no-store");

    // ---- API: current config ----
    server.on("/api/ledconfig", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "application/json", configToJson(config));
    });

    // ---- API: preview (chunk-safe body aggregator) ----
    server.on("/api/ledpreview", HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        NULL,
        [](AsyncWebServerRequest *request, uint8_t* data, size_t len, size_t index, size_t total){
            if (index == 0) {
                request->_tempObject = new String();
                ((String*)request->_tempObject)->reserve(total);
            }
            String* body = (String*)request->_tempObject;
            body->concat((const char*)data, len);

            if (index + len == total) {
                StaticJsonDocument<384> doc;
                DeserializationError err = deserializeJson(doc, body->c_str());
                delete body; request->_tempObject = nullptr;

                if (err) { request->send(400, "text/plain", "Bad JSON"); return; }

                OXFP_Config tmp;
                tmp.mode = (OXFP_Mode)doc["mode"].as<uint8_t>();
                tmp.brightness = doc["brightness"].as<uint8_t>();

                auto arr = doc["greenColor"].as<JsonArray>();   tmp.greenColor = { arr[0], arr[1], arr[2] };
                arr = doc["redColor"].as<JsonArray>();          tmp.redColor   = { arr[0], arr[1], arr[2] };
                arr = doc["orangeColor"].as<JsonArray>();       tmp.orangeColor= { arr[0], arr[1], arr[2] };

                tmp.animMode = (OXFP_AnimMode)doc["animMode"].as<uint8_t>();
                auto arrA = doc["animColorA"].as<JsonArray>();  tmp.animColorA = { arrA[0], arrA[1], arrA[2] };
                auto arrB = doc["animColorB"].as<JsonArray>();  tmp.animColorB = { arrB[0], arrB[1], arrB[2] };
                tmp.animSpeed = doc["animSpeed"].as<uint8_t>();

                OXFP_config::preview(tmp);
                request->send(200, "text/plain", "Previewing");
            }
        }
    );

    // ---- API: save (chunk-safe body aggregator) ----
    server.on("/api/ledsave", HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        NULL,
        [](AsyncWebServerRequest *request, uint8_t* data, size_t len, size_t index, size_t total){
            if (index == 0) {
                request->_tempObject = new String();
                ((String*)request->_tempObject)->reserve(total);
            }
            String* body = (String*)request->_tempObject;
            body->concat((const char*)data, len);

            if (index + len == total) {
                StaticJsonDocument<384> doc;
                DeserializationError err = deserializeJson(doc, body->c_str());
                delete body; request->_tempObject = nullptr;

                if (err) { request->send(400, "text/plain", "Bad JSON"); return; }

                config.mode = (OXFP_Mode)doc["mode"].as<uint8_t>();
                config.brightness = doc["brightness"].as<uint8_t>();

                auto arr = doc["greenColor"].as<JsonArray>();   config.greenColor = { arr[0], arr[1], arr[2] };
                arr = doc["redColor"].as<JsonArray>();          config.redColor   = { arr[0], arr[1], arr[2] };
                arr = doc["orangeColor"].as<JsonArray>();       config.orangeColor= { arr[0], arr[1], arr[2] };

                config.animMode = (OXFP_AnimMode)doc["animMode"].as<uint8_t>();
                auto arrA = doc["animColorA"].as<JsonArray>();  config.animColorA = { arrA[0], arrA[1], arrA[2] };
                auto arrB = doc["animColorB"].as<JsonArray>();  config.animColorB = { arrB[0], arrB[1], arrB[2] };
                config.animSpeed = doc["animSpeed"].as<uint8_t>();

                OXFP_config::savePreferences();
                inPreview = false;
                OXFP_config::applyConfig();
                request->send(200, "text/plain", "Config saved!");
            }
        }
    );

    // ---- API: reset ----
    server.on("/api/ledreset", HTTP_POST, [](AsyncWebServerRequest *request){
        OXFP_config::resetPreferences();
        inPreview = false;
        OXFP_config::applyConfig();
        request->send(200, "text/plain", "Reset to defaults.");
    });

    // NOTE: no "/" handler and no onNotFound — to avoid clobbering your Wi-Fi portal.
}
