# OXFP - Original Xbox Front Panel with RGB control

<div align=center>
  <img src="https://github.com/Darkone83/oxfp/blob/main/images/DC%20logo.png">
</div>

OXFP is a highly flexible RGB controller for the original Xbox front panel, supporting original LED status mirroring, static color override, per-LED color animation, and advanced web-based configuration. It is designed for the ESP32 platform and includes a WiFi setup portal and a rich web UI for real-time preview and editing.

<div align=center>
  <img src="https://github.com/Darkone83/oxfp/blob/main/images/Render_Front.png" height=300 width=300><img src="https://github.com/Darkone83/oxfp/blob/main/images/Render_Back.png" height=300 width=300>
</div>

## Features

- **Original Xbox LED Logic:**  
  By default, WS2812 RGB LEDs perfectly mirror the classic left/right Xbox front panel indicators, using the original status inputs.

- **WiFi Configuration Portal:**  
  Easily connect your device to WiFi and access a web-based UI for mode selection, per-LED color customization, and live animation preview.

- **Per-LED Static RGB Override:**  
  Override the default Xbox logic with custom color mapping for Red, Green, and Orange states on both LEDs.

- **Advanced Animation Modes:**  
  Select from 11 dynamic RGB effects, each with customizable per-LED colors (where applicable) and adjustable speed.

- **Expanability:**
  Add 2 extra WS2812-compatible LED strips for more flair. LED strips will reflect LEDS on the OXFP.

- **Global Brightness Control:**  
  Adjust the overall LED brightness live from the web interface.

- **Settings Persistence:**  
  All settings (including colors, animation, brightness, and speed) are saved to flash and recalled on every boot.

- **Live Preview:**  
  Instantly preview any configuration or animation on the front panel before saving.

---

## Required Libraries

Install these libraries via the Arduino Library Manager:

- [Adafruit NeoPixel](https://github.com/adafruit/Adafruit_NeoPixel)
- [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer)
- [AsyncTCP](https://github.com/me-no-dev/AsyncTCP)  
  *(for ESP32 AsyncWebServer)*
- [ArduinoJson](https://arduinojson.org/)
- [Preferences](https://github.com/espressif/arduino-esp32/tree/master/libraries/Preferences)

---

## Hardware Connections

You will need to remove the connector from your original Xbox front panel and solder to the OXFP board. A separate 5V wire must be run to the 5V pad to power the WS2812 LEDs.

---

## Setup & Usage

### 1. **Flash the Firmware**

- Open the project in Arduino IDE.
- Install all required libraries (see above).
- Select your ESP32 board and correct COM port.
- Upload the firmware.

### 2. **WiFi Configuration**

- On first boot, the device starts in **WiFi Access Point mode** named `OXFP Setup`.
- Connect via your phone or computer.
- The captive portal or [http://192.168.4.1](http://192.168.4.1) allows you to join your home WiFi.

### 3. **Access the Web UI**

- Once connected, find the device’s IP address (via router or serial output).
- Or, just use [http://oxfp.local](http://oxfp.local) (mDNS enabled).
- Open [http://oxfp.local/config](http://oxfp.local/config) for full LED control.

### 4. **Web Configuration UI**

- **Set brightness** with the slider.
- **Select Mode:**
    - **Stock**: Xbox-original status logic (Green/Red/Orange).
    - **Static**: Custom RGB mapping for each status (Red, Green, Orange). Both LEDs set together.
    - **Animation**: Select from 7 dynamic effects. Each effect allows per-LED color selection (if applicable), speed adjustment, and brightness.
- **Live Preview:**  
  Preview any combination of settings and animation instantly.
- **Save:**  
  Applies and persists your settings to flash.
- **Reset:**  
  Restores all options to factory defaults.

---

## Modes Overview

### **1. Stock (Original) Mode**
- LEDs follow Xbox front panel status:
    - Green = console ready/on, Red = error, Orange = warning (matches hardware).

### **2. Static Override**
- LEDs ignore Xbox inputs and always display your chosen colors for Red, Green, and Orange states (both LEDs).

### **3. Animation Modes**

Each animation allows independent color selection for **LED 0** and **LED 1** (where relevant):

1. **Color Bounce:**  
   LEDs alternate ("bounce") between two user-selected colors.
2. **Breathing/Pulse:**  
   Both LEDs pulse smoothly in brightness (each with its own color).
3. **Chase:**  
   A single color appears to "chase" between the two LEDs.
4. **RGB Fade:**  
   Both LEDs cycle through a full RGB color wheel/fade.
5. **Blinking:**  
   Both LEDs blink on/off, each with their selected color.
6. **Alternating:**  
   LEDs alternate their colors back and forth.
7. **Fire/Flicker:**  
   Both LEDs flicker with warm fire-like hues (per-LED random, colors not user-settable in this mode).
8. **Plasma:**
    Smooth, organic “plasma” glow—continuous cross-fade with subtle spectral drift.
9. **Heartbeat:**
    “Lub-dub” rhythm: quick double-pulse then rest; A on left, B on right.
10. **Opposed Breath:**
    Counter-phase breathing—left brightens while right dims (and vice versa).
11. **Sparkle:**
    Sparse random sparkles over a dark base; brief pops in A/B.

All animations support speed and brightness adjustment via sliders.

---

## Additional Notes

- All configuration changes are persistent and will be restored on reboot.
- If you lose WiFi access, reset the device to re-enter WiFi setup mode.
- Live preview works for both static and animated effects.

---

## License

MIT License

---

### Attribution

If you modify and redistribute this code, **please credit Darkone83 for the original source**.

