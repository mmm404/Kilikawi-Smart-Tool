# Kilikawi Smart Farm Tool - ESP32 Dashboard

<img width="1365" height="734" alt="image" src="https://github.com/user-attachments/assets/dff659e0-e517-4e18-8a78-8449fea9008a" />

A robust, offline ESP32-based dashboard for real-time monitoring of agricultural tools. This system operates as a Wi-Fi Access Point (AP) and serves a web-based interface for tracking Voltage, Current, and Power.
<img width="1366" height="692" alt="image" src="https://github.com/user-attachments/assets/7e8a6a22-6bab-4e4e-8402-e62f43987d1a" />

# Kilikawi Smart Farm Tool - ESP32 Dashboard

<img width="1365" height="734" alt="Dashboard Preview" src="https://github.com/user-attachments/assets/dff659e0-e517-4e18-8a78-8449fea9008a" />

A robust, offline ESP32-based dashboard for real-time monitoring of high-power agricultural tools. This system operates as a Wi-Fi Access Point (AP) and serves a web interface for tracking heavy-duty powertrain metrics.

<img width="1114" height="896" alt="Circuit Diagram" src="https://github.com/user-attachments/assets/a6d56ed3-1397-42e2-a06b-d0198fff90f9" />

## Key Features
- **Offline Operation**: Operates as a Wi-Fi Access Point (SSID: `Kilikawi-Smart-Tool`).
- **Real-time Graphs**: Live monitoring using Chart.js (stored locally via LittleFS).
- **72V, 3kW BLDC Powertrain**: Specialized for high-power monitoring using the **ACS758LCB-050B** current sensor.
- **Admin/Farmer Roles**: Dual access modes with password-protected admin features (CSV download, profile reset).
- **Data Preservation**: 50-profile limit with automatic downsampling to manage LittleFS space.
- **Moving Average Filter**: 20-sample software filter to smooth out electromagnetic interference from the BLDC motor.
- **Visual Feedback**: Comprehensive LED indicators for Battery Level, Accessory Type, and Throttle State.
- **JSON Serial Output**: Broadcasts telemetry data via Serial for external integration.

---

## Hardware Pinout Guide

### Powertrain Sensors (72V System)
| ESP32 Pin | Function | Description |
|-----------|----------|-------------|
| **GPIO 39** | Current (VN) | From ACS758 (40mV/A sensitivity, 1.65V offset) |
| **GPIO 36** | Voltage (VP) | From MCP6022 Voltage Follower |

### Accessory Inputs & Indicators
| ESP32 Pin | Function | Accessory |
|-----------|----------|-----------|
| **GPIO 13** | Inductive 1 | Tool Detection (Input) |
| **GPIO 12** | Inductive 2 | Tool Detection (Input) |
| **GPIO 14** | Inductive 3 | Tool Detection (Input) |
| **GPIO 27** | Inductive 4 | Tool Detection (Input) |
| **GPIO 25** | LED 1 | Accessory 1 Status (Output) |
| **GPIO 33** | LED 2 | Accessory 2 Status (Output) |
| **GPIO 32** | LED 3 | Accessory 3 Status (Output) |
| **GPIO 26** | LED 4 | Accessory 4 Status (Output) |

### Throttle Interface
| ESP32 Pin | Input Switch | Indicator LED | State |
|-----------|--------------|---------------|-------|
| **GPIO 21** | Forward | GPIO 18 | Forward Mode |
| **GPIO 22** | Reverse | GPIO 5 | Reverse Mode |
| **GPIO 23** | Neutral | GPIO 17 | Neutral Mode |

### Battery Level Indicators (LED Bar)
| ESP32 Pin | Battery Level |
|-----------|---------------|
| **GPIO 19** | 20% |
| **GPIO 15** | 40% |
| **GPIO 2**  | 60% |
| **GPIO 4**  | 80% |
| **GPIO 16** | 100% |

---

## Technical Specifications
- **Voltage Range**: 54.0V (Cut-off) to 84.6V (Peak Charge).
- **Current Sensing**: Bi-directional 50A capability with automatic zero-offset calibration at boot.
- **Filtering**: 20-sample moving average for stable Power (W) readings.
- **Filesystem**: LittleFS for storing `chart.min.js` and JSON log profiles.

---

## Software Configuration
- **Wi-Fi SSID**: `Kilikawi-Smart-Tool`
- **Wi-Fi Password**: `kilikawi2026`
- **Access URL**: `http://192.168.4.1`
- **Admin Password**: `kilikawi`

### JSON Telemetry Format
The system outputs the following structure via Serial every 1000ms:
```json
{
  "timestamp": 45000,
  "accessory": "Tiller",
  "current": 41.67,
  "voltage": 72.0,
  "power": 3000.2,
  "throttle": 1,
  "battery_percent": 85
}
```