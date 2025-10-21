#include "ota.h"
#include <Update.h>

namespace OTA {

static const char* ota_html PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Type D OTA Update</title>
    <meta name="viewport" content="width=320,initial-scale=1">
    <style>
        body {background:#111;color:#EEE;font-family:sans-serif;}
        .container {max-width:320px;margin:24px auto;background:#222;padding:2em;border-radius:8px;box-shadow:0 0 16px #0008;}
        h2 {margin-bottom:1em;}
        input[type=file] {margin:.7em 0;padding:.5em;}
        .btn-primary {background:#299a2c;color:white;border:none;padding:.7em 1.5em;border-radius:6px;cursor:pointer;}
        .status {margin-top:1em;font-size:.95em;}
    </style>
</head>
<body>
<div class="container">
    <h2>OTA Firmware Update</h2>
    <form id="upload_form" enctype="multipart/form-data">
        <input type="file" name="firmware" id="firmware"><br>
        <button class="btn-primary" type="submit">Upload & Update</button>
    </form>
    <div class="status" id="status">Ready.</div>
</div>
<script>
document.getElementById('upload_form').onsubmit = function(e){
    e.preventDefault();
    var file = document.getElementById('firmware').files[0];
    if (!file) {document.getElementById('status').innerText = "No file selected."; return;}
    var form = new FormData();
    form.append("firmware", file);
    var xhr = new XMLHttpRequest();
    xhr.open("POST", "/update", true);
    xhr.upload.onprogress = function(e){
        if (e.lengthComputable)
            document.getElementById('status').innerText = "Uploading: " + Math.round(100*e.loaded/e.total) + "%";
    };
    xhr.onload = function(){
        document.getElementById('status').innerText = xhr.responseText;
        if (xhr.status == 200 && xhr.responseText.indexOf("Success") !== -1)
            setTimeout(()=>location.reload(),2000);
    };
    xhr.onerror = function(){ document.getElementById('status').innerText = "Upload failed."; };
    xhr.send(form);
};
</script>
</body>
</html>
)rawliteral";

void begin(AsyncWebServer& server) {
    // Serve the OTA update page
    server.on("/update", HTTP_GET, [](AsyncWebServerRequest* request){
        request->send_P(200, "text/html", ota_html);
    });

    // Handle the firmware upload
    server.on("/update", HTTP_POST, 
        [](AsyncWebServerRequest* request){},
        [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
            if (!index) {
                if (!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)) {
                    Update.printError(Serial);
                }
            }
            if (Update.write(data, len) != len) {
                Update.printError(Serial);
            }
            if (final) {
                if (Update.end(true)) {
                    request->send(200, "text/plain", "Success! Rebooting...");
                    delay(500);
                    ESP.restart();
                } else {
                    request->send(500, "text/plain", "Update failed.");
                }
            }
        }
    );
}
} // namespace OTA
