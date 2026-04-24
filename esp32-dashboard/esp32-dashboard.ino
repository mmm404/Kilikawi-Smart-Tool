#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <vector>

// --- Configuration ---
const char* AP_SSID = "Kilikawi-Smart-Tool";
const char* AP_PASS = "kilikawi2026"; 
const IPAddress AP_IP(192, 168, 4, 1);

// ============================================================
// PIN ASSIGNMENTS
// ============================================================

// Inductive Sensor Inputs (4 sensors for accessory detection)
const int PIN_INDUCTIVE_1 = 13;  // D13 - Inductive sensor 1
const int PIN_INDUCTIVE_2 = 12;  // D12 - Inductive sensor 2  
const int PIN_INDUCTIVE_3 = 14;  // D14 - Inductive sensor 3
const int PIN_INDUCTIVE_4 = 27;  // D27 - Inductive sensor 4
const int PINS_INDUCTIVE[] = {PIN_INDUCTIVE_1, PIN_INDUCTIVE_2, PIN_INDUCTIVE_3, PIN_INDUCTIVE_4};

// Accessory Indicator Outputs (D35, D32, D33, D26)
const int PIN_ACCESSORY_LED_1 = 35;  // D35 - Tiller indicator
const int PIN_ACCESSORY_LED_2 = 32;  // D32 - Sheller indicator
const int PIN_ACCESSORY_LED_3 = 33;  // D33 - Planter indicator
const int PIN_ACCESSORY_LED_4 = 26;  // D26 - Mower indicator
const int PINS_ACCESSORY_LED[] = {PIN_ACCESSORY_LED_1, PIN_ACCESSORY_LED_2, PIN_ACCESSORY_LED_3, PIN_ACCESSORY_LED_4};

// Voltage & Current Sensor Inputs (72V BLDC Powertrain)
// VN pin - ACS758LCB-050B current sensor (40mV/A sensitivity)
// VP pin - MCP6022 voltage follower
const int PIN_CURRENT_SENSOR = 34;   // VN - Current sensor input (ACS758)
const int PIN_VOLTAGE_SENSOR = 35;    // VP - Voltage sensor input (MCP6022)

// Throttle Switch Inputs (D21, D22, D23)
const int PIN_THROTTLE_FWD = 21;   // D21 - Forward switch
const int PIN_THROTTLE_REV = 22;   // D22 - Reverse switch
const int PIN_THROTTLE_NEUT = 23;  // D23 - Neutral switch
const int PINS_THROTTLE[] = {PIN_THROTTLE_FWD, PIN_THROTTLE_REV, PIN_THROTTLE_NEUT};

// Battery Percentage LEDs (D34, D15, D2, D4, RX2)
const int PIN_BATT_LED_1 = 34;  // D34 - 20% LED
const int PIN_BATT_LED_2 = 15;  // D15 - 40% LED
const int PIN_BATT_LED_3 = 2;   // D2  - 60% LED
const int PIN_BATT_LED_4 = 4;   // D4  - 80% LED
const int PIN_BATT_LED_5 = 16;  // RX2 - 100% LED (using GPIO16 as RX2 is not available on all ESP32)
const int PINS_BATT_LED[] = {PIN_BATT_LED_1, PIN_BATT_LED_2, PIN_BATT_LED_3, PIN_BATT_LED_4, PIN_BATT_LED_5};

// Throttle State LEDs (D18, D5, TX2)
const int PIN_THROTTLE_LED_FWD = 18;  // D18 - Forward LED
const int PIN_THROTTLE_LED_REV = 5;    // D5  - Reverse LED
const int PIN_THROTTLE_LED_NEUT = 17;  // TX2 - Neutral LED (using GPIO17)
const int PINS_THROTTLE_LED[] = {PIN_THROTTLE_LED_FWD, PIN_THROTTLE_LED_REV, PIN_THROTTLE_LED_NEUT};

// ============================================================
// CALIBRATION CONSTANTS - 72V, 3kW BLDC Powertrain
// ============================================================

// ACS758LCB-050B Current Sensor Specifications:
// - Sensitivity: 40mV/A (0.040 V/A)
// - Quiescent voltage: 0.5 × VCC = 0.5 × 3.3V = 1.65V at zero current
// - VCC: 3.3V (ESP32 ADC reference)
const float CURRENT_SENSITIVITY = 0.040;  // V/A for ACS758LCB-050B
const float VCC = 3.3;                    // Supply voltage
const float QUIESCENT_VOLTAGE = 0.5 * VCC; // 1.65V at zero current

// Voltage Divider Ratio (MCP6022 voltage follower output)
// Assuming 100K/10K divider for 72V max input
const float VOLTAGE_DIVIDER_RATIO = 11.0;  // (100K + 10K) / 10K

// ADC Parameters
const float V_REF = 3.3;
const int ADC_MAX = 4095;

// Nominal Powertrain Values
const float NOMINAL_VOLTAGE = 72.0;    // V
const float NOMINAL_POWER = 3000.0;   // W (3kW)
const float NOMINAL_CURRENT = NOMINAL_POWER / NOMINAL_VOLTAGE;  // ~41.67A

// ============================================================
// MOVING AVERAGE FILTER CONFIGURATION
// ============================================================
const int FILTER_WINDOW_SIZE = 20;  // Number of samples for moving average
float voltageFilterBuffer[FILTER_WINDOW_SIZE];
float currentFilterBuffer[FILTER_WINDOW_SIZE];
int filterIndex = 0;
bool filterInitialized = false;

