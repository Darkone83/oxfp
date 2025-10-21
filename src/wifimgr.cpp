#include "wifimgr.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <DNSServer.h>
#include "led_stat.h"
#include <vector>
#include "esp_wifi.h"
#include <Update.h>

static AsyncWebServer server(80);

static const char OTA_PAGE[] PROGMEM = R"html(
<!DOCTYPE html><html><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>OXFP OTA Update</title>
<style>
body{background:#111;color:#EEE;font-family:sans-serif;margin:24px}
.card{max-width:420px;margin:auto;background:#1a1a1a;border:1px solid #333;border-radius:10px;padding:14px}
button,input[type=submit]{background:#299a2c;color:#fff;border:0;border-radius:6px;padding:.6em 1em;cursor:pointer}
input[type=file]{width:100%;margin:.6em 0}
.row{display:flex;gap:.5em}.row>*{flex:1}
.danger{background:#a22}
small{opacity:.75}
a{color:#8acfff}
progress{width:100%;height:16px;appearance:none;-webkit-appearance:none;background:#222;border:1px solid #444;border-radius:6px}
progress::-webkit-progress-bar{background:#222}
progress::-webkit-progress-value{background:linear-gradient(90deg,#52d273,#5aa9ff)}
progress::-moz-progress-bar{background:linear-gradient(90deg,#52d273,#5aa9ff)}
#barwrap{margin:.6em 0}
#ok{color:#5fd35f}
#err{color:#ff6b6b}
</style></head><body>
<div class="card">
  <h2>OTA Update</h2>
  <form id="f">
    <input type="file" name="firmware" id="fw" accept=".bin,.bin.gz" required>
    <div id="barwrap" style="display:none">
      <progress id="pb" max="100" value="0"></progress>
      <div id="pct">0%</div>
    </div>
    <div class="row">
      <input type="submit" value="Upload & Flash">
      <button type="button" onclick="reboot()" class="danger">Reboot</button>
    </div>
  </form>
  <div id="s"></div>
  <p><a href="/">&larr; Back to Setup</a></p>
</div>
<script>
const s = document.getElementById('s');
const pb = document.getElementById('pb');
const pct = document.getElementById('pct');
const barwrap = document.getElementById('barwrap');

function reboot(){
  s.textContent = 'Rebooting...';
  fetch('/reboot',{method:'POST'}).catch(()=>0);
  setTimeout(()=>location.reload(), 2500);
}

document.getElementById('f').addEventListener('submit', (e)=>{
  e.preventDefault();
  const file = document.getElementById('fw').files[0];
  if(!file){ s.innerHTML = '<span id="err">Choose a file first.</span>'; return; }

  barwrap.style.display='block'; pb.value=0; pct.textContent='0%';
  s.textContent='Uploading...';

  const fd = new FormData(); fd.append('firmware', file);
  const xhr = new XMLHttpRequest();
  xhr.open('POST','/update');

  xhr.upload.onprogress = (ev)=>{
    if(ev.lengthComputable){
      const p = Math.round((ev.loaded/ev.total)*100);
      pb.value = p; pct.textContent = p + '%';
    }
  };

  xhr.onload = ()=>{
    try {
      const j = JSON.parse(xhr.responseText||'{}');
      if (xhr.status === 200 && j.ok) {
        s.innerHTML = '<span id="ok">Update uploaded successfully ('+(j.bytes||0)+' bytes). Rebooting in 2s...</span>';
        setTimeout(()=>reboot(), 2000);
      } else {
        s.innerHTML = '<span id="err">Update failed.</span>';
      }
    } catch(e){
      if (xhr.status === 200) {
        s.innerHTML = '<span id="ok">Update uploaded. Rebooting in 2s...</span>';
        setTimeout(()=>reboot(), 2000);
      } else {
        s.innerHTML = '<span id="err">Upload error.</span>';
      }
    }
  };

  xhr.onerror = ()=>{ s.innerHTML = '<span id="err">Network error.</span>'; };

  xhr.send(fd);
});
</script>
</body></html>
)html";

namespace WiFiMgr {

static String ssid, password;
static Preferences prefs;
static DNSServer dnsServer;
static std::vector<String> lastScanResults;

enum class State { IDLE, CONNECTING, CONNECTED, PORTAL };
static State state = State::PORTAL;

static int connectAttempts = 0;
static const int maxAttempts = 3;
static unsigned long connectStartTime = 0;
static const unsigned long connectTimeout = 30000; // 30 seconds - let WiFi.begin() do its thing
static unsigned long lastConnCheck = 0;
static const unsigned long connCheckInterval = 10000;
static unsigned long lastRetryTime = 0;
static const unsigned long retryInterval = 300000; // 5 minutes between retry cycles

AsyncWebServer& getServer() {
    return server;
}

static void setAPConfig() {
    WiFi.softAPConfig(
        IPAddress(192, 168, 4, 1),
        IPAddress(192, 168, 4, 1),
        IPAddress(255, 255, 255, 0)
    );
}

void loadCreds() {
    prefs.begin("wifi", true);
    ssid = prefs.getString("ssid", "");
    password = prefs.getString("pass", "");
    prefs.end();
    Serial.printf("[WiFiMgr] Loaded credentials - SSID: '%s' (%d chars)\n", 
                  ssid.c_str(), ssid.length());
}

bool saveCreds(const String& s, const String& p) {
    if (!prefs.begin("wifi", false)) {
        Serial.println("[WiFiMgr] ERROR: Cannot open preferences namespace");
        return false;
    }
    
    size_t ssidWritten = prefs.putString("ssid", s);
    size_t passWritten = prefs.putString("pass", p);
    prefs.end();
    
    if (ssidWritten == 0) {
        Serial.println("[WiFiMgr] ERROR: Failed to write SSID to preferences");
        return false;
    }
    
    Serial.printf("[WiFiMgr] Credentials saved - SSID: %d bytes, Pass: %d bytes\n", 
                  ssidWritten, passWritten);
    return true;
}

void clearCreds() {
    prefs.begin("wifi", false);
    prefs.remove("ssid");
    prefs.remove("pass");
    prefs.end();
}

void startPortal() {
    WiFi.disconnect(true);
    delay(100);
    setAPConfig();
    WiFi.mode(WIFI_AP_STA);
    delay(100);

    bool apok = WiFi.softAP("OXFP Setup", "", 6, 0);
    esp_wifi_set_max_tx_power(80); // 80 = 20 dBm (units are 0.25 dBm, so 80 * 0.25 = 20 dBm max)
    LedStat::setStatus(LedStatus::Portal);
    Serial.printf("[WiFiMgr] softAP result: %d, IP: %s\n", apok, WiFi.softAPIP().toString().c_str());
    delay(200);

    IPAddress apIP = WiFi.softAPIP();
    dnsServer.start(53, "*", apIP);

    server.reset();

    // ===== OTA ROUTES =====
    server.on("/fw", HTTP_GET, [](AsyncWebServerRequest* req){
        String v = String("OXFP/") + String(__DATE__) + " " + String(__TIME__);
        req->send(200, "text/plain", v);
    });

    server.on("/update", HTTP_GET, [](AsyncWebServerRequest* req){
        req->send_P(200, "text/html", OTA_PAGE);
    });

    server.on(
        "/update",
        HTTP_POST,
        [](AsyncWebServerRequest* request){
            bool ok = !Update.hasError();
            String msg;
            if (ok) {
                msg = "{\"ok\":true,\"bytes\":" + String(Update.progress()) + "}";
                request->send(200, "application/json", msg);
                Serial.println("[OTA] Update uploaded OK; client will reboot device.");
            } else {
                msg = "{\"ok\":false}";
                request->send(500, "application/json", msg);
                Serial.println("[OTA] Update failed.");
            }
        },
        [](AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final){
            if (index == 0) {
                Serial.printf("[OTA] Starting: %s\n", filename.c_str());
                LedStat::setStatus(LedStatus::Booting);
                if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                    Update.printError(Serial);
                }
            }
            if (len) {
                if (Update.write(data, len) != len) {
                    Update.printError(Serial);
                }
            }
            if (final) {
                if (!Update.end(true)) {
                    Update.printError(Serial);
                } else {
                    Serial.printf("[OTA] Finished: %u bytes\n", (unsigned)(index + len));
                }
            }
        }
    );

    server.on("/reboot", HTTP_POST, [](AsyncWebServerRequest* req){
        req->send(200, "text/plain", "Rebooting...");
        Serial.println("[OTA] Reboot requested");
        LedStat::setStatus(LedStatus::Booting);
        delay(300);
        ESP.restart();
    });

    // ===== MAIN SETUP PAGE WITH CONFIG BUTTON =====
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        String page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>OXFP WiFi Setup</title>
    <meta name="viewport" content="width=340,initial-scale=1">
    <style>
        :root{--bg:#0e0f12;--panel:#171a1f;--text:#e8ebf1;--accent:#52d273;--warn:#ffaf40;--danger:#ff5964;--info:#5aa9ff;--border:#242833;--muted:#99a2b2}
        *{box-sizing:border-box}
        body{margin:0;background:var(--bg);color:var(--text);font-family:system-ui,sans-serif;padding:16px}
        .container{max-width:360px;margin:0 auto;background:var(--panel);padding:24px;border-radius:12px;box-shadow:0 8px 24px rgba(0,0,0,.35);border:1px solid var(--border)}
        .header{text-align:center;margin-bottom:20px}
        .header h1{margin:0 0 8px;font-size:24px;font-weight:600}
        .header .subtitle{color:var(--muted);font-size:14px}
        input,select,button{width:100%;padding:12px;margin:8px 0;border-radius:8px;border:1px solid var(--border);background:#11151a;color:var(--text);font-size:15px}
        button{cursor:pointer;font-weight:600;transition:all .2s}
        .btn-primary{background:var(--accent);border-color:transparent;color:#08120a}
        .btn-primary:hover{opacity:.9;transform:translateY(-1px)}
        .btn-info{background:var(--info);border-color:transparent;color:#fff}
        .btn-info:hover{opacity:.9}
        .btn-danger{background:var(--danger);border-color:transparent;color:#fff}
        .btn-danger:hover{opacity:.9}
        .status{margin-top:16px;padding:12px;background:#11151a;border-radius:6px;font-size:14px;color:var(--muted);text-align:center}
        .status.connected{color:var(--accent);border:1px solid var(--accent)}
        .status.error{color:var(--danger);border:1px solid var(--danger)}
        label{display:block;margin:12px 0 4px;color:var(--muted);font-size:13px;font-weight:500}
        .btn-group{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-top:8px}
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>OXFP Setup</h1>
            <div class="subtitle">WiFi Configuration</div>
        </div>
        <form id="wifiForm">
            <label>Available Networks</label>
            <select id="ssidDropdown">
                <option value="">Scanning...</option>
            </select>
            <label>Network Name (SSID)</label>
            <input type="text" id="ssid" placeholder="Enter SSID manually if needed">
            <label>Password</label>
            <input type="password" id="pass" placeholder="WiFi Password">
            <button type="button" onclick="save()" class="btn-primary">Connect &amp; Save</button>
            <div class="btn-group">
                <button type="button" onclick="window.location='/config'" class="btn-info">LED Config</button>
                <button type="button" onclick="window.location='/update'" class="btn-info">OTA Update</button>
            </div>
            <button type="button" onclick="forget()" class="btn-danger">Forget WiFi</button>
        </form>
        <div class="status" id="status">Loading status...</div>
    </div>
    <script>
        function updateStatus(){
            fetch('/status').then(r=>r.text()).then(t=>{
                let el=document.getElementById('status');
                el.innerText=t;
                el.className='status';
                if(t.includes('Connected'))el.className='status connected';
                else if(t.includes('Failed'))el.className='status error';
            });
        }
        function scan(){
            fetch('/scan').then(r=>r.json()).then(list=>{
                let dd=document.getElementById('ssidDropdown');
                dd.innerHTML='';
                let opt=document.createElement('option');
                opt.value='';
                opt.text=list.length>0?'Select network':'No networks found';
                dd.appendChild(opt);
                list.forEach(s=>{
                    let o=document.createElement('option');
                    o.value=s;
                    o.text=s;
                    dd.appendChild(o);
                });
                dd.onchange=function(){document.getElementById('ssid').value=dd.value;};
            }).catch(()=>{
                document.getElementById('ssidDropdown').innerHTML='<option>Scan failed</option>';
            });
        }
        function save(){
            let s=document.getElementById('ssid').value;
            let p=document.getElementById('pass').value;
            if(!s){alert('Please enter an SSID');return;}
            fetch('/save',{
                method:'POST',
                headers:{'Content-Type':'application/json'},
                body:JSON.stringify({ssid:s,pass:p})
            }).then(r=>r.text()).then(t=>{
                document.getElementById('status').innerText=t;
                setTimeout(updateStatus,3000);
            });
        }
        function forget(){
            if(!confirm('Forget saved WiFi credentials?'))return;
            fetch('/forget').then(r=>r.text()).then(t=>{
                document.getElementById('status').innerText=t;
                document.getElementById('ssid').value='';
                document.getElementById('pass').value='';
            });
        }
        setInterval(scan,5000);
        setInterval(updateStatus,2000);
        window.onload=function(){scan();updateStatus();};
    </script>
</body>
</html>
        )rawliteral";
        request->send(200, "text/html", page);
    });

    // ===== CHUNKED OTA PAGE WITH PROGRESS BAR =====
    server.on("/ota", HTTP_GET, [](AsyncWebServerRequest *request){
        String page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>OXFP OTA Update</title>
    <meta name="viewport" content="width=340,initial-scale=1">
    <style>
        :root{--bg:#0e0f12;--panel:#171a1f;--text:#e8ebf1;--accent:#52d273;--info:#5aa9ff;--border:#242833;--muted:#99a2b2}
        *{box-sizing:border-box}
        body{margin:0;background:var(--bg);color:var(--text);font-family:system-ui,sans-serif;padding:16px}
        .container{max-width:360px;margin:0 auto;background:var(--panel);padding:24px;border-radius:12px;box-shadow:0 8px 24px rgba(0,0,0,.35);border:1px solid var(--border)}
        .header{text-align:center;margin-bottom:20px}
        .header h1{margin:0 0 8px;font-size:24px;font-weight:600}
        input[type=file],button{width:100%;padding:12px;margin:8px 0;border-radius:8px;border:1px solid var(--border);background:#11151a;color:var(--text);font-size:15px;cursor:pointer}
        button{font-weight:600;transition:all .2s}
        .btn-primary{background:var(--accent);border-color:transparent;color:#08120a}
        .btn-primary:hover{opacity:.9}
        .btn-info{background:var(--info);border-color:transparent;color:#fff}
        .btn-info:hover{opacity:.9}
        .progress-container{margin:16px 0;display:none}
        .progress-bar{width:100%;height:32px;background:#11151a;border-radius:8px;overflow:hidden;border:1px solid var(--border)}
        .progress-fill{height:100%;background:linear-gradient(90deg,var(--accent),var(--info));transition:width .3s;display:flex;align-items:center;justify-content:center;color:#fff;font-weight:600;font-size:14px}
        .status{margin-top:16px;padding:12px;background:#11151a;border-radius:6px;font-size:14px;text-align:center;color:var(--muted)}
        .file-info{margin:8px 0;padding:8px;background:#11151a;border-radius:6px;font-size:13px;color:var(--muted)}
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>OTA Update</h1>
            <div class="subtitle" style="color:var(--muted);font-size:14px">Upload new firmware</div>
        </div>
        <input type="file" id="fileInput" accept=".bin" onchange="showFileInfo()">
        <div id="fileInfo" class="file-info" style="display:none"></div>
        <button class="btn-primary" onclick="uploadFirmware()" id="uploadBtn">Upload &amp; Flash</button>
        <div class="progress-container" id="progressContainer">
            <div class="progress-bar">
                <div class="progress-fill" id="progressFill">0%</div>
            </div>
        </div>
        <div class="status" id="status">Select a .bin file to begin</div>
        <button class="btn-info" onclick="window.location='/'" style="margin-top:8px">Back to WiFi Setup</button>
    </div>
    <script>
        function showFileInfo(){
            let file=document.getElementById('fileInput').files[0];
            if(file){
                let info=document.getElementById('fileInfo');
                info.style.display='block';
                info.innerHTML=`<strong>File:</strong> ${file.name}<br><strong>Size:</strong> ${(file.size/1024).toFixed(1)} KB`;
                document.getElementById('status').innerText='Ready to upload';
            }
        }
        function uploadFirmware(){
            let file=document.getElementById('fileInput').files[0];
            if(!file){alert('Please select a firmware file');return;}
            if(!file.name.endsWith('.bin')){alert('Please select a .bin file');return;}
            
            document.getElementById('uploadBtn').disabled=true;
            document.getElementById('uploadBtn').innerText='Uploading...';
            document.getElementById('progressContainer').style.display='block';
            
            let xhr=new XMLHttpRequest();
            xhr.upload.onprogress=function(e){
                if(e.lengthComputable){
                    let pct=Math.round(100*e.loaded/e.total);
                    document.getElementById('progressFill').style.width=pct+'%';
                    document.getElementById('progressFill').innerText=pct+'%';
                    document.getElementById('status').innerText='Uploading: '+pct+'%';
                }
            };
            xhr.onload=function(){
                if(xhr.status==200){
                    document.getElementById('status').innerText='Update successful! Rebooting...';
                    document.getElementById('progressFill').innerText='Complete';
                    setTimeout(()=>window.location='/',5000);
                }else{
                    document.getElementById('status').innerText='Update failed: '+xhr.responseText;
                    document.getElementById('uploadBtn').disabled=false;
                    document.getElementById('uploadBtn').innerText='Upload & Flash';
                }
            };
            xhr.onerror=function(){
                document.getElementById('status').innerText='Upload failed - network error';
                document.getElementById('uploadBtn').disabled=false;
                document.getElementById('uploadBtn').innerText='Upload & Flash';
            };
            xhr.open('POST','/update',true);
            xhr.send(file);
        }
    </script>
</body>
</html>
        )rawliteral";
        request->send(200, "text/html", page);
    });

    // ===== CHUNKED OTA UPLOAD HANDLER =====
    server.on("/update", HTTP_POST,
        [](AsyncWebServerRequest *request){
            // This is called after upload completes
            bool success = !Update.hasError();
            if (success) {
                request->send(200, "text/plain", "Update successful! Rebooting...");
                delay(1000);
                ESP.restart();
            } else {
                request->send(500, "text/plain", "Update failed: " + String(Update.errorString()));
            }
        },
        [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
            if (!index) {
                Serial.printf("[OTA] Starting update: %s\n", filename.c_str());
                
                if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                    Update.printError(Serial);
                }
            }
            
            if (Update.write(data, len) != len) {
                Update.printError(Serial);
            }
            
            if (final) {
                if (Update.end(true)) {
                    Serial.println("[OTA] Update complete! Rebooting...");
                } else {
                    Update.printError(Serial);
                }
            }
        }
    );

    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
        String stat;
        if (WiFi.status() == WL_CONNECTED)
            stat = "Connected to " + WiFi.SSID() + " | IP: " + WiFi.localIP().toString();
        else if (state == State::CONNECTING)
            stat = "Connecting to " + ssid + "...";
        else
            stat = "Portal mode - Not connected";
        request->send(200, "text/plain", stat);
    });

    // ===== SCAN ENDPOINT WITH CACHING =====
    server.on("/scan", HTTP_GET, [](AsyncWebServerRequest *request){
        int n = WiFi.scanComplete();
        if (n == -2) {
            WiFi.scanNetworks(true, true);
            String json = "[";
            for (size_t i = 0; i < lastScanResults.size(); ++i) {
                if (i) json += ",";
                json += "\"" + lastScanResults[i] + "\"";
            }
            json += "]";
            request->send(200, "application/json", json);
            return;
        } else if (n == -1) {
            String json = "[";
            for (size_t i = 0; i < lastScanResults.size(); ++i) {
                if (i) json += ",";
                json += "\"" + lastScanResults[i] + "\"";
            }
            json += "]";
            request->send(200, "application/json", json);
            return;
        }
        lastScanResults.clear();
        for (int i = 0; i < n; ++i) {
            lastScanResults.push_back(WiFi.SSID(i));
        }
        WiFi.scanDelete();
        String json = "[";
        for (size_t i = 0; i < lastScanResults.size(); ++i) {
            if (i) json += ",";
            json += "\"" + lastScanResults[i] + "\"";
        }
        json += "]";
        request->send(200, "application/json", json);
    });

    server.on("/forget", HTTP_GET, [](AsyncWebServerRequest *request){
        clearCreds();
        ssid = ""; 
        password = "";
        WiFi.disconnect();
        state = State::PORTAL;
        request->send(200, "text/plain", "WiFi credentials cleared");
    });

    // ===== SAVE ENDPOINT WITH PROPER JSON PARSING & ERROR HANDLING =====
    server.on("/save", HTTP_POST,
        [](AsyncWebServerRequest *request){
            Serial.println("[WiFiMgr] ERROR: /save called without body handler");
            request->send(400, "text/plain", "No data received");
        },
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            if (index == 0) {
                request->_tempObject = new String();
                ((String*)request->_tempObject)->reserve(total);
            }
            
            String* body = (String*)request->_tempObject;
            for (size_t i = 0; i < len; i++) {
                *body += (char)data[i];
            }
            
            if (index + len != total) {
                return;
            }
            
            Serial.printf("[WiFiMgr] Received body (%d bytes): %s\n", body->length(), body->c_str());
            
            int ssidStart = body->indexOf("\"ssid\":\"");
            int passStart = body->indexOf("\"pass\":\"");
            
            if (ssidStart == -1) {
                Serial.println("[WiFiMgr] ERROR: SSID field not found in JSON");
                delete body;
                request->_tempObject = nullptr;
                request->send(400, "text/plain", "Invalid JSON: missing ssid field");
                return;
            }
            
            ssidStart += 8;
            int ssidEnd = body->indexOf("\"", ssidStart);
            
            String newSsid = "";
            String newPass = "";
            
            if (ssidEnd > ssidStart) {
                newSsid = body->substring(ssidStart, ssidEnd);
            }
            
            if (passStart != -1) {
                passStart += 8;
                int passEnd = body->indexOf("\"", passStart);
                if (passEnd > passStart) {
                    newPass = body->substring(passStart, passEnd);
                }
            }
            
            delete body;
            request->_tempObject = nullptr;
            
            if (newSsid.length() == 0) {
                Serial.println("[WiFiMgr] ERROR: Empty SSID");
                request->send(400, "text/plain", "SSID cannot be empty");
                return;
            }
            
            Serial.printf("[WiFiMgr] Saving credentials - SSID: '%s', Pass length: %d\n", 
                         newSsid.c_str(), newPass.length());
            
            if (!saveCreds(newSsid, newPass)) {
                Serial.println("[WiFiMgr] ERROR: Failed to save credentials to flash");
                request->send(500, "text/plain", "Failed to save config to flash");
                return;
            }
            
            ssid = newSsid;
            password = newPass;
            state = State::CONNECTING;
            connectAttempts = 0;
            connectStartTime = millis();
            
            WiFi.disconnect(true, true);
            delay(1000);
            WiFi.mode(WIFI_AP_STA);
            delay(100);
            WiFi.begin(newSsid.c_str(), newPass.c_str());
            
            Serial.printf("[WiFiMgr] ✅ Config saved, connecting to: %s\n", newSsid.c_str());
            request->send(200, "text/plain", "Saved! Connecting to: " + newSsid);
        }
    );

    // ===== CAPTIVE PORTAL REDIRECTS =====
    auto cp = [](AsyncWebServerRequest *r){
        r->send(200, "text/html", "<meta http-equiv='refresh' content='0; url=/' />");
    };
    server.on("/generate_204", HTTP_GET, cp);
    server.on("/hotspot-detect.html", HTTP_GET, cp);
    server.on("/redirect", HTTP_GET, cp);
    server.on("/ncsi.txt", HTTP_GET, cp);
    server.on("/captiveportal", HTTP_GET, cp);
    server.onNotFound(cp);

    server.begin();
    state = State::PORTAL;
}

void stopPortal() {
    dnsServer.stop();
}

void tryConnect() {
    if (ssid.length() > 0) {
        Serial.printf("[WiFiMgr] Attempting connection to: %s\n", ssid.c_str());
        
        WiFi.disconnect(true, true);
        delay(1000);
        
        WiFi.mode(WIFI_AP_STA);
        delay(100);
        WiFi.begin(ssid.c_str(), password.c_str());
        state = State::CONNECTING;
        connectAttempts = 0;
        connectStartTime = millis();
    } else {
        startPortal();
    }
}

void begin() {
    LedStat::setStatus(LedStatus::Booting);
    loadCreds();
    startPortal();
    if (ssid.length() > 0)
        tryConnect();
}

void loop() {
    dnsServer.processNextRequest();
    
    // ===== SAFE RECONNECTION MECHANISM =====
    if (state == State::CONNECTING) {
        wl_status_t status = WiFi.status();
        
        if (status == WL_CONNECTED) {
            state = State::CONNECTED;
            Serial.println("[WiFiMgr] ✅ WiFi connected");
            Serial.printf("[WiFiMgr] IP: %s\n", WiFi.localIP().toString().c_str());
            Serial.printf("[WiFiMgr] Connected after %lu ms\n", millis() - connectStartTime);
            LedStat::setStatus(LedStatus::WifiConnected);
            connectAttempts = 0;
        } else if (status == WL_CONNECT_FAILED || status == WL_NO_SSID_AVAIL) {
            connectAttempts++;
            Serial.printf("[WiFiMgr] Connection failed (status: %d). Attempt %d/%d\n", 
                         status, connectAttempts, maxAttempts);
            
            if (connectAttempts >= maxAttempts) {
                Serial.println("[WiFiMgr] ❌ Max attempts reached, keeping portal active");
                Serial.println("[WiFiMgr] Will retry connection in 5 minutes...");
                WiFi.disconnect(true, true);
                state = State::PORTAL;
                lastRetryTime = millis(); // Track when we gave up
                LedStat::setStatus(LedStatus::WifiFailed);
            } else {
                Serial.println("[WiFiMgr] Disconnecting and waiting for clean state...");
                WiFi.disconnect(true, true);
                
                // CRITICAL: Wait for WiFi to report it's actually disconnected
                unsigned long waitStart = millis();
                while (WiFi.status() != WL_DISCONNECTED && (millis() - waitStart) < 3000) {
                    delay(100);
                }
                Serial.printf("[WiFiMgr] Status after disconnect: %d\n", WiFi.status());
                
                delay(500); // Additional settling time
                WiFi.mode(WIFI_AP_STA);
                delay(100);
                WiFi.begin(ssid.c_str(), password.c_str());
                connectStartTime = millis();
            }
        } else if (millis() - connectStartTime > connectTimeout) {
            connectAttempts++;
            Serial.printf("[WiFiMgr] Connection timeout after %lu ms (status: %d). Attempt %d/%d\n",
                         connectTimeout, status, connectAttempts, maxAttempts);
            
            if (connectAttempts >= maxAttempts) {
                Serial.println("[WiFiMgr] ❌ Max attempts reached, keeping portal active");
                Serial.println("[WiFiMgr] Will retry connection in 5 minutes...");
                WiFi.disconnect(true, true);
                state = State::PORTAL;
                lastRetryTime = millis(); // Track when we gave up
                LedStat::setStatus(LedStatus::WifiFailed);
            } else {
                Serial.println("[WiFiMgr] Disconnecting and waiting for clean state...");
                WiFi.disconnect(true, true);
                
                // CRITICAL: Wait for WiFi to report it's actually disconnected
                unsigned long waitStart = millis();
                while (WiFi.status() != WL_DISCONNECTED && (millis() - waitStart) < 3000) {
                    delay(100);
                }
                Serial.printf("[WiFiMgr] Status after disconnect: %d\n", WiFi.status());
                
                delay(500);
                WiFi.mode(WIFI_AP_STA);
                delay(100);
                WiFi.begin(ssid.c_str(), password.c_str());
                connectStartTime = millis();
            }
        }
    }
    
    // ===== PERIODIC CONNECTION CHECK & AUTO-RECONNECT =====
    if (state == State::CONNECTED) {
        if (millis() - lastConnCheck > connCheckInterval) {
            lastConnCheck = millis();
            
            if (WiFi.status() != WL_CONNECTED) {
                Serial.println("[WiFiMgr] ⚠️ Connection lost, attempting reconnect...");
                state = State::CONNECTING;
                connectAttempts = 0;
                connectStartTime = millis();
                LedStat::setStatus(LedStatus::WifiFailed);
                
                WiFi.disconnect(true, true);
                
                // Wait for clean disconnect
                unsigned long waitStart = millis();
                while (WiFi.status() != WL_DISCONNECTED && (millis() - waitStart) < 3000) {
                    delay(100);
                }
                
                delay(500);
                WiFi.mode(WIFI_AP_STA);
                delay(100);
                WiFi.begin(ssid.c_str(), password.c_str());
            }
        }
    }
    
    // ===== PERIODIC RETRY AFTER FAILURE =====
    if (state == State::PORTAL && ssid.length() > 0 && lastRetryTime > 0) {
        if (millis() - lastRetryTime > retryInterval) {
            Serial.println("[WiFiMgr] 🔄 Periodic retry - attempting to reconnect...");
            connectAttempts = 0;
            connectStartTime = millis();
            state = State::CONNECTING;
            lastRetryTime = 0; // Reset so we don't immediately retry again
            
            WiFi.disconnect(true, true);
            delay(500);
            WiFi.mode(WIFI_AP_STA);
            delay(100);
            WiFi.begin(ssid.c_str(), password.c_str());
        }
    }
}

void restartPortal() {
    startPortal();
}

void forgetWiFi() {
    clearCreds();
    ssid = "";
    password = "";
    WiFi.disconnect(true);
    state = State::PORTAL;
    Serial.println("[WiFiMgr] WiFi credentials cleared");
}

void forgetWiFiFromSerial() {
    clearCreds();
    WiFi.disconnect(true);
    ssid = "";
    password = "";
    Serial.println("[SerialCmd] WiFi credentials forgotten.");
    startPortal();
}

bool isConnected() {
    return WiFi.status() == WL_CONNECTED && state == State::CONNECTED;
}

String getStatus() {
    if (isConnected()) return "Connected to: " + ssid;
    if (state == State::CONNECTING) return "Connecting to: " + ssid;
    return "Not connected";
}

} // namespace WiFiMgr