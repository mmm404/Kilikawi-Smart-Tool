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

// Pin Assignments
const int PIN_VOLTAGE = 34;
const int PIN_CURRENT = 35;
const int PIN_THROTTLE = 32;
const int PINS_ACCESSORY[] = {13, 12, 14, 27}; 

// Calibration Constants
const float VOLTAGE_DIVIDER_RATIO = 11.0; 
const float CURRENT_SENSITIVITY = 0.066; 
const float V_REF = 3.3;
const int ADC_MAX = 4095;

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

// Web Server & DNS
WebServer server(80);
DNSServer dnsServer;

// --- HTML Content ---
const char INDEX_HTML_1[] PROGMEM = R"rawliteral(
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
    <button type="button" id="btnEnter" style="z-index: 9999; position: relative; background-color: #27ae60; color: white; padding: 15px; border-radius: 8px; font-size: 18px; width: 100%; border: none;">Enter Dashboard</button>
    <div id="fsWarn" style="display:none; color:var(--danger); font-size:12px; margin-top:10px; text-align:center;">
        Warning: Graph library (chart.min.js) missing from ESP32 storage.
    </div>
</div>
<div id="dashboardPage" class="dashboard-container" style="display:none">
    <div class="header">
        <div class="logo-container">
            <svg class="logo-img" viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg">
                <path d="M12 2L2 7L12 12L22 7L12 2Z" fill="#2ecc71"/>
                <path d="M2 17L12 22L22 17" stroke="#2ecc71" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
                <path d="M2 12L12 17L22 12" stroke="#2ecc71" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
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
<script>
    var HAS_CHART = )rawliteral";