// Zero-current offset calibration
float currentOffset = 0.0;
bool calibrationComplete = false;
unsigned long calibrationStartTime = 0;
const unsigned long CALIBRATION_DURATION = 2000; // 2 seconds for calibration

// Data Logging
struct DataPoint {
  unsigned long timestamp;
  float voltage;
  float current;
  float power;
  int throttleState; 
};

const int MAX_RAM_POINTS = 60; 
DataPoint ramBuffer[MAX_RAM_POINTS];
int bufferIndex = 0;
unsigned long lastCaptureTime = 0;
unsigned long lastSaveTime = 0;
unsigned long lastPowerTime = 0;
bool isLogging = true;
String currentAccessoryName = "Unknown";
String currentProfileFile = "";
int nextProfileNum = 1;

// Real-time power calculation variables
float realTimeVoltage = 0.0;
float realTimeCurrent = 0.0;
float realTimePower = 0.0;
int currentThrottleState = 1; // 0=Reverse, 1=Neutral, 2=Forward
int currentBatteryPercent = 0;

// JSON Serial Output timing
unsigned long lastJsonOutput = 0;
const unsigned long JSON_OUTPUT_INTERVAL = 1000; // 1 second

// Web Server & DNS
WebServer server(80);
DNSServer dnsServer;

