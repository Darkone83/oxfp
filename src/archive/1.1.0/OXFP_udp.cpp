#include "OXFP_udp.h"
#include <ArduinoJson.h>
#include <IPAddress.h>
#include <WiFi.h>

// We use the public OXFP_config API only:
//   - getConfig()
//   - preview(const OXFP_Config&)
//   - applyConfig()
//   - savePreferences()
//   - resetPreferences()
// NOTE: There is no public setter to modify the live config in RAM.
//       Therefore ops that need a persistent change ("set"/"save")
//       are implemented as PREVIEW for now (temporary). We add clear
//       TODOs where a tiny OXFP_config setter would enable persistence.

namespace {
  WiFiUDP udp;
  bool g_enabled = false;
  uint16_t g_port = 32123;

  static String localIpString() {
  if (WiFi.isConnected()) return WiFi.localIP().toString();
  return String("0.0.0.0");
  }

  // Rx buffer (fits typical JSON control packets)
  constexpr size_t kRxMax = 1024;
  char rxBuf[kRxMax];

  // Identify (visual) state
  bool identActive = false;
  unsigned long identUntil = 0;

  // Helpers ------------- //
  void startUdp() {
    if (!g_enabled) return;
    udp.stop();
    udp.begin(g_port);
  }
  void stopUdp() {
    udp.stop();
  }

  // Build {"ok":..., "op":..., "seq":..., ...} into out String
  void makeBaseReply(JsonDocument& doc, const char* op, long seq, bool ok, const char* err = nullptr) {
    doc.clear();
    doc["ok"] = ok;
    if (op)  doc["op"]  = op;
    if (seq) doc["seq"] = seq;
    doc["ver"] = "udp-1.0";
    if (!ok && err) doc["err"] = err;
  }

  // Serialize JSON reply and send back to requester
  void sendReply(const IPAddress& ip, uint16_t port, JsonDocument& reply) {
    char out[768];
    size_t n = serializeJson(reply, out, sizeof(out));
    udp.beginPacket(ip, port);
    udp.write((const uint8_t*)out, n);
    udp.endPacket();
  }

  // Convert current config to JSON object (into doc["config"])
  void configToJson(const OXFP_Config& c, JsonDocument& doc) {
    JsonObject cfg = doc.createNestedObject("config");
    cfg["mode"]       = (uint8_t)c.mode;
    cfg["brightness"] = c.brightness;

    JsonArray g = cfg.createNestedArray("greenColor");
    g.add(c.greenColor.r); g.add(c.greenColor.g); g.add(c.greenColor.b);

    JsonArray r = cfg.createNestedArray("redColor");
    r.add(c.redColor.r); r.add(c.redColor.g); r.add(c.redColor.b);

    JsonArray o = cfg.createNestedArray("orangeColor");
    o.add(c.orangeColor.r); o.add(c.orangeColor.g); o.add(c.orangeColor.b);

    cfg["animMode"] = (uint8_t)c.animMode;

    JsonArray aA = cfg.createNestedArray("animColorA");
    aA.add(c.animColorA.r); aA.add(c.animColorA.g); aA.add(c.animColorA.b);

    JsonArray aB = cfg.createNestedArray("animColorB");
    aB.add(c.animColorB.r); aB.add(c.animColorB.g); aB.add(c.animColorB.b);

    cfg["animSpeed"] = c.animSpeed;
  }

  // Safe array->[r,g,b]
  bool parseRGB(JsonVariant v, OXFP_RGB& out) {
    if (!v.is<JsonArray>()) return false;
    JsonArray a = v.as<JsonArray>();
    if (a.size() != 3) return false;
    out.r = (uint8_t)a[0].as<int>();
    out.g = (uint8_t)a[1].as<int>();
    out.b = (uint8_t)a[2].as<int>();
    return true;
  }

