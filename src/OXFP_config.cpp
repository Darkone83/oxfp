#include "oxfp_config.h"
#include "OXFP_orig.h"
#include <Preferences.h>
#include <ArduinoJson.h>
#include <math.h>

// Access the shared NeoPixel object
extern Adafruit_NeoPixel leds;

// --- HSV to RGB Helper ---
static uint32_t hsv2rgb(float h, float s, float v) {
    float r, g, b;
    int i = int(h * 6);
    float f = h * 6 - i;
    float p = v * (1 - s);
    float q = v * (1 - f * s);
    float t = v * (1 - (1 - f) * s);
    switch(i % 6){
        case 0: r = v, g = t, b = p; break;
        case 1: r = q, g = v, b = p; break;
        case 2: r = p, g = v, b = t; break;
        case 3: r = p, g = q, b = v; break;
        case 4: r = t, g = p, b = v; break;
        case 5: r = v, g = p, b = q; break;
    }
    return leds.Color((uint8_t)(r*255), (uint8_t)(g*255), (uint8_t)(b*255));
}

// --- Color Palette (RRGGBB hex, 32 entries) ---
#define COLOR_COUNT 32
static const uint32_t color_palette[COLOR_COUNT] = {
    0x000000, 0xFFFFFF, 0xFF0000, 0x00FF00, 0x0000FF, 0xFFFF00, 0x00FFFF, 0xFF00FF,
    0x800000, 0x008000, 0x000080, 0x808000, 0x008080, 0x800080, 0x808080, 0xC0C0C0,
    0xFFA500, 0xA52A2A, 0x008B8B, 0xB8860B, 0x006400, 0x8B008B, 0x556B2F, 0xFF69B4,
    0x4B0082, 0xB22222, 0x228B22, 0xDAA520, 0x20B2AA, 0x32CD32, 0x4682B4, 0x9ACD32
};

static const char* animation_names[] = {
    "Pulse", "Fade", "Rainbow", "Dual Rainbow", "Color Chase", "Sparkle"
};

