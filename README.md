# Kilikawi Smart Farm Tool - ESP32 Dashboard

A robust, offline ESP32-based dashboard for real-time monitoring of agricultural tools. This system operates as a Wi-Fi Access Point (AP) and serves a web-based interface for tracking Voltage, Current, and Power.

## Features
- **Offline Operation**: Operates as a Wi-Fi Access Point (SSID: `Kilikawi-Smart-Tool`).
- **Real-time Graphs**: Live monitoring using Chart.js (stored locally).
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

## Installation
1. Open the `.ino` file in VS Code (with PlatformIO or Arduino extension) or Arduino IDE.
2. Ensure **ESP32 Core 3.x** is installed.
3. Select **Huge APP (3MB No OTA)** partition scheme in Tools.
4. Upload the code.
5. Upload the `data/` folder to LittleFS (using the LittleFS upload tool).