  // Apply partial fields from obj into cfg
  void applyPartialConfig(JsonObject obj, OXFP_Config& cfg) {
    if (obj.containsKey("mode"))       cfg.mode = (OXFP_Mode)obj["mode"].as<uint8_t>();
    if (obj.containsKey("brightness")) cfg.brightness = (uint8_t)obj["brightness"].as<int>();

    if (obj.containsKey("greenColor"))  { OXFP_RGB t; if (parseRGB(obj["greenColor"], t)) cfg.greenColor = t; }
    if (obj.containsKey("redColor"))    { OXFP_RGB t; if (parseRGB(obj["redColor"], t))   cfg.redColor = t; }
    if (obj.containsKey("orangeColor")) { OXFP_RGB t; if (parseRGB(obj["orangeColor"], t))cfg.orangeColor = t; }

    if (obj.containsKey("animMode"))    cfg.animMode = (OXFP_AnimMode)obj["animMode"].as<uint8_t>();
    if (obj.containsKey("animColorA"))  { OXFP_RGB t; if (parseRGB(obj["animColorA"], t)) cfg.animColorA = t; }
    if (obj.containsKey("animColorB"))  { OXFP_RGB t; if (parseRGB(obj["animColorB"], t)) cfg.animColorB = t; }
    if (obj.containsKey("animSpeed"))   cfg.animSpeed = (uint8_t)obj["animSpeed"].as<int>();
  }

  // Start identify effect: brief white blink while we own the renderer, then restore
  void beginIdentify(unsigned long ms) {
    identActive = true;
    identUntil = millis() + (ms ? ms : 1500);
    // Take over renderer during identify
    OXFP_orig::ledCustomOverride([]{
      // Simple 4Hz blink white
      unsigned long t = millis();
      bool on = ((t / 125) & 1) == 0;
      uint32_t w = leds.Color(255,255,255);
      leds.setPixelColor(0, on ? w : 0);
      leds.setPixelColor(1, on ? w : 0);
      leds.show();
    });
  }

  void endIdentifyIfExpired() {
    if (identActive && millis() > identUntil) {
      identActive = false;
      // Give control back to normal renderer
      OXFP_config::applyConfig();
    }
  }

  // Handle a single JSON request and write reply into 'reply'
  void handleJson(JsonDocument& req, JsonDocument& reply, const IPAddress& rip, uint16_t rport) {
    const char* op = req["op"] | "";
    long seq = req["seq"] | 0;

    // Common base
    makeBaseReply(reply, op, seq, true);

    // ---- ping ----
    if (!strcmp(op, "ping")) {
      reply["name"] = "OXFP";
      reply["ip"]   = localIpString();   // was: WiFi.localIP().toString()
      reply["port"] = g_port;
      return;
    }

    

    // If disabled, only allow ping
    if (!g_enabled) {
      makeBaseReply(reply, op, seq, false, "disabled");
      return;
    }

    // ---- get ----
    if (!strcmp(op, "get")) {
      const OXFP_Config& c = OXFP_config::getConfig();
      configToJson(c, reply);
      return;
    }

    // ---- preview ---- (partial fields allowed; optional previewMs)
    if (!strcmp(op, "preview")) {
      OXFP_Config tmp = OXFP_config::getConfig();
      applyPartialConfig(req.as<JsonObject>(), tmp);
      OXFP_config::preview(tmp);
      // Note: preview lifetime is governed by OXFP_config (currently 8s)
      reply["status"] = "previewing";
      return;
    }

    // ---- mode ---- (quick switch)
    if (!strcmp(op, "mode")) {
      if (!req.containsKey("mode")) {
        makeBaseReply(reply, op, seq, false, "missing_mode");
        return;
      }
      OXFP_Config tmp = OXFP_config::getConfig();
      tmp.mode = (OXFP_Mode)req["mode"].as<uint8_t>();
      OXFP_config::preview(tmp);     // immediate feedback
      OXFP_config::applyConfig();    // and render via normal path
      reply["status"] = "applied";
      return;
    }

    // ---- identify ---- (visual confirmation)
    if (!strcmp(op, "identify")) {
      unsigned long ms = req["ms"] | 1500;
      beginIdentify(ms);
      reply["status"] = "identifying";
      reply["ms"] = ms;
      return;
    }

    // ---- reset ---- (factory defaults)
    if (!strcmp(op, "reset")) {
      OXFP_config::resetPreferences();
      OXFP_config::applyConfig();
      reply["status"] = "defaults_applied";
      return;
    }

    // ---- save ---- (TODO: requires setter in OXFP_config)
    if (!strcmp(op, "save")) {
      // Currently, we cannot push pending changes from UDP into the internal
      // config because OXFP_config exposes no setter. If you want UDP 'save'
      // to persist the last 'preview' values, expose a:
      //   void setConfig(const OXFP_Config& newCfg, bool alsoSave);
      // For now, we just save the existing config snapshot.
      OXFP_config::savePreferences();
      reply["status"] = "saved_current_config";
      return;
    }

    // ---- set ---- (partial live update, RAM only) — implemented as PREVIEW for now
    if (!strcmp(op, "set")) {
      OXFP_Config tmp = OXFP_config::getConfig();
      applyPartialConfig(req.as<JsonObject>(), tmp);
      // As we lack a public setter, treat as a long-ish preview.
      // NOTE: OXFP_config auto-exits preview after 8s in current build.
      OXFP_config::preview(tmp);
      reply["status"] = "applied_temporarily";
      reply["note"]   = "expose OXFP_config::setConfig(...) to persist without preview";
      return;
    }

    // Unknown op
    makeBaseReply(reply, op, seq, false, "unknown_op");
  }