// --- HTML Content ---
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Kilikawi Smart Dashboard</title>
    <style>
        :root { --primary: #27ae60; --dark: #1e272e; --light: #f1f2f6; --danger: #eb4d4b; --warning: #f0932b; --accent: #2980b9; }
        body { font-family: 'Segoe UI', sans-serif; background: var(--light); margin: 0; }
        .logo-container { display: flex; align-items: center; gap: 8px; }
        .logo-img { width: 24px; height: 24px; }
        .login-container { max-width: 400px; margin: 40px auto; padding: 20px; background: white; border-radius: 10px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); }
        .dashboard-container { display: none; padding: 20px; }
        .header { display: flex; justify-content: space-between; align-items: center; background: var(--dark); color: white; padding: 10px 20px; }
        .card { background: white; padding: 20px; margin: 10px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.05); }
        .gauges { display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 15px; }
        .gauge-item { text-align: center; border-left: 3px solid var(--primary); padding-left: 10px; }
        .gauge-item:first-child { border-left-color: var(--accent); }
        .gauge-item:nth-child(2) { border-left-color: var(--warning); }
        .gauge-value { font-size: 24px; font-weight: bold; color: var(--dark); }
        .gauge-label { color: #666; font-size: 14px; text-transform: uppercase; letter-spacing: 1px; }
        .chart-container { height: 400px; }
        select, input, button { width: 100%; padding: 12px; margin: 10px 0; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box; font-size: 16px; }
        button { background: var(--primary); color: white; border: none; cursor: pointer; font-weight: bold; transition: background 0.2s; }
        button:active { background: #1e8449; }
        .btn-admin { background: var(--accent); }
        .btn-reset { background: var(--danger); }
        .throttle-indicator { padding: 5px 12px; border-radius: 20px; color: white; font-size: 12px; font-weight: bold; }
        .throttle-0 { background: var(--danger); }
        .throttle-1 { background: #7f8c8d; }
        .throttle-2 { background: var(--primary); }
        .status-msg { font-size: 12px; color: #888; text-align: center; }
        .role-selector { display: flex; gap: 10px; margin-bottom: 15px; }
        .role-selector label { flex: 1; text-align: center; cursor: pointer; padding: 10px; border: 1px solid #ddd; border-radius: 4px; font-weight: bold; }
        .role-selector input:checked + label { background: var(--primary); color: white; border-color: var(--primary); }
        .role-selector input { display: none; }
        #debugLog { position: fixed; bottom: 0; left: 0; right: 0; background: rgba(0,0,0,0.8); color: #0f0; font-family: monospace; font-size: 10px; max-height: 100px; overflow-y: auto; padding: 5px; pointer-events: none; z-index: 10000; display: none; }
    </style>
</head>
<body>
<div id="loginPage" class="login-container">
    <div style="text-align:center; margin-bottom: 20px;">
        <svg class="logo-img" viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg" style="width:48px; height:48px; margin:auto; display:block">
            <path d="M12 2L2 7L12 12L22 7L12 2Z" fill="#27ae60"/>
            <path d="M2 17L12 22L22 17" stroke="#27ae60" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
            <path d="M2 12L12 17L22 12" stroke="#27ae60" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
        </svg>
        <h2 style="margin:10px 0; color:var(--dark)">Kilikawi Smart Tool</h2>
    </div>
    <div class="role-selector">
        <input type="radio" id="roleFarmer" name="role" value="farmer" checked onchange="toggleAdminField()">
        <label for="roleFarmer">Farmer</label>
        <input type="radio" id="roleAdmin" name="role" value="admin" onchange="toggleAdminField()">
        <label for="roleAdmin">Admin</label>
    </div>
    <div id="adminField" style="display:none">
        <input type="password" id="password" placeholder="Admin Password" value="">
    </div>
    <button type="button" id="btnEnter" onclick="enterApp()" ontouchstart="enterApp()" style="z-index: 9999; position: relative;">Enter Dashboard</button>
</div>
<div id="dashboardPage" class="dashboard-container">
    <div class="header">
        <div class="logo-container">
            <svg class="logo-img" viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg">
                <path d="M12 2L2 7L12 12L22 7L12 2Z" fill="#2ecc71"/>
                <path d="M2 17L12 22L22 17" stroke="#2ecc71" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
                <path d="M2 12L17L22 12" stroke="#2ecc71" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
            </svg>
            <h3 id="accessoryTitle" style="margin:0">Detecting...</h3>
        </div>
        <button type="button" onclick="logout()" style="width:auto; margin:0; padding:5px 15px; background:var(--danger)">Logout</button>
    </div>
    <div class="card gauges">
        <div class="gauge-item"><div id="vValue" class="gauge-value">0.0V</div><div class="gauge-label">Voltage</div></div>
        <div class="gauge-item"><div id="iValue" class="gauge-value">0.0A</div><div class="gauge-label">Current</div></div>
        <div class="gauge-item"><div id="pValue" class="gauge-value">0.0W</div><div class="gauge-label">Power</div></div>
        <div class="gauge-item"><div id="throttleState" class="throttle-indicator throttle-1">Neutral</div><div class="gauge-label">Throttle</div></div>
        <div class="gauge-item"><div id="battValue" class="gauge-value">0%</div><div class="gauge-label">Battery</div></div>
    </div>
    <div class="card">
        <label>Historical Profiles:</label>
        <select id="profileSelect" onchange="loadProfile()"><option value="live">Live Data</option></select>
        <p id="loggingStatus" class="status-msg">Logging active...</p>
        <div id="adminTools" style="display:none; margin-top: 15px; border-top: 1px solid #eee; padding-top: 15px;">
            <button type="button" class="btn-admin" onclick="downloadLastCSV()">Download Last Profile (CSV)</button>
            <button type="button" class="btn-reset" onclick="resetProfiles()">Reset All Profiles</button>
        </div>
    </div>
    <div class="card chart-container"><canvas id="mainChart"></canvas></div>
</div>
<div id="debugLog"></div>
<script>
    // Confirm script execution immediately
    document.addEventListener("DOMContentLoaded", function() {
        var logEl = document.getElementById('debugLog');
        if (logEl) {
            logEl.style.display = 'block';
            logEl.innerHTML += '<div>[SYSTEM] App Ready</div>';
        }
    });

    function log(m) {
        var logEl = document.getElementById('debugLog');
        if (logEl) {
            logEl.style.display = 'block';
            logEl.innerHTML += '<div>[' + new Date().toLocaleTimeString() + '] ' + m + '</div>';
            logEl.scrollTop = logEl.scrollHeight;
        }
        console.log(m);
    }

    window.onerror = function(msg, url, line) {
        log("ERROR: " + msg + " at line " + line);
        alert("System Error: " + msg + "\nLine: " + line);
        return false;
    };

    var chart = null;
    var isLive = true;

    function toggleAdminField() {
        var field = document.getElementById('adminField');
        var admin = document.getElementById('roleAdmin');
        if (field && admin) {
            field.style.display = admin.checked ? 'block' : 'none';
        }
    }

    var enterAppTriggered = false;
    function enterApp() {
        if (enterAppTriggered) return; // Prevent double trigger
        enterAppTriggered = true;
        
        log("Button Triggered");
        try {
            var roleAdmin = document.getElementById('roleAdmin');
            var isAdmin = roleAdmin && roleAdmin.checked;
            
            if (isAdmin) {
                var passEl = document.getElementById('password');
                if (passEl) {
                    var pass = passEl.value;
                    if (pass !== 'kilikawi') {
                        alert("Incorrect Admin Password");
                        enterAppTriggered = false; // Reset so they can try again
                        return;
                    }
                }
            }
            
            log("Opening Dashboard...");
            var login = document.getElementById('loginPage');
            var dash = document.getElementById('dashboardPage');
            var adminTools = document.getElementById('adminTools');

            if (login && dash) {
                login.style.display = 'none';
                dash.style.display = 'block';
                if (adminTools) adminTools.style.display = isAdmin ? 'block' : 'none';
                log("Success");
                
                // Initialize Chart.js ONLY after the UI is visible
                var script = document.createElement('script');
                script.src = '/chart.min.js';
                script.onload = function() {
                    log("Chart.js Loaded");
                    initDashboard();
                };
                script.onerror = function() {
                    log("Chart.js Failed");
                    initDashboard(); // Still init dashboard, just no graph
                };
                document.head.appendChild(script);
                
            } else {
                log("Error: HTML missing");
            }
        } catch(e) {
            log("Critical: " + e.message);
            alert("App Error: " + e.message);
        }
    }

    function logout() { location.reload(); }

    function ajaxGet(url, callback) {
        var xhr = new XMLHttpRequest();
        xhr.onreadystatechange = function() {
            if (xhr.readyState == 4) {
                if (xhr.status == 200) {
                    callback(xhr.responseText);
                } else {
                    log("AJAX Error: " + url + " (" + xhr.status + ")");
                }
            }
        };
        xhr.onerror = function() { log("Network Error: " + url); };
        xhr.open("GET", url, true);
        xhr.send();
    }

    function initDashboard() {
        log("Connecting Data...");
        var ctx = document.getElementById('mainChart');
        if (!ctx) {
            log("Error: Canvas missing");
            return;
        }
        
        if (typeof Chart === 'undefined') {
            log("Warning: No Chart.js");
        } else {
            try {
                chart = new Chart(ctx.getContext('2d'), {
                    type: 'line',
                    data: { labels: [], datasets: [
                        { label: 'Voltage (V)', data: [], borderColor: '#3498db', yAxisID: 'y' },
                        { label: 'Current (A)', data: [], borderColor: '#e67e22', yAxisID: 'y1' },
                        { label: 'Power (W)', data: [], borderColor: '#2ecc71', yAxisID: 'y' },
                        { label: 'Throttle', data: [], backgroundColor: 'rgba(0,0,0,0.1)', fill: true, stepped: true, yAxisID: 'y2', pointRadius: 0 }
                    ]},
                    options: { responsive: true, maintainAspectRatio: false, scales: { 
                        y: { type: 'linear', position: 'left' }, 
                        y1: { type: 'linear', position: 'right', grid: { drawOnChartArea: false } },
                        y2: { display: false, min: 0, max: 2 }
                    }}
                });
                log("Graph Created");
            } catch(e) {
                log("Chart.js Error: " + e.message);
            }
        }
        
        fetchProfiles();
        
        setInterval(function() {
            if (isLive) {
                ajaxGet('/api/live', function(res) {
                    try {
                        var data = JSON.parse(res);
                        updateUI(data);
                        if (chart) {
                            if (chart.data.labels.length > 20) { 
                                chart.data.labels.shift(); 
                                for(var i=0; i<chart.data.datasets.length; i++) chart.data.datasets[i].data.shift();
                            }
                            chart.data.labels.push(new Date().toLocaleTimeString());
                            chart.data.datasets[0].data.push(data.v); 
                            chart.data.datasets[1].data.push(data.i);
                            chart.data.datasets[2].data.push(data.p); 
                            chart.data.datasets[3].data.push(data.throttle);
                            chart.update(0);
                        }
                    } catch(e) {}
                });
            }
        }, 1000);
    }

    function updateUI(data) {
        document.getElementById('accessoryTitle').innerText = "Accessory: " + data.accessory;
        document.getElementById('vValue').innerText = data.v.toFixed(1) + "V";
        document.getElementById('iValue').innerText = data.i.toFixed(2) + "A";
        document.getElementById('pValue').innerText = data.p.toFixed(1) + "W";
        var states = ["Reverse", "Neutral", "Forward"];
        document.getElementById('throttleState').innerText = states[data.throttle];
        document.getElementById('throttleState').className = "throttle-indicator throttle-" + data.throttle;
        document.getElementById('loggingStatus').innerText = data.logging ? "Logging active..." : "Logging stopped (Zero Power)";
        if (data.battery_percent !== undefined) {
            document.getElementById('battValue').innerText = data.battery_percent + "%";
        }
    }

    function fetchProfiles() {
        ajaxGet('/api/profiles', function(res) {
            var files = JSON.parse(res);
            var select = document.getElementById('profileSelect');
            while(select.options.length > 1) select.remove(1);
            for(var i=0; i<files.length; i++) {
                var opt = document.createElement('option');
                opt.value = files[i];
                opt.innerText = files[i];
                select.appendChild(opt);
            }
        });
    }

    function loadProfile() {
        var val = document.getElementById('profileSelect').value;
        if(val === "live") {
            isLive = true;
            if (chart) {
                chart.data.labels = [];
                for(var i=0; i<chart.data.datasets.length; i++) chart.data.datasets[i].data = [];
                chart.update();
            }
        } else {
            isLive = false;
            ajaxGet('/api/history?file=' + val, function(text) {
                var lines = text.split('\n');
                var data = [];
                for(var i=0; i<lines.length; i++) {
                    if(lines[i].trim()) data.push(JSON.parse(lines[i]));
                }
                if (data.length > 0) document.getElementById('accessoryTitle').innerText = "Accessory: " + (data[0].acc || "Unknown");
                if (chart) {
                    chart.data.labels = [];
                    chart.data.datasets[0].data = [];
                    chart.data.datasets[1].data = [];
                    chart.data.datasets[2].data = [];
                    chart.data.datasets[3].data = [];
                    for(var j=0; j<data.length; j++) {
                        chart.data.labels.push(new Date(data[j].t).toLocaleTimeString());
                        chart.data.datasets[0].data.push(data[j].v);
                        chart.data.datasets[1].data.push(data[j].i);
                        chart.data.datasets[2].data.push(data[j].p);
                        chart.data.datasets[3].data.push(data[j].th);
                    }
                    chart.update();
                }
            });
        }
    }

    function downloadLastCSV() {
        ajaxGet('/api/profiles', function(res) {
            var files = JSON.parse(res);
            if(files.length > 0) window.location.href = '/api/download?file=' + files[files.length-1];
        });
    }

    function resetProfiles() {
        if(confirm("Reset all?")) {
            var xhr = new XMLHttpRequest();
            xhr.open("POST", "/api/reset", true);
            xhr.onreadystatechange = function() {
                if(xhr.readyState == 4) { fetchProfiles(); loadProfile(); }
            };
            xhr.send();
        }
    }
</script>
</body>
</html>
)rawliteral";

// --- Logic Functions ---

// ============================================================
// BOOT-UP LED ANIMATION
// ============================================================
void runBootUpAnimation() {
  Serial.println("Running boot-up LED animation...");
  
  // Turn off all LEDs first
  for (int i = 0; i < 4; i++) digitalWrite(PINS_ACCESSORY_LED[i], LOW);
  for (int i = 0; i < 5; i++) digitalWrite(PINS_BATT_LED[i], LOW);
  for (int i = 0; i < 3; i++) digitalWrite(PINS_THROTTLE_LED[i], LOW);
  
  delay(200);
  
  // Phase 1: Accessory LEDs from left to right (D35, D32, D33, D26)
  Serial.println("Phase 1: Accessory LEDs left to right");
  for (int i = 0; i < 4; i++) {
    digitalWrite(PINS_ACCESSORY_LED[i], HIGH);
    delay(100);
  }
  delay(200);
  
  // Phase 2: Throttle LEDs from left to right (D18, D5, TX2)
  Serial.println("Phase 2: Throttle LEDs left to right");
  for (int i = 0; i < 3; i++) {
    digitalWrite(PINS_THROTTLE_LED[i], HIGH);
    delay(100);
  }
  delay(200);
  
  // Phase 3: Battery LEDs from 0% to 100% (20%, 40%, 60%, 80%, 100%)
  Serial.println("Phase 3: Battery LEDs 0% to 100%");
  for (int i = 0; i < 5; i++) {
    digitalWrite(PINS_BATT_LED[i], HIGH);
    delay(100);
  }
  delay(300);
  
  // Phase 4: Battery LEDs from 100% to 0%
  Serial.println("Phase 4: Battery LEDs 100% to 0%");
  for (int i = 4; i >= 0; i--) {
    digitalWrite(PINS_BATT_LED[i], LOW);
    delay(100);
  }
  delay(200);
  
  // Phase 5: Throttle LEDs from right to left
  Serial.println("Phase 5: Throttle LEDs right to left");
  for (int i = 2; i >= 0; i--) {
    digitalWrite(PINS_THROTTLE_LED[i], LOW);
    delay(100);
  }
  delay(200);
  
  // Phase 6: Accessory LEDs from right to left
  Serial.println("Phase 6: Accessory LEDs right to left");
  for (int i = 3; i >= 0; i--) {
    digitalWrite(PINS_ACCESSORY_LED[i], LOW);
    delay(100);
  }
  delay(200);
  
  Serial.println("Boot-up animation complete!");
}
void performZeroCurrentCalibration() {
  Serial.println("\n=== Starting Zero-Current Offset Calibration ===");
  Serial.println("Please ensure no current is flowing through the sensor...");
  
  calibrationStartTime = millis();
  float sum = 0;
  int samples = 0;
  
  // Collect samples for 2 seconds
  while (millis() - calibrationStartTime < CALIBRATION_DURATION) {
    int rawADC = analogRead(PIN_CURRENT_SENSOR);
    float voltage = rawADC * (V_REF / ADC_MAX);
    sum += voltage;
    samples++;
    delay(10);
  }
  
  // Calculate offset as average quiescent voltage
  currentOffset = sum / samples;
  calibrationComplete = true;
  
  Serial.print("Calibration complete. Offset voltage: ");
  Serial.print(currentOffset, 4);
  Serial.println("V");
  Serial.print("Samples collected: ");
  Serial.println(samples);
  Serial.println("=================================================\n");
}

// ============================================================
// MOVING AVERAGE FILTER
// ============================================================
float applyMovingAverage(float newValue, float* buffer, int bufferSize) {
  // Shift all values and add new value
  for (int i = bufferSize - 1; i > 0; i--) {
    buffer[i] = buffer[i - 1];
  }
  buffer[0] = newValue;
  
  // Calculate average
  float sum = 0;
  for (int i = 0; i < bufferSize; i++) {
    sum += buffer[i];
  }
  return sum / bufferSize;
}

float filterVoltage(float rawVoltage) {
  return applyMovingAverage(rawVoltage, voltageFilterBuffer, FILTER_WINDOW_SIZE);
}

float filterCurrent(float rawCurrent) {
  return applyMovingAverage(rawCurrent, currentFilterBuffer, FILTER_WINDOW_SIZE);
}

// ============================================================
// REAL-TIME POWER CALCULATION (72V, 3kW BLDC)
// ============================================================
void calculatePowerMetrics() {
  // Read raw ADC values with oversampling for noise reduction
  long voltageSum = 0;
  long currentSum = 0;
  
  for (int i = 0; i < 50; i++) {
    voltageSum += analogRead(PIN_VOLTAGE_SENSOR);
    currentSum += analogRead(PIN_CURRENT_SENSOR);
    delayMicroseconds(100);
  }
  
  // Calculate average ADC values
  float avgVoltageADC = voltageSum / 50.0;
  float avgCurrentADC = currentSum / 50.0;
  
  // Convert ADC to voltage
  float rawVoltage = avgVoltageADC * (V_REF / ADC_MAX);
  float rawCurrentSensor = avgCurrentADC * (V_REF / ADC_MAX);
  
  // Apply voltage divider ratio
  realTimeVoltage = rawVoltage * VOLTAGE_DIVIDER_RATIO;
  
  // Apply current sensor calibration with offset
  // I = (V_sensor - V_offset - V_quiescent) / Sensitivity
  float adjustedSensorVoltage = rawCurrentSensor - currentOffset - QUIESCENT_VOLTAGE;
  realTimeCurrent = adjustedSensorVoltage / CURRENT_SENSITIVITY;
  
  // Apply deadband to filter noise (below 0.5A considered zero)
  if (abs(realTimeCurrent) < 0.5) {
    realTimeCurrent = 0;
  }
  
  // Calculate real-time power: P = V × I
  realTimePower = realTimeVoltage * realTimeCurrent;
  
  // Apply moving average filter
  realTimeVoltage = filterVoltage(realTimeVoltage);
  realTimeCurrent = filterCurrent(realTimeCurrent);
  realTimePower = realTimeVoltage * realTimeCurrent;
}

// ============================================================
// ACCESSORY DETECTION WITH LED INDICATORS
// ============================================================
String getAccessoryName() {
  int code = 0;
  for (int i = 0; i < 4; i++) {
    if (digitalRead(PINS_INDUCTIVE[i]) == LOW) {
      delay(5); 
      if (digitalRead(PINS_INDUCTIVE[i]) == LOW) code |= (1 << i);
    }
  }
  
  // Update accessory indicator LEDs
  // Binary combinations: 0000=Tiller, 0001=Sheller, 0010=Planter, 0100=Mower
  // Map to: D35=Tiller, D32=Sheller, D33=Planter, D26=Mower
  digitalWrite(PINS_ACCESSORY_LED[0], (code == 0) ? HIGH : LOW);   // D35 - Tiller (0000)
  digitalWrite(PINS_ACCESSORY_LED[1], (code == 1) ? HIGH : LOW);  // D32 - Sheller (0001)
  digitalWrite(PINS_ACCESSORY_LED[2], (code == 2) ? HIGH : LOW);  // D33 - Planter (0010)
  digitalWrite(PINS_ACCESSORY_LED[3], (code == 4) ? HIGH : LOW);  // D26 - Mower (0100)
  
  switch (code) {
    case 0b0000: return "Tiller";
    case 0b0001: return "Sheller";
    case 0b0010: return "Planter";
    case 0b0100: return "Mower";
    default: return "No Tool";
  }
}

// ============================================================
// THROTTLE SWITCH READING
// ============================================================
int getThrottleState() {
  // D21 = Forward, D22 = Reverse, D23 = Neutral
  bool fwd = digitalRead(PIN_THROTTLE_FWD) == LOW;
  bool rev = digitalRead(PIN_THROTTLE_REV) == LOW;
  bool neut = digitalRead(PIN_THROTTLE_NEUT) == LOW;
  
  // Update throttle state LEDs
  digitalWrite(PIN_THROTTLE_LED_FWD, fwd ? HIGH : LOW);
  digitalWrite(PIN_THROTTLE_LED_REV, rev ? HIGH : LOW);
  digitalWrite(PIN_THROTTLE_LED_NEUT, neut ? HIGH : LOW);
  
  if (fwd) return 2;  // Forward
  if (rev) return 0;  // Reverse
  return 1;           // Neutral
}

// ============================================================
// BATTERY PERCENTAGE LEDs
// ============================================================
void updateBatteryLEDs(float voltage) {
  // 72V system - typical range is 54V (empty) to 84.6V (full, 14.1V per cell × 6 cells)
  // Calculate percentage based on voltage
  const float MIN_VOLTAGE = 54.0;  // Empty battery
  const float MAX_VOLTAGE = 84.6;  // Full battery
  
  float percent = ((voltage - MIN_VOLTAGE) / (MAX_VOLTAGE - MIN_VOLTAGE)) * 100.0;
  percent = constrain(percent, 0, 100);
  currentBatteryPercent = (int)percent;
  
  // Update LEDs based on percentage thresholds
  // 20%, 40%, 60%, 80%, 100%
  digitalWrite(PIN_BATT_LED_1, (percent >= 20) ? HIGH : LOW);  // 20%
  digitalWrite(PIN_BATT_LED_2, (percent >= 40) ? HIGH : LOW);  // 40%
  digitalWrite(PIN_BATT_LED_3, (percent >= 60) ? HIGH : LOW);  // 60%
  digitalWrite(PIN_BATT_LED_4, (percent >= 80) ? HIGH : LOW);  // 80%
  digitalWrite(PIN_BATT_LED_5, (percent >= 100) ? HIGH : LOW); // 100%
}

// ============================================================
// JSON SERIAL OUTPUT
// ============================================================
void sendJsonToSerial() {
  unsigned long now = millis();
  if (now - lastJsonOutput >= JSON_OUTPUT_INTERVAL) {
    lastJsonOutput = now;
    
    JsonDocument doc;
    doc["timestamp"] = now;
    doc["accessory"] = currentAccessoryName;
    doc["current"] = realTimeCurrent;
    doc["voltage"] = realTimeVoltage;
    doc["power"] = realTimePower;
    doc["throttle"] = currentThrottleState;
    doc["battery_percent"] = currentBatteryPercent;
    
    String jsonOutput;
    serializeJson(doc, jsonOutput);
    Serial.println(jsonOutput);
  }
}

float readVoltage() {
  // Legacy function - now using calculatePowerMetrics()
  return realTimeVoltage;
}

float readCurrent() {
  // Legacy function - now using calculatePowerMetrics()
  return realTimeCurrent;
}

void downsampleFile(String path) {
  String tempPath = "/temp.json";
  File oldFile = LittleFS.open(path, "r");
  File newFile = LittleFS.open(tempPath, "w");
  if (!oldFile || !newFile) return;
  while (oldFile.available()) {
    String line1 = oldFile.readStringUntil('\n');
    if (!oldFile.available()) { newFile.println(line1); break; }
    String line2 = oldFile.readStringUntil('\n');
    JsonDocument doc1, doc2, merged;
    deserializeJson(doc1, line1); deserializeJson(doc2, line2);
    merged["t"] = doc2["t"];
    merged["v"] = (doc1["v"].as<float>() + doc2["v"].as<float>()) / 2.0;
    merged["i"] = (doc1["i"].as<float>() + doc2["i"].as<float>()) / 2.0;
    merged["p"] = (doc1["p"].as<float>() + doc2["p"].as<float>()) / 2.0;
    merged["th"] = doc2["th"]; merged["acc"] = doc1["acc"];
    serializeJson(merged, newFile); newFile.println();
  }
  oldFile.close(); newFile.close();
  LittleFS.remove(path); LittleFS.rename(tempPath, path);
}

void createSampleProfile() {
  File f = LittleFS.open("/profile_0_SAMPLE.json", "w");
  if (f) {
    const char* sampleTool = "Tiller";
    for (int i = 0; i < 20; i++) {
      JsonDocument doc;
      doc["t"] = millis() + (i * 500);
      doc["v"] = 12.5 + (random(-5, 5) / 10.0);
      doc["i"] = 2.0 + (random(-2, 2) / 10.0);
      doc["p"] = doc["v"].as<float>() * doc["i"].as<float>();
      doc["th"] = (i < 5) ? 1 : (i < 15 ? 2 : 1); // Neutral -> Forward -> Neutral
      doc["acc"] = sampleTool;
      serializeJson(doc, f);
      f.println();
    }
    f.close();
    Serial.println("Sample profile created.");
  }
}

void manageFiles() {
  File root = LittleFS.open("/");
  File file = root.openNextFile();
  String oldestFile = "";
  int count = 0, maxNum = 0;
  bool hasAnyProfile = false;

  while (file) {
    String name = String(file.name());
    if (name.startsWith("/profile_")) {
      hasAnyProfile = true;
      count++;
      int num = name.substring(9, name.indexOf(".json")).toInt();
      if (num > maxNum) maxNum = num;
      if (oldestFile == "" || name < oldestFile) oldestFile = name;
    }
    file = root.openNextFile();
  }

  if (!hasAnyProfile) {
    createSampleProfile();
  }

  if (count >= 50) {
    File oldest = LittleFS.open(oldestFile, "r");
    if (oldest.size() > 500) { oldest.close(); downsampleFile(oldestFile); }
    else { oldest.close(); LittleFS.remove(oldestFile); }
  }
  nextProfileNum = (maxNum == 0) ? 1 : maxNum + 1;
  currentProfileFile = "/profile_" + String(nextProfileNum) + ".json";
}

void saveToFS() {
  if (bufferIndex == 0 || !isLogging) return;
  File file = LittleFS.open(currentProfileFile, FILE_APPEND);
  if (file) {
    for (int i = 0; i < bufferIndex; i++) {
      JsonDocument doc;
      doc["t"] = ramBuffer[i].timestamp; 
      doc["v"] = ramBuffer[i].voltage;
      doc["i"] = ramBuffer[i].current; 
      doc["p"] = ramBuffer[i].power;
      doc["th"] = ramBuffer[i].throttleState; 
      doc["acc"] = currentAccessoryName;
      doc["bp"] = currentBatteryPercent;
      serializeJson(doc, file); 
      file.println();
    }
    file.close();
  }
  bufferIndex = 0;
}

// --- Setup & Loop ---

void setup() {
  Serial.begin(115200);
  delay(2000); 
  Serial.println("\n--- Kilikawi Dashboard Booting ---");
  Serial.println("72V, 3kW BLDC Powertrain Calibration System");
  Serial.println("==========================================\n");
  
  // ============================================================
  // INITIALIZE INPUT PINS
  // ============================================================
  
  // Inductive sensor inputs (with pull-up)
  for (int i = 0; i < 4; i++) {
    pinMode(PINS_INDUCTIVE[i], INPUT_PULLUP);
  }
  
  // Throttle switch inputs (with pull-up)
  pinMode(PIN_THROTTLE_FWD, INPUT_PULLUP);
  pinMode(PIN_THROTTLE_REV, INPUT_PULLUP);
  pinMode(PIN_THROTTLE_NEUT, INPUT_PULLUP);
  
  // Analog inputs for voltage and current sensors
  pinMode(PIN_VOLTAGE_SENSOR, INPUT);
  pinMode(PIN_CURRENT_SENSOR, INPUT);
  
  // ============================================================
  // INITIALIZE OUTPUT PINS
  // ============================================================
  
  // Accessory indicator LEDs
  for (int i = 0; i < 4; i++) {
    pinMode(PINS_ACCESSORY_LED[i], OUTPUT);
    digitalWrite(PINS_ACCESSORY_LED[i], LOW);
  }
  
  // Battery percentage LEDs
  for (int i = 0; i < 5; i++) {
    pinMode(PINS_BATT_LED[i], OUTPUT);
    digitalWrite(PINS_BATT_LED[i], LOW);
  }
  
  // Throttle state LEDs
  pinMode(PIN_THROTTLE_LED_FWD, OUTPUT);
  pinMode(PIN_THROTTLE_LED_REV, OUTPUT);
  pinMode(PIN_THROTTLE_LED_NEUT, OUTPUT);
  digitalWrite(PIN_THROTTLE_LED_FWD, LOW);
  digitalWrite(PIN_THROTTLE_LED_REV, LOW);
  digitalWrite(PIN_THROTTLE_LED_NEUT, HIGH); // Default to neutral
  
  // ============================================================
  // RUN BOOT-UP LED ANIMATION
  // ============================================================
  runBootUpAnimation();
  
  // ============================================================
  // INITIALIZE FILTER BUFFERS
  // ============================================================
  for (int i = 0; i < FILTER_WINDOW_SIZE; i++) {
    voltageFilterBuffer[i] = 0.0;
    currentFilterBuffer[i] = 0.0;
  }
  
  // ============================================================
  // PERFORM ZERO-CURRENT OFFSET CALIBRATION
  // ============================================================
  performZeroCurrentCalibration();
  
  // ============================================================
  // WiFi AND FILE SYSTEM SETUP
  // ============================================================
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(AP_SSID, AP_PASS);
  
  if (!LittleFS.begin(true)) Serial.println("LittleFS Fail");
  else manageFiles();

  // ============================================================
  // WEB SERVER ENDPOINTS
  // ============================================================
  server.on("/", []() { server.send_P(200, "text/html", INDEX_HTML); });
  server.on("/chart.min.js", []() {
    if (LittleFS.exists("/chart.min.js")) {
      File f = LittleFS.open("/chart.min.js", "r");
      server.streamFile(f, "application/javascript");
      f.close();
    } else server.send(404, "text/plain", "Missing Chart.js");
  });
  server.on("/api/live", []() {
    JsonDocument doc;
    doc["accessory"] = currentAccessoryName; 
    doc["v"] = realTimeVoltage; 
    doc["i"] = realTimeCurrent; 
    doc["p"] = realTimePower;
    doc["throttle"] = currentThrottleState; 
    doc["logging"] = isLogging;
    doc["battery_percent"] = currentBatteryPercent;
    String res; serializeJson(doc, res);
    server.send(200, "application/json", res);
  });
  server.on("/api/profiles", []() {
    JsonDocument doc; JsonArray arr = doc.to<JsonArray>();
    File root = LittleFS.open("/"); File f = root.openNextFile();
    while(f) { if (String(f.name()).startsWith("/profile_")) arr.add(f.name()); f = root.openNextFile(); }
    String res; serializeJson(doc, res);
    server.send(200, "application/json", res);
  });
  server.on("/api/history", []() {
    String fName = server.arg("file");
    if (LittleFS.exists(fName)) {
      File f = LittleFS.open(fName, "r");
      server.streamFile(f, "text/plain");
      f.close();
    } else server.send(404);
  });
  server.on("/api/reset", HTTP_POST, []() {
    File root = LittleFS.open("/"); File f = root.openNextFile();
    while(f) { String n = String(f.name()); if (n.startsWith("/profile_")) LittleFS.remove(n); f = root.openNextFile(); }
    createSampleProfile(); // Re-create sample profile after reset
    nextProfileNum = 1; currentProfileFile = "/profile_1.json";
    server.send(200);
  });
  server.on("/api/download", []() {
    String fName = server.arg("file");
    if (!LittleFS.exists(fName)) { server.send(404); return; }
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.sendHeader("Content-Type", "text/csv");
    server.sendHeader("Content-Disposition", "attachment; filename=profile.csv");
    server.send(200, "text/csv", "");
    server.sendContent("Timestamp,Voltage(V),Current(A),Power(W),ThrottleState,Accessory,Battery%\n");
    File f = LittleFS.open(fName, "r");
    while (f.available()) {
      String l = f.readStringUntil('\n');
      if (l.length() > 0) {
        JsonDocument d; deserializeJson(d, l);
        char buf[128];
        sprintf(buf, "%lu,%.2f,%.2f,%.2f,%d,%s,%d%%\n", d["t"].as<unsigned long>(), d["v"].as<float>(), d["i"].as<float>(), d["p"].as<float>(), d["th"].as<int>(), d["acc"].as<const char*>(), d["bp"].as<int>());
        server.sendContent(buf);
      }
    }
    f.close();
  });
  server.onNotFound([]() { server.sendHeader("Location", "http://192.168.4.1/"); server.send(302); });

  server.begin();
  dnsServer.start(53, "*", AP_IP);
  Serial.println("\nSystem Ready.");
  Serial.println("Access Point: Kilikawi-Smart-Tool");
  Serial.println("Password: kilikawi2026");
  Serial.println("Dashboard: http://192.168.4.1");
  Serial.println("\nJSON Serial Output enabled at 1-second intervals");
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  
  unsigned long now = millis();
  if (now - lastCaptureTime >= 500) {
    lastCaptureTime = now;
    
    // Calculate real-time power metrics (72V, 3kW BLDC)
    calculatePowerMetrics();
    
    // Update accessory detection with LED indicators
    currentAccessoryName = getAccessoryName();
    
    // Update throttle state
    currentThrottleState = getThrottleState();
    
    // Update battery percentage LEDs
    updateBatteryLEDs(realTimeVoltage);
    
    // Power-based logging control
    if (realTimePower > 1.0) { 
      lastPowerTime = now; 
      isLogging = true; 
    }
    else if (now - lastPowerTime > 300000) isLogging = false;

    // Buffer data for logging
    if (isLogging && bufferIndex < MAX_RAM_POINTS) {
      ramBuffer[bufferIndex] = {now, realTimeVoltage, realTimeCurrent, realTimePower, currentThrottleState};
      bufferIndex++;
    }
    
    // Send JSON data to Serial0 for dashboard
    sendJsonToSerial();
  }
  
  // Save to filesystem every 30 seconds
  if (now - lastSaveTime >= 30000) { 
    lastSaveTime = now; 
    saveToFS(); 
  }
  
  delay(1);
}

