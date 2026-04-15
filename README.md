# Kilikawi Smart Farm Tool - ESP32 Dashboard

<img width="1365" height="734" alt="image" src="https://github.com/user-attachments/assets/dff659e0-e517-4e18-8a78-8449fea9008a" />

A robust, offline ESP32-based dashboard for real-time monitoring of agricultural tools. This system operates as a Wi-Fi Access Point (AP) and serves a web-based interface for tracking Voltage, Current, and Power.

## Features
- **Offline Operation**: Operates as a Wi-Fi Access Point (SSID: `Kilikawi-Smart-Tool`).
- **Real-time Graphs**: Live monitoring using Chart.js (stored locally via LittleFS).
- **Data Preservation**: Historical logging with 50-profile limit and downsampling to save space while maintaining power accuracy.
- **Admin/Farmer Roles**: Dual access modes with password-protected admin features (CSV download, profile reset).
- **Tool Detection**: Automatic identification of attached accessories via digital pins.

---

## Hardware Pinout Guide

| ESP32 Pin | Function | Description |
|-----------|----------|-------------|
| **GPIO 34** | Voltage Sensing | Analog input (0-3.3V) via Voltage Divider (11:1 ratio) |
| **GPIO 35** | Current Sensing | Analog input from ACS712 (0.066V/A sensitivity) |
| **GPIO 32** | Throttle Input | Analog input for throttle state detection |
| **GPIO 13** | Tool ID: Sheller | Digital input (Pull-up, active LOW) |
| **GPIO 12** | Tool ID: Tiller | Digital input (Pull-up, active LOW) |
| **GPIO 14** | Tool ID: Planter | Digital input (Pull-up, active LOW) |
| **GPIO 27** | Tool ID: Mower | Digital input (Pull-up, active LOW) |

### Connection Details
1. **Voltage Divider**: Connect R1=100kΩ and R2=10kΩ to measure batteries up to ~36V safely on Pin 34.
2. **ACS712 Current Sensor**: Connect VCC=5V, GND=GND, and OUT=GPIO 35.
3. **Throttle**: Connect to Pin 32 (Analog range: 0-3.3V).
4. **Tool Selection**: Ground the specific GPIO pin (13, 12, 14, or 27) to identify the attached tool.

---

## Software Configuration
- **Wi-Fi SSID**: `Kilikawi-Smart-Tool`
- **Wi-Fi Password**: `kilikawi2026`
- **Access URL**: `http://192.168.4.1`
- **Admin Password**: `kilikawi`

---

## Installation & Deployment

### 1. Firmware Setup
1. Open `esp32-dashboard.ino` in your preferred IDE (Trae, VS Code, or Arduino IDE).
2. Ensure **ESP32 Core 3.x** is installed via the Boards Manager.
3. **Partition Scheme**: Go to `Tools > Partition Scheme` and select **Huge APP (3MB No OTA)**. This ensures enough space for both the application logic and the filesystem.
4. Click **Upload** to flash the code.

### 2. Filesystem Upload (LittleFS)
The dashboard assets (like `chart.min.js`) are stored in the `/data` folder. You must flash this folder separately to the ESP32's flash memory.

#### Using Arduino IDE 2.x or Trae:
1. Download the [Arduino LittleFS Upload Tool (.vsix)](https://github.com/earlephilhower/arduino-littlefs-upload/releases).
2. Place the `.vsix` file in `%USERPROFILE%\.arduinoIDE\plugins\`.
3. Restart the IDE.
4. **Close the Serial Monitor** (Upload will fail if the port is busy).
5. Press `Ctrl + Shift + P` and select **"Upload LittleFS to Pico/ESP8266/ESP32"**.

#### Using Arduino IDE 1.8.x:
1. Install the [ESP32FS Plugin](https://github.com/me-no-dev/arduino-esp32fs-plugin/releases).
2. Place the `.jar` in `Documents/Arduino/tools/ESP32FS/tool/`.
3. Restart the IDE and select **Tools > ESP32 Sketch Data Upload**.

> **Note**: Standard code updates do not overwrite the LittleFS partition. You only need to re-upload the data if you modify the files inside the `/data` folder.

---

## External Resources
- [Chart.js Documentation](https://www.chartjs.org/docs/latest/) - For modifying graph styles.
- [LittleFS Library](https://github.com/lorol/LITTLEFS) - Filesystem backend for ESP32.
- [Espressif Partition Tables](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/partition-tables.html) - Understanding flash memory allocation.