// --- HTML Fragments: File scope ---
static const char page_head[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html><head>
<title>OXFP Config</title>
<meta name="viewport" content="width=400">
<style>
body { background: #111; color: #eee; font-family: sans-serif; }
h2 { color: #3c8d1a; }
.colgrid { display: grid; grid-template-columns: repeat(8, 28px); gap: 4px; }
.colbtn { width: 28px; height: 28px; border: 2px solid #333; border-radius: 6px; cursor: pointer; }
.sel { border-color: #fff !important; }
input[type=range] { width:180px; }
</style>
</head>
<body>
<h2>OXFP Front Panel LED Config</h2>
<b>Brightness:</b>
<input type="range" min="8" max="255" value="255" id="bright" style="width:180px;">
<span id="bright_val">255</span>
<br><br>
Mode: 
<select id="mode">
  <option value="0">Static</option>
  <option value="1">Animation</option>
</select><br><br>
<div id="static_cfg">
  <b>Static Colors</b><br>
  <b>Right LED:</b><br>
  Red <div class="colgrid" id="rr"></div>
  Green <div class="colgrid" id="rg"></div>
  Orange <div class="colgrid" id="ro"></div>
  <br>
  <b>Left LED:</b><br>
  Red <div class="colgrid" id="lr"></div>
  Green <div class="colgrid" id="lg"></div>
  Orange <div class="colgrid" id="lo"></div>
</div>
<div id="anim_cfg" style="display:none">
  <b>Animation:</b>
  <select id="anim_id">
)rawliteral";

static const char page_mid[] PROGMEM = R"rawliteral(
</select><br>
  <div id="anim_colors"></div>
</div>
<br>
<button onclick="saveCfg()">Save</button>
<span id="stat"></span>
<script>
const palette = [
'#000','#fff','#f00','#0f0','#00f','#ff0','#0ff','#f0f','#800','#080','#008','#880','#088','#808','#888','#ccc',
'#fa0','#a52','#088','#b86','#064','#808','#556','#f6b','#4b0','#b22','#282','#da2','#2ba','#2c2','#468','#9c2'
];
let cur = {};
const animColorNeeds = [
    [1,0], [1,0], [0,0], [0,0], [1,1], [1,1]
];
function fillGrid(id, sel) {
  let div = document.getElementById(id);
  div.innerHTML = '';
  for(let i=0;i<palette.length;i++) {
    let b = document.createElement('div');
    b.className = "colbtn" + (sel==i?" sel":"");
    b.style.background = palette[i];
    b.onclick = ()=>{ cur[id]=i; fillGrid(id,i); };
    div.appendChild(b);
  }
}
function showAnimColorPickers(anim_id) {
    let html = "";
    if (animColorNeeds[anim_id][0]) {
        html += 'Main Color <div class="colgrid" id="animcol_a"></div>';
    }
    if (animColorNeeds[anim_id][1]) {
        html += 'Second Color <div class="colgrid" id="animcol_b"></div>';
    }
    document.getElementById('anim_colors').innerHTML = html;
    if (animColorNeeds[anim_id][0]) fillGrid('animcol_a', cur.animcol_a);
    if (animColorNeeds[anim_id][1]) fillGrid('animcol_b', cur.animcol_b);
}
function updateUI() {
  document.getElementById('mode').value = cur.mode;
  fillGrid('rr', cur.rr); fillGrid('rg', cur.rg); fillGrid('ro', cur.ro);
  fillGrid('lr', cur.lr); fillGrid('lg', cur.lg); fillGrid('lo', cur.lo);
  document.getElementById('bright').value = cur.brightness || 255;
  document.getElementById('bright_val').innerText = cur.brightness || 255;
  document.getElementById('static_cfg').style.display = cur.mode==0?"block":"none";
  document.getElementById('anim_cfg').style.display = cur.mode==1?"block":"none";
  if (cur.mode==1) showAnimColorPickers(cur.anim_id);
}
function loadCfg() {
  fetch('/config/settings').then(r=>r.json()).then(js=>{
    cur = js;
    updateUI();
  });
}
function saveCfg() {
  cur.mode = +document.getElementById('mode').value;
  cur.anim_id = +document.getElementById('anim_id').value;
  cur.brightness = +document.getElementById('bright').value;
  if (document.getElementById('animcol_a')) cur.animcol_a = cur.animcol_a || 0;
  if (document.getElementById('animcol_b')) cur.animcol_b = cur.animcol_b || 0;
  fetch('/config/settings', {
    method: 'POST', headers: {'Content-Type':'application/json'},
    body: JSON.stringify(cur)
  }).then(r=>r.text()).then(t=>{
    document.getElementById('stat').innerText = "Saved!";
  });
}
document.getElementById('bright').oninput = function() {
    document.getElementById('bright_val').innerText = this.value;
    cur.brightness = +this.value;
};
document.getElementById('mode').onchange = updateUI;
document.getElementById('anim_id').onchange = ()=>{
    cur.anim_id = +document.getElementById('anim_id').value;
    showAnimColorPickers(cur.anim_id);
};
window.onload = loadCfg;
</script>
</body>
</html>
)rawliteral";

// --- Settings ---
static OXFP_config::Settings settings;
static Preferences prefs;

// --- Save/Load Settings ---
void saveSettings() {
    prefs.begin("oxfp_cfg", false);
    prefs.putBytes("settings", &settings, sizeof(settings));
    prefs.end();
}
void loadSettings() {
    prefs.begin("oxfp_cfg", true);
    if (prefs.isKey("settings")) {
        prefs.getBytes("settings", &settings, sizeof(settings));
        if (settings.brightness < 8 || settings.brightness > 255) settings.brightness = 255;
    } else {
        settings = {
            OXFP_config::Static,
            2, 3, 16,   // right: red, green, orange (palette idx)
            2, 3, 16,   // left:  red, green, orange
            0, 3, 4,    // animation_id, color_a, color_b
            255         // brightness
        };
    }
    prefs.end();
}

namespace OXFP_config {

static bool handlerActive = false;
static void animationHandler();

void setSettings(const Settings& s) {
    settings = s;
    saveSettings();
}
const Settings& getSettings() { return settings; }

bool isActive() {
    return (settings.mode == Animation);
}

void begin(AsyncWebServer& server) {
    loadSettings();

    // --- Web UI ---
    server.on("/config", HTTP_GET, [](AsyncWebServerRequest* req){
        String animOpts;
        for (int i = 0; i < AnimationCount; ++i) {
            animOpts += "<option value='" + String(i) + "'";
            if (i == settings.animation_id) animOpts += " selected";
            animOpts += ">" + String(animation_names[i]) + "</option>\n";
        }
        String page = String(page_head) + animOpts + String(page_mid);
        req->send(200, "text/html", page);
    });

    server.on("/config/colors", HTTP_GET, [](AsyncWebServerRequest* req){
        String js = "[";
        for (int i = 0; i < COLOR_COUNT; ++i) {
            if (i) js += ",";
            char buf[8];
            sprintf(buf, "\"#%06x\"", color_palette[i]);
            js += buf;
        }
        js += "]";
        req->send(200, "application/json", js);
    });

    server.on("/config/settings", HTTP_GET, [](AsyncWebServerRequest* req){
        DynamicJsonDocument doc(512);
        doc["mode"] = settings.mode;
        doc["rr"] = settings.static_right_red;
        doc["rg"] = settings.static_right_green;
        doc["ro"] = settings.static_right_orange;
        doc["lr"] = settings.static_left_red;
        doc["lg"] = settings.static_left_green;
        doc["lo"] = settings.static_left_orange;
        doc["anim_id"] = settings.animation_id;
        doc["animcol_a"] = settings.animation_color_a;
        doc["animcol_b"] = settings.animation_color_b;
        doc["brightness"] = settings.brightness;
        String out;
        serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    server.on("/config/settings", HTTP_POST, [](AsyncWebServerRequest* req){
        if (req->hasArg("plain")) {
            String body = req->arg("plain");
            DynamicJsonDocument doc(512);
            deserializeJson(doc, body);
            settings.mode = (OXFP_config::Mode)doc["mode"].as<uint8_t>();
            settings.static_right_red = doc["rr"];
            settings.static_right_green = doc["rg"];
            settings.static_right_orange = doc["ro"];
            settings.static_left_red = doc["lr"];
            settings.static_left_green = doc["lg"];
            settings.static_left_orange = doc["lo"];
            settings.animation_id = doc["anim_id"];
            settings.animation_color_a = doc["animcol_a"];
            settings.animation_color_b = doc["animcol_b"];
            settings.brightness = doc["brightness"] | 255;
            saveSettings();
            req->send(200, "text/plain", "OK");
        } else {
            req->send(400, "text/plain", "Missing body");
        }
    });
}

// --- Animation handler: runs when config mode is active ---
static void animationHandler() {
    static uint32_t last_anim_ms = 0;
    static int anim_phase = 0;
    uint32_t now = millis();

    leds.setBrightness(settings.brightness);

    switch (settings.animation_id) {
        case Pulse: {
            float bright = (sin(now / 400.0f) + 1.0f) * 0.5f; // 0..1
            uint32_t c = color_palette[settings.animation_color_a];
            uint8_t r = ((c >> 16) & 0xFF) * bright;
            uint8_t g = ((c >> 8) & 0xFF) * bright;
            uint8_t b = (c & 0xFF) * bright;
            leds.setPixelColor(0, leds.Color(r, g, b));
            leds.setPixelColor(1, leds.Color(r, g, b));
            break;
        }
        case Fade: {
            float phase = fmod(now / 1000.0f, 1.0f);
            float bright = phase < 0.5f ? phase * 2 : (1.0f - phase) * 2;
            uint32_t c = color_palette[settings.animation_color_a];
            uint8_t r = ((c >> 16) & 0xFF) * bright;
            uint8_t g = ((c >> 8) & 0xFF) * bright;
            uint8_t b = (c & 0xFF) * bright;
            leds.setPixelColor(0, leds.Color(r, g, b));
            leds.setPixelColor(1, leds.Color(r, g, b));
            break;
        }
        case Rainbow: {
            float h = fmod(now / 6000.0f, 1.0f);
            uint32_t rgb = hsv2rgb(h, 1.0, 1.0);
            leds.setPixelColor(0, rgb);
            leds.setPixelColor(1, rgb);
            break;
        }
        case DualRainbow: {
            float h0 = fmod(now / 6000.0f, 1.0f);
            float h1 = fmod((now / 6000.0f) + 0.5f, 1.0f);
            leds.setPixelColor(0, hsv2rgb(h0, 1.0, 1.0));
            leds.setPixelColor(1, hsv2rgb(h1, 1.0, 1.0));
            break;
        }
        case ColorChase: {
            if (now - last_anim_ms > 300) {
                last_anim_ms = now;
                anim_phase = !anim_phase;
            }
            uint32_t ca = color_palette[settings.animation_color_a];
            uint32_t cb = color_palette[settings.animation_color_b];
            leds.setPixelColor(anim_phase ? 0 : 1, ca);
            leds.setPixelColor(anim_phase ? 1 : 0, cb);
            break;
        }
        case Sparkle: {
            uint32_t ca = color_palette[settings.animation_color_a];
            uint32_t cb = color_palette[settings.animation_color_b];
            leds.setPixelColor(0, (random(3)==0) ? ca : (random(3)==1 ? cb : 0));
            leds.setPixelColor(1, (random(3)==0) ? ca : (random(3)==1 ? cb : 0));
            break;
        }
    }
    leds.show();
}

void loop() {
    // Activate/deactivate animation handler as needed
    bool shouldBeActive = (settings.mode == Animation);
    if (shouldBeActive && !handlerActive) {
        OXFP_orig::setCustomHandler(animationHandler);
        handlerActive = true;
    } else if (!shouldBeActive && handlerActive) {
        OXFP_orig::setCustomHandler(nullptr); // back to stock logic
        handlerActive = false;
    }
}

} // namespace OXFP_config
