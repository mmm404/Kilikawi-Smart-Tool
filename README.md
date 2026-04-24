# Kilikawi Smart Farm Tool - ESP32 Dashboard
<img width="1365" height="734" alt="image" src="https://github.com/user-attachments/assets/dff659e0-e517-4e18-8a78-8449fea9008a" />

A robust, offline ESP32-based dashboard for real-time monitoring of agricultural tools. This system operates as a Wi-Fi Access Point (AP) and serves a web-based interface for tracking Voltage, Current, and Power.

## Features
- **Offline Operation**: Operates as a Wi-Fi Access Point (SSID: `Kilikawi-Smart-Tool`).
- **Real-time Graphs**: Live monitoring using Chart.js (stored locally).
- **Data Preservation**: Historical logging with 50-profile limit and downsampling to save space while maintaining power accuracy.
- **Admin/Farmer Roles**: Dual access modes with password-protected admin features (CSV download, profile reset).
- **Tool Detection**: Automatic identification of attached accessories via digital pins.
- **72V, 3kW BLDC Powertrain**: Accurate real-time power calculation using ACS758LCB-050B current sensor and MCP6022 voltage follower.
- **Zero-Current Offset Calibration**: Automatic calibration at startup for accurate current readings.
- **Moving Average Filter**: 20-sample filter to smooth electromagnetic noise from the motor.
- **Boot-Up LED Animation**: Visual indication of system startup with flowing LED patterns.
- **JSON Serial Output**: Real-time data transmission to external dashboards.

---

## Hardware Pinout Guide

### 72V, 3kW BLDC Powertrain Sensors
| ESP32 Pin | Function | Description |
|-----------|----------|-------------|
| **GPIO 34** | Current Sensor (VN) | Analog input from ACS758LCB-050B (40mV/A sensitivity) |
| **GPIO 35** | Voltage Sensor (VP) | Analog input from MCP6022 voltage follower |

### Inductive Sensor Inputs (Tool Detection)
| ESP32 Pin | Function | Binary Code | Accessory |
|-----------|----------|-------------|-----------|
| **GPIO 13** | Inductive Sensor 1 | - | Tool Detection |
| **GPIO 12** | Inductive Sensor 2 | - | Tool Detection |
| **GPIO 14** | Inductive Sensor 3 | - | Tool Detection |
| **GPIO 27** | Inductive Sensor 4 | - | Tool Detection |

### Accessory Indicator LEDs
| ESP32 Pin | Function | Accessory |
|-----------|----------|-----------|
| **GPIO 35** | LED 1 | Tiller (0000) |
| **GPIO 32** | LED 2 | Sheller (0001) |
| **GPIO 33** | LED 3 | Planter (0010) |
| **GPIO 26** | LED 4 | Mower (0100) |

### Throttle Switch Inputs
| ESP32 Pin | Function | Description |
|-----------|----------|-------------|
| **GPIO 21** | Forward Switch | Throttle forward input |
| **GPIO 22** | Reverse Switch | Throttle reverse input |
| **GPIO 23** | Neutral Switch | Throttle neutral input |

### Throttle State LEDs
| ESP32 Pin | Function | Description |
|-----------|----------|-------------|
| **GPIO 18** | Forward LED | Indicates forward throttle |
| **GPIO 5** | Reverse LED | Indicates reverse throttle |
| **GPIO 17** | Neutral LED | Indicates neutral throttle |

### Battery Percentage LEDs
| ESP32 Pin | Function | Battery Level |
|-----------|----------|---------------|
| **GPIO 34** | LED 1 | 20% |
| **GPIO 15** | LED 2 | 40% |
| **GPIO 2** | LED 3 | 60% |
| **GPIO 4** | LED 4 | 80% |
| **GPIO 16** | LED 5 | 100% |

### Connection Details
1. **ACS758LCB-050B Current Sensor**: Connect to GPIO 34 (VN pin). Sensitivity: 40mV/A, Quiescent voltage: 1.65V (0.5 × VCC).
2. **MCP6022 Voltage Follower**: Connect to GPIO 35 (VP pin) for voltage sensing.
3. **Voltage Range**: 54V (empty) to 84.6V (full) for 72V system.
4. **Inductive Sensors**: Connect to GPIO 13, 12, 14, 27 with pull-up resistors. Ground specific pin to select tool.

---

## Power Calculation Details
- **Nominal Voltage**: 72V
- **Rated Power**: 3000W (3kW)
- **Calculated Current**: I = P/V = 3000W/72V ≈ 41.67A
- **Formula**: P = V × I (real-time power calculation)

---

## Software Configuration
- **Wi-Fi SSID**: `Kilikawi-Smart-Tool`
- **Wi-Fi Password**: `kilikawi2026`
- **Access URL**: `http://192.168.4.1`
- **Admin Password**: `kilikawi`

## JSON Serial Output
The system outputs JSON data to Serial0 every 1 second:
```json
{"timestamp":12345,"accessory":"Tiller","current":41.67,"voltage":72.0,"power":3000.0,"throttle":1,"battery_percent":100}
```

## Installation
1. Open the `.ino` file in VS Code (with PlatformIO or Arduino extension) or Arduino IDE.
2. Ensure **ESP32 Core 3.x** is installed.
3. Select **Huge APP (3MB No OTA)** partition scheme in Tools.
4. Upload the code.
5. Upload the `data/` folder to LittleFS (using the LittleFS upload tool).
