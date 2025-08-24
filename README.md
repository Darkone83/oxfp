# OXFP – Original Xbox Front Panel with RGB Control

<div align=center>
  <img src ="https://github.com/Darkone83/oxfp/blob/main/images/DC%20logo.png">
</div>

OXFP is a highly flexible RGB controller for the original Xbox front panel, supporting original LED status mirroring, static color overrides, per-LED color animations, and a modern web UI for real-time preview and editing. It targets the ESP32 family and includes a Wi-Fi setup portal.

<div align=center>
  <img src="https://github.com/Darkone83/oxfp/blob/main/images/Render_Front.png" width=500 height=300><img src="https://github.com/Darkone83/oxfp/blob/main/images/Render_Back.png" width=500 height=300>
</div>

---

## Features

- **Original Xbox LED Logic**  
  By default, WS2812 RGB LEDs mirror the classic left/right Xbox front panel indicators using the original status inputs.

- **Wi-Fi Configuration Portal**  
  Easily connect your device to Wi-Fi and access a web UI for mode selection, per-LED color customization, and live animation preview.

- **Per-LED Static RGB Override**  
  Override the default Xbox logic with custom color mapping for Red, Green, and Orange states on both LEDs.

- **Advanced Animation Modes (11 total)**  
  Choose from eleven effects. Where applicable, each supports per-LED color selection (A/B) and adjustable speed.

- **Expandability**  
  Add up to two external WS2812-compatible LED strips to mirror the panel LEDs (optional).

- **Global Brightness Control**  
  Adjust overall LED brightness live from the web interface.

- **Settings Persistence**  
  All settings (colors, animation mode, brightness, speed) are saved to flash and restored on boot.

- **Live Preview**  
  Instantly preview any configuration or animation before saving.

---

## Required Libraries

Install via Arduino Library Manager:

- [Adafruit NeoPixel](https://github.com/adafruit/Adafruit_NeoPixel)  
- [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer)  
- [AsyncTCP](https://github.com/me-no-dev/AsyncTCP) *(for ESP32)*  
- [ArduinoJson](https://arduinojson.org/)  
- [Preferences](https://github.com/espressif/arduino-esp32/tree/master/libraries/Preferences)

---

## Required Hardware

ESP32 C6 Zero: <a href="https://www.amazon.com/dp/B0D1CB3PBW?ref=ppx_yo2ov_dt_b_fed_asin_title">Amazon</a>

JST 1.0 3P Connectors: <a href="https://www.amazon.com/dp/B0CQ28CCQG?ref=ppx_yo2ov_dt_b_fed_asin_title&th=1">Amazon</a>

Buttons: <a href="https://www.lcsc.com/product-detail/C5340161.html">LCSC</a>, <a href="https://www.digikey.com/en/products/detail/c-k/PTS647SK38SMTR2-LFS/9649862">Digi-Key</a>

WS2812 LED's: <a href="https://www.amazon.com/dp/B09X1JYT6R?th=1">Amazon</a>

Optional:

WS2812 LED strip: <a href="https://www.amazon.com/dp/B09PBGZMNS?th=1">Amazon</a>

JSt 1.0 Connector kit: <a href="https://www.amazon.com/dp/B0D5X6BY5Z">Amazon</a>

---

## Hardware Connections

Remove the connector from your original Xbox front panel and solder to the OXFP board. Provide a separate 5V wire to the 5V pad to power the WS2812 LEDs and connect ground to the GND pad.

If you can't reuse your original XBOX FP harness, an alternate can be sourced

PHD 2.0 10p : <a href="https://www.aliexpress.us/item/3256807761209020.html?spm=a2g0o.order_list.order_list_main.59.2dc11802OOCfQn&gatewayAdapt=glo2usa">Aliexpress</a>

---

## Setup & Usage

### 1) Flash the Firmware
- Open the project in Arduino IDE.
- Install the required libraries.
- Select your ESP32 board + COM port.
- Upload.

### 2) Wi-Fi Configuration
- On first boot, the device starts in **Wi-Fi Access Point** mode named **`OXFP Setup`**.
- Connect from your phone/computer.
- Use the captive portal or visit `http://192.168.4.1` to join your home Wi-Fi.

### 3) Access the Web UI
- After joining Wi-Fi, find the device IP from your router or the serial console.  
- Or use **mDNS** at `http://oxfp.local`.  
- Open `http://oxfp.local/config` for full LED control.

### 4) Web Configuration UI
- **Brightness**: Adjust via slider.  
- **Mode**:
  - **Stock** — Xbox-original status logic (Green/Red/Orange).
  - **Static** — Custom RGB mapping for each status (Red, Green, Orange). Both LEDs together.
  - **Animation** — Pick from 11 effects; set per-LED colors (A/B) when supported, speed, and brightness.
- **Live Preview**: Preview any changes instantly.  
- **Save**: Persist settings to flash.  
- **Reset**: Restore factory defaults.

---

## Modes Overview

### 1. Stock (Original) Mode
LEDs follow the original Xbox front panel status:
- **Green** = console on/ready  
- **Red** = error  
- **Orange** = warning  
Blink patterns are preserved.

### 2. Static Override
LEDs ignore Xbox inputs and always display your chosen colors for **Green**, **Red**, and **Orange** states (both LEDs).

### 3. Animation Modes (11)

Where applicable, animations use **Color A** for LED 0 (left) and **Color B** for LED 1 (right). All respect global **Speed** (1–10) and **Brightness**, unless noted.

1. **Color Bounce** – LEDs alternate (“bounce”) between two user colors (A/B).  
2. **Breathing/Pulse** – Both LEDs pulse smoothly; each uses its own color (A/B).  
3. **Chase** – A simple two-step chase between the two LEDs (A→B→swap).  
4. **RGB Fade** – Both LEDs sweep through the full RGB spectrum together. *(Ignores user colors.)*  
5. **Blinking** – Both LEDs blink on/off in sync; each uses A/B.  
6. **Alternating** – LEDs alternate A/B back and forth.  
7. **Fire/Flicker** – Warm, randomized fire-like flicker per LED. *(Colors and speed ignored; procedural hues.)*  
8. **Plasma** – Smooth, organic “plasma” glow with continuous cross-fades and spectral drift (uses A/B).  
9. **Heartbeat** – “Lub-dub” double-pulse then rest; left=A, right=B.  
10. **Opposed Breath** – Counter-phase breathing (left brightens as right dims, then swap).  
11. **Sparkle** – Sparse random sparkles over a dark base; brief pops in A/B.

> **Note:** If the console signals an **error state**, stock/error indication can preempt animations to display the correct original colors/patterns.

---

## Additional Notes

- Configuration changes are persistent and restored at boot.  
- If Wi-Fi is lost, reset to re-enter setup mode.  
- Live preview works for both static and animated effects.

---

## License

MIT License

---

### Attribution

If you modify and redistribute this code, **please credit Darkone83 for the original source**.