  // Read one UDP packet; return true if handled
  bool readOne() {
    int packetSize = udp.parsePacket();
    if (packetSize <= 0) return false;

    int n = (packetSize > (int)kRxMax) ? kRxMax : packetSize;
    int got = udp.read(rxBuf, n);
    if (got <= 0) return false;

    // Heuristic: if first non-space is '{' -> JSON
    int i = 0;
    while (i < got && isspace((unsigned char)rxBuf[i])) ++i;
    bool isJson = (i < got && rxBuf[i] == '{');

    StaticJsonDocument<896> req;
    StaticJsonDocument<768> reply;

    if (!isJson) {
      // Not JSON: treat as ping
      makeBaseReply(reply, "ping", 0, true);
      reply["name"] = "OXFP";
      reply["ip"]   = localIpString();   // was: WiFi.localIP().toString()
      reply["port"] = g_port;
      sendReply(udp.remoteIP(), udp.remotePort(), reply);
      return true;
    }

    DeserializationError err = deserializeJson(req, rxBuf, got);
    if (err) {
      makeBaseReply(reply, "unknown", 0, false, "bad_json");
      sendReply(udp.remoteIP(), udp.remotePort(), reply);
      return true;
    }

    handleJson(req, reply, udp.remoteIP(), udp.remotePort());
    sendReply(udp.remoteIP(), udp.remotePort(), reply);
    return true;
  }

} // namespace

namespace OXFP_udp {

  void begin(uint16_t port, bool enable) {
    g_port = port;
    g_enabled = enable;
    if (g_enabled) startUdp();
  }

  void loop() {
    // Run identify state if active (non-blocking)
    endIdentifyIfExpired();

    if (!g_enabled) return;
    // Drain all pending packets quickly
    for (int i = 0; i < 4; ++i) {
      if (!readOne()) break;
    }
  }

  void setEnabled(bool en) {
    if (en == g_enabled) return;
    g_enabled = en;
    if (g_enabled) startUdp(); else stopUdp();
  }

  void setPort(uint16_t port) {
    if (port == 0 || port == g_port) return;
    g_port = port;
    if (g_enabled) startUdp();
  }

  bool enabled() { return g_enabled; }
  uint16_t port() { return g_port; }

} // namespace OXFP_udp