const char INDEX_HTML_2[] PROGMEM = R"rawliteral(;
    var chart = null;
    var isLive = true;
    var enterAppTriggered = false;
    var dashboardInterval = null;

    function log(m) {
        console.log(m);
    }

    window.onerror = function(msg, url, line) {
        log("ERROR: " + msg + " line " + line);
        return false;
    };

    // Run immediately since script is at bottom of body
    log("System Initialized");
    if (!HAS_CHART) {
        var warn = document.getElementById('fsWarn');
        if (warn) warn.style.display = 'block';
        log("Warn: chart.min.js missing");
    }

    function toggleAdminField() {
        var field = document.getElementById('adminField');
        var admin = document.getElementById('roleAdmin');
        if (field && admin) field.style.display = admin.checked ? 'block' : 'none';
    }

    function enterApp() {
        log("Enter Triggered");
        if (enterAppTriggered) { log("Busy"); return; }
        enterAppTriggered = true;
        try {
            var login = document.getElementById('loginPage');
            var dash = document.getElementById('dashboardPage');
            if (!login || !dash) { log("DOM missing!"); return; }

            var roleAdmin = document.getElementById('roleAdmin');
            var isAdmin = roleAdmin && roleAdmin.checked;

            if (isAdmin) {
                var passEl = document.getElementById('password');
                if (!passEl || passEl.value !== 'kilikawi') {
                    alert("Incorrect Admin Password");
                    return;
                }
            }

            login.style.setProperty('display', 'none', 'important');
            dash.style.setProperty('display', 'block', 'important');
            var adminTools = document.getElementById('adminTools');
            if (adminTools) adminTools.style.display = isAdmin ? 'block' : 'none';
            log("UI Switched");
            initDashboard();

            if (typeof Chart === 'undefined') {
                log("Loading Chart.js...");
                var s = document.createElement('script');
                s.src = HAS_CHART ? '/chart.min.js' : 'https://cdn.jsdelivr.net/npm/chart.js';
                s.onload = function() { log("Chart.js Loaded"); reInitChart(); };
                s.onerror = function() { 
                    if (HAS_CHART) {
                        log("Local Chart.js Failed - Trying CDN...");
                        var s2 = document.createElement('script');
                        s2.src = 'https://cdn.jsdelivr.net/npm/chart.js';
                        s2.onload = function() { log("CDN Chart.js Loaded"); reInitChart(); };
                        document.head.appendChild(s2);
                    } else {
                        log("Chart.js Load Failed"); 
                    }
                };
                document.head.appendChild(s);
            }
        } catch(e) {
            log("JS Error: " + e.message);
        } finally {
            enterAppTriggered = false;
        }
    }

    function reInitChart() {
        log("Re-initializing Chart...");
        var ctx = document.getElementById('mainChart');
        if (ctx && typeof Chart !== 'undefined') {
            try {
                if (chart) chart.destroy();
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
        if (dashboardInterval) return;
        log("Connecting Data...");
        reInitChart();
        fetchProfiles();
        dashboardInterval = setInterval(function() {
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

    // Bind button immediately
    (function() {
        var btn = document.getElementById('btnEnter');
        if (btn) {
            btn.addEventListener('click', enterApp);
            btn.addEventListener('touchend', function(e) {
                e.preventDefault();
                enterApp();
            });
        } else {
            log("ERROR: btnEnter not found!");
        }
    })();
</script>
</body>
</html>
)rawliteral";

// --- Logic Functions ---

String getAccessoryName() {
  int code = 0;
  for (int i = 0; i < 4; i++) {
    if (digitalRead(PINS_ACCESSORY[i]) == LOW) {
      delay(5); 
      if (digitalRead(PINS_ACCESSORY[i]) == LOW) code |= (1 << i);
    }
  }
  switch (code) {
    case 0b0001: return "Sheller";
    case 0b0010: return "Tiller";
    case 0b0100: return "Planter";
    case 0b1000: return "Mower";
    default: return "No Tool";
  }
}

int getThrottleState() {
  float voltage = analogRead(PIN_THROTTLE) * (V_REF / ADC_MAX);
  if (voltage < 1.0) return 0;
  if (voltage < 2.0) return 1;
  return 2;
}

float readVoltage() {
  long sum = 0;
  for(int i=0; i<20; i++) sum += analogRead(PIN_VOLTAGE);
  return (sum / 20.0) * (V_REF / ADC_MAX) * VOLTAGE_DIVIDER_RATIO;
}

float readCurrent() {
  long sum = 0;
  for(int i=0; i<50; i++) sum += analogRead(PIN_CURRENT);
  float voltage = (sum / 50.0) * (V_REF / ADC_MAX);
  float current = (voltage - 1.65) / CURRENT_SENSITIVITY;
  return (abs(current) < 0.15) ? 0 : current;
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
  }d

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
      doc["t"] = ramBuffer[i].timestamp; doc["v"] = ramBuffer[i].voltage;
      doc["i"] = ramBuffer[i].current; doc["p"] = ramBuffer[i].power;
      doc["th"] = ramBuffer[i].throttleState; doc["acc"] = currentAccessoryName;
      serializeJson(doc, file); file.println();
    }
    file.close();
  }
  bufferIndex = 0;
}

// --- Setup & Loop ---

bool checkFileExists(const char* path) {
  return LittleFS.exists(path) || LittleFS.exists(String("/") + path);
}

void setup() {
  Serial.begin(115200);
  delay(2000); 
  Serial.println("\n--- Kilikawi Dashboard Booting ---");
  
  for (int i = 0; i < 4; i++) pinMode(PINS_ACCESSORY[i], INPUT_PULLUP);
  pinMode(PIN_VOLTAGE, INPUT); pinMode(PIN_CURRENT, INPUT); pinMode(PIN_THROTTLE, INPUT);

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(AP_SSID, AP_PASS);
  
  Serial.print("Initializing LittleFS... ");
  if (!LittleFS.begin(true)) {
    Serial.println("FAILED! Attempting emergency format...");
    if (LittleFS.format()) {
      Serial.println("Format successful, retrying mount...");
      if (LittleFS.begin(true)) Serial.println("LittleFS Ready after format.");
      else Serial.println("LittleFS still failing!");
    } else {
      Serial.println("Format failed!");
    }
  } else {
    Serial.println("Mount Successful.");
  }
  
  manageFiles();
   
   if (!checkFileExists("chart.min.js")) {
     Serial.println("WARNING: chart.min.js not found in storage. Graphs will only work if device has internet access (CDN fallback).");
   } else {
     Serial.println("Chart.js found in LittleFS.");
   }

  server.on("/", []() {
    bool hasChart = checkFileExists("chart.min.js");
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");
    server.sendContent_P(INDEX_HTML_1);
    server.sendContent(hasChart ? "true" : "false");
    server.sendContent_P(INDEX_HTML_2);
  });
  server.on("/chart.min.js", []() {
    if (checkFileExists("chart.min.js")) {
      File f = LittleFS.open(LittleFS.exists("/chart.min.js") ? "/chart.min.js" : "chart.min.js", "r");
      server.streamFile(f, "application/javascript");
      f.close();
    } else server.send(404, "text/plain", "Missing Chart.js");
  });
  server.on("/api/live", []() {
    JsonDocument doc;
    float v = readVoltage(), i = readCurrent();
    doc["accessory"] = currentAccessoryName; doc["v"] = v; doc["i"] = i; doc["p"] = v * i;
    doc["throttle"] = getThrottleState(); doc["logging"] = isLogging;
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
    server.sendContent("Timestamp,Voltage(V),Current(A),Power(W),ThrottleState,Accessory\n");
    File f = LittleFS.open(fName, "r");
    while (f.available()) {
      String l = f.readStringUntil('\n');
      if (l.length() > 0) {
        JsonDocument d; deserializeJson(d, l);
        char buf[128];
        sprintf(buf, "%lu,%.2f,%.2f,%.2f,%d,%s\n", d["t"].as<unsigned long>(), d["v"].as<float>(), d["i"].as<float>(), d["p"].as<float>(), d["th"].as<int>(), d["acc"].as<const char*>());
        server.sendContent(buf);
      }
    }
    f.close();
  });
  server.onNotFound([]() { server.sendHeader("Location", "http://192.168.4.1/"); server.send(302); });

  server.begin();
  dnsServer.start(53, "*", AP_IP);
  Serial.println("System Ready.");
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  
  unsigned long now = millis();
  if (now - lastCaptureTime >= 500) {
    lastCaptureTime = now;
    currentAccessoryName = getAccessoryName(); // Update detection every 500ms
    float v = readVoltage(), i = readCurrent(), p = v * i;
    if (p > 1.0) { lastPowerTime = now; isLogging = true; }
    else if (now - lastPowerTime > 300000) isLogging = false;

    if (isLogging && bufferIndex < MAX_RAM_POINTS) {
      ramBuffer[bufferIndex] = {now, v, i, p, getThrottleState()};
      bufferIndex++;
    }
  }
  if (now - lastSaveTime >= 30000) { lastSaveTime = now; saveToFS(); }
  delay(1);
}

