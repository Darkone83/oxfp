# OXFP - Original Xbox Front Panel RGB Controller

<div align=center>
  <img src="https://github.com/Darkone83/oxfp/blob/main/images/DC%20logo.png">
</div>

OXFP is a highly flexible RGB controller for the original Xbox front panel, supporting original LED status mirroring, full color override, and advanced animations. It is designed for the ESP32 platform and includes a built-in WiFi configuration portal and web-based RGB control.

<div align=center>
  <img src="https://github.com/Darkone83/oxfp/blob/main/images/Render_Front.png" height=300 width=300><img src="https://github.com/Darkone83/oxfp/blob/main/images/Render_Back.png" height=300 width=300>
</div>

## Features

- **Original Xbox LED Logic:**  
  By default, WS2812 RGB LEDs mimic the classic left/right Xbox front panel status indicators using the original input pins.

- **WiFi-Enabled Configuration Portal:**  
  Connect your device to WiFi and access a full-featured web UI for LED mode selection, color customization, and animation control.

- **Static RGB Override:**  
  Override default Xbox logic with custom color mapping for each indicator (Red, Green, Orange) on both LEDs.

- **Animation Modes:**  
  Select from multiple dynamic RGB effects with user-selectable colors and global brightness.

- **Global Brightness Control:**  
  Adjust overall LED brightness from the web UI.

- **Settings Persistence:**  
  All user settings are saved in flash and recalled automatically on boot.


## Required Libraries

Install these libraries via the Arduino Library Manager:

- [Adafruit NeoPixel](https://github.com/adafruit/Adafruit_NeoPixel)
- [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer)
- [AsyncTCP](https://github.com/me-no-dev/AsyncTCP)  
  *(for ESP32 AsyncWebServer)*
- [ArduinoJson](https://arduinojson.org/)
- [Preferences](https://github.com/espressif/arduino-esp32/tree/master/libraries/Preferences)


## Hardware Connections

You will need to remove the connector from your original Xbox front panel connector and solder to the OXFP. then you will need to run a spare 5V wire to the 5V pad

## Setup & Usage

### 1. **Flash the Firmware**

- Open the project in Arduino IDE.
- Install all required libraries (see above).
- Select your ESP32 board and correct COM port.
- Flash as usual.

### 2. **WiFi Configuration**

- On first boot, the device starts in **WiFi Access Point mode** as `OXFP Setup`.
- Connect to this WiFi with your phone or computer.
- The captive portal or [http://192.168.4.1](http://192.168.4.1) will let you join your main WiFi network.

### 3. **Access the Web UI**

- Once connected to WiFi, find the device’s IP address (check your router or serial output).
- Or, just open [http://oxfp.local](http://oxfp.local) (mDNS).
- Visit `/config` (e.g. [http://oxfp.local/config](http://oxfp.local/config)).

### 4. **Web Configuration UI**

- Set **global brightness** with the slider.
- Select **Mode:**
    - **Static**: Custom RGB mapping for each Xbox status color (Red, Green, Orange) on both LEDs.
    - **Animation**: Select from several effects. User colors (where applicable) and brightness are used.
- Click **Save** to apply and persist settings.


## Modes Overview

### **1. Stock (Original) Mode**
- LEDs follow Xbox panel logic:
    - Green = console on/ready, Red = error, Orange = warning (just like stock hardware).

### **2. Static Override**
- LEDs ignore Xbox inputs and always display user-chosen colors for Red, Green, and Orange states (for both left and right LEDs).

### **3. Animation Modes**

- **Pulse**: Both LEDs smoothly pulse brightness with user-selected color.
- **Fade**: Fade in/out in a sawtooth pattern, user color.
- **Rainbow**: Both LEDs cycle through a smooth rainbow.
- **Dual Rainbow**: Each LED cycles its own rainbow, offset from each other.
- **Color Chase**: LEDs alternate between two user-selected colors in a chasing pattern.
- **Sparkle**: Both LEDs randomly pick between two colors or off, creating a sparkling effect.

## Additional Notes

- All configuration changes are persistent and will be restored on reboot.
- If you lose WiFi access, hold the device reset to re-enter WiFi setup mode.


## License

MIT License


### Attribution

If you modify and redistribute this code, **please credit Darkone83 for the original source**.


