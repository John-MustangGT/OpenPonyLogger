#include "wifi_manager.h"
#include <cstdio>

// Static member initialization
AsyncWebServer* WiFiManager::m_server = nullptr;
AsyncWebSocket* WiFiManager::m_websocket = nullptr;
bool WiFiManager::m_initialized = false;
String WiFiManager::m_ssid = "";
String WiFiManager::m_password = "";

// HTML web page with embedded CSS and JavaScript
const char* HTML_PAGE = R"====(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>OpenPonyLogger</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, Cantarell, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
            padding: 20px;
        }
        
        .container {
            background: white;
            border-radius: 12px;
            box-shadow: 0 10px 40px rgba(0,0,0,0.2);
            max-width: 800px;
            width: 100%;
            padding: 30px;
        }
        
        h1 {
            color: #333;
            margin-bottom: 10px;
            text-align: center;
        }
        
        .status {
            text-align: center;
            color: #666;
            margin-bottom: 30px;
            font-size: 14px;
        }
        
        .status.connected::before {
            content: "●";
            color: #4caf50;
            margin-right: 8px;
            font-size: 16px;
        }
        
        .status.disconnected::before {
            content: "●";
            color: #f44336;
            margin-right: 8px;
            font-size: 16px;
        }
        
        .grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 20px;
            margin-bottom: 30px;
        }
        
        .card {
            background: #f5f5f5;
            border-radius: 8px;
            padding: 20px;
            border-left: 4px solid #667eea;
        }
        
        .card h3 {
            color: #667eea;
            font-size: 12px;
            text-transform: uppercase;
            letter-spacing: 1px;
            margin-bottom: 10px;
        }
        
        .card .value {
            font-size: 24px;
            font-weight: bold;
            color: #333;
            font-family: 'Courier New', monospace;
        }
        
        .card .unit {
            font-size: 12px;
            color: #999;
            margin-left: 5px;
        }
        
        .gps-card {
            border-left-color: #ff9800;
        }
        
        .battery-card {
            border-left-color: #4caf50;
        }
        
        .imu-card {
            border-left-color: #2196f3;
        }
        
        .gps-status {
            display: inline-block;
            width: 12px;
            height: 12px;
            border-radius: 50%;
            margin-right: 5px;
            background: #f44336;
            vertical-align: middle;
        }
        
        .gps-status.valid {
            background: #4caf50;
        }
        
        .section-title {
            color: #333;
            font-size: 16px;
            font-weight: 600;
            margin-top: 25px;
            margin-bottom: 15px;
            padding-bottom: 10px;
            border-bottom: 2px solid #f0f0f0;
        }
        
        .raw-data {
            background: #f9f9f9;
            border: 1px solid #e0e0e0;
            border-radius: 6px;
            padding: 15px;
            font-family: 'Courier New', monospace;
            font-size: 12px;
            color: #333;
            max-height: 200px;
            overflow-y: auto;
            word-break: break-all;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🐴 OpenPonyLogger</h1>
        <div class="status disconnected" id="connectionStatus">Connecting...</div>
        
        <div class="section-title">Navigation</div>
        <div class="grid">
            <div class="card">
                <h3>Uptime</h3>
                <div class="value"><span id="uptime">--:--:--</span></div>
            </div>
            <div class="card">
                <h3>Samples Logged</h3>
                <div class="value"><span id="sampleCount">0</span><span class="unit" id="sampleSymbol">●</span></div>
            </div>
        </div>
        
        <div class="section-title">GPS</div>
        <div class="grid">
            <div class="card gps-card">
                <h3>Status</h3>
                <div class="value"><span class="gps-status" id="gpsStatus"></span><span id="gpsStatusText">No Fix</span></div>
            </div>
            <div class="card gps-card">
                <h3>Latitude</h3>
                <div class="value"><span id="latitude">--</span>°</div>
            </div>
            <div class="card gps-card">
                <h3>Longitude</h3>
                <div class="value"><span id="longitude">--</span>°</div>
            </div>
            <div class="card gps-card">
                <h3>Altitude</h3>
                <div class="value"><span id="altitude">--</span><span class="unit">m</span></div>
            </div>
            <div class="card gps-card">
                <h3>Speed</h3>
                <div class="value"><span id="speed">--</span><span class="unit">kt</span></div>
            </div>
            <div class="card gps-card">
                <h3>Satellites</h3>
                <div class="value"><span id="satellites">--</span></div>
            </div>
        </div>
        
        <div class="section-title">IMU</div>
        <div class="grid">
            <div class="card imu-card">
                <h3>Acceleration</h3>
                <div class="value"><span id="accel">--</span><span class="unit">g</span></div>
            </div>
            <div class="card imu-card">
                <h3>Rotation</h3>
                <div class="value"><span id="gyro">--</span><span class="unit">dps</span></div>
            </div>
            <div class="card imu-card">
                <h3>Temperature</h3>
                <div class="value"><span id="temperature">--</span>°<span id="tempUnit">C</span></div>
            </div>
        </div>
        
        <div class="section-title">Battery</div>
        <div class="grid">
            <div class="card battery-card">
                <h3>State of Charge</h3>
                <div class="value"><span id="battery">--</span><span class="unit">%</span></div>
            </div>
            <div class="card battery-card">
                <h3>Voltage</h3>
                <div class="value"><span id="voltage">--</span><span class="unit">V</span></div>
            </div>
            <div class="card battery-card">
                <h3>Current</h3>
                <div class="value"><span id="current">--</span><span class="unit">mA</span></div>
            </div>
            <div class="card battery-card">
                <h3>Temperature</h3>
                <div class="value"><span id="batteryTemp">--</span>°<span id="batteryTempUnit">C</span></div>
            </div>
        </div>
        
        <div class="section-title">Raw Data Stream</div>
        <div class="raw-data" id="rawData">Waiting for data...</div>
    </div>

    <script>
        // WebSocket connection
        let ws = null;
        const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        const wsUrl = protocol + '//' + window.location.host + '/ws';
        
        // Configuration: format numbers to fixed decimal places
        const DECIMAL_PLACES = 2;
        
        // Format number with specified decimal places
        function formatNumber(num, decimals = DECIMAL_PLACES) {
            if (num === null || num === undefined || isNaN(num)) return '--';
            return parseFloat(num).toFixed(decimals);
        }
        
        // Connect to WebSocket
        function connectWebSocket() {
            try {
                ws = new WebSocket(wsUrl);
                
                ws.onopen = () => {
                    console.log('WebSocket connected');
                    updateConnectionStatus(true);
                };
                
                ws.onmessage = (event) => {
                    try {
                        const data = JSON.parse(event.data);
                        
                        // Only process sensor data messages
                        if (data.type === 'sensor') {
                            updateSensorDisplay(data);
                        }
                        // Ignore other message types for extensibility
                    } catch (e) {
                        console.error('Failed to parse JSON:', e);
                    }
                };
                
                ws.onerror = (error) => {
                    console.error('WebSocket error:', error);
                    updateConnectionStatus(false);
                };
                
                ws.onclose = () => {
                    console.log('WebSocket disconnected');
                    updateConnectionStatus(false);
                    // Attempt to reconnect after 3 seconds
                    setTimeout(connectWebSocket, 3000);
                };
            } catch (e) {
                console.error('WebSocket connection failed:', e);
                updateConnectionStatus(false);
                setTimeout(connectWebSocket, 3000);
            }
        }
        
        // Update connection status indicator
        function updateConnectionStatus(connected) {
            const status = document.getElementById('connectionStatus');
            if (connected) {
                status.textContent = 'Connected';
                status.className = 'status connected';
            } else {
                status.textContent = 'Disconnected';
                status.className = 'status disconnected';
            }
        }
        
        // Update sensor display from JSON data
        function updateSensorDisplay(data) {
            // Navigation
            if (data.uptime_ms !== undefined) {
                const totalSeconds = Math.floor(data.uptime_ms / 1000);
                const hours = Math.floor(totalSeconds / 3600);
                const minutes = Math.floor((totalSeconds % 3600) / 60);
                const seconds = totalSeconds % 60;
                document.getElementById('uptime').textContent = 
                    `${hours}:${String(minutes).padStart(2, '0')}:${String(seconds).padStart(2, '0')}`;
            }
            
            if (data.sample_count !== undefined) {
                let displayCount = data.sample_count;
                let unit = '';
                if (data.sample_count >= 1000000) {
                    displayCount = (data.sample_count / 1000000).toFixed(1);
                    unit = 'M';
                } else if (data.sample_count >= 1000) {
                    displayCount = (data.sample_count / 1000).toFixed(1);
                    unit = 'K';
                }
                document.getElementById('sampleCount').textContent = displayCount + unit;
                const symbol = data.is_paused ? '⏸' : '●';
                document.getElementById('sampleSymbol').textContent = symbol;
            }
            
            // GPS
            if (data.gps_valid !== undefined) {
                const statusEl = document.getElementById('gpsStatus');
                const textEl = document.getElementById('gpsStatusText');
                if (data.gps_valid) {
                    statusEl.className = 'gps-status valid';
                    textEl.textContent = 'Valid';
                } else {
                    statusEl.className = 'gps-status';
                    textEl.textContent = 'No Fix';
                }
            }
            
            if (data.latitude !== undefined) {
                document.getElementById('latitude').textContent = formatNumber(data.latitude, 6);
            }
            if (data.longitude !== undefined) {
                document.getElementById('longitude').textContent = formatNumber(data.longitude, 6);
            }
            if (data.altitude !== undefined) {
                document.getElementById('altitude').textContent = formatNumber(data.altitude, 1);
            }
            if (data.speed !== undefined) {
                document.getElementById('speed').textContent = formatNumber(data.speed, 1);
            }
            if (data.satellites !== undefined) {
                document.getElementById('satellites').textContent = data.satellites;
            }
            
            // IMU
            if (data.accel_x !== undefined && data.accel_y !== undefined && data.accel_z !== undefined) {
                const mag = Math.sqrt(data.accel_x**2 + data.accel_y**2 + data.accel_z**2);
                document.getElementById('accel').textContent = formatNumber(mag, 2);
            }
            if (data.gyro_x !== undefined && data.gyro_y !== undefined && data.gyro_z !== undefined) {
                const mag = Math.sqrt(data.gyro_x**2 + data.gyro_y**2 + data.gyro_z**2);
                document.getElementById('gyro').textContent = formatNumber(mag, 1);
            }
            if (data.temperature !== undefined) {
                document.getElementById('temperature').textContent = formatNumber(data.temperature, 1);
            }
            
            // Battery
            if (data.battery_soc !== undefined) {
                document.getElementById('battery').textContent = formatNumber(data.battery_soc, 0);
            }
            if (data.battery_voltage !== undefined) {
                document.getElementById('voltage').textContent = formatNumber(data.battery_voltage, 2);
            }
            if (data.battery_current !== undefined) {
                document.getElementById('current').textContent = formatNumber(data.battery_current, 0);
            }
            if (data.battery_temp !== undefined) {
                document.getElementById('batteryTemp').textContent = formatNumber(data.battery_temp, 1);
            }
            
            // Show raw JSON for debugging
            const rawDataEl = document.getElementById('rawData');
            rawDataEl.textContent = JSON.stringify(data, null, 2);
        }
        
        // Initialize on page load
        window.addEventListener('load', () => {
            connectWebSocket();
        });
        
        // Cleanup on page unload
        window.addEventListener('beforeunload', () => {
            if (ws) {
                ws.close();
            }
        });
    </script>
</body>
</html>
)====" ;

bool WiFiManager::init() {
    if (m_initialized) {
        return true;
    }
    
    Serial.println("[WiFi] Initializing WiFi in AP mode...");
    
    // Generate SSID from MAC address
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    
    char ssid_buffer[32];
    snprintf(ssid_buffer, sizeof(ssid_buffer), "PonyLogger-%02X%02X", mac[4], mac[5]);
    m_ssid = String(ssid_buffer);
    m_password = "";  // Open network
    
    Serial.printf("[WiFi] Starting AP with SSID: %s\n", m_ssid.c_str());
    
    // Configure AP mode
    WiFi.mode(WIFI_AP);
    WiFi.softAP(m_ssid.c_str(), m_password.c_str());
    
    IPAddress ap_ip(192, 168, 4, 1);
    IPAddress netmask(255, 255, 255, 0);
    WiFi.softAPConfig(ap_ip, ap_ip, netmask);
    
    IPAddress ip = WiFi.softAPIP();
    Serial.printf("[WiFi] AP IP Address: %s\n", ip.toString().c_str());
    
    // Create web server
    m_server = new AsyncWebServer(80);
    
    if (m_server == nullptr) {
        Serial.println("[WiFi] ERROR: Failed to allocate AsyncWebServer");
        return false;
    }
    
    // Create WebSocket handler
    m_websocket = new AsyncWebSocket("/ws");
    
    if (m_websocket == nullptr) {
        Serial.println("[WiFi] ERROR: Failed to allocate AsyncWebSocket");
        delete m_server;
        m_server = nullptr;
        return false;
    }
    
    // Set up WebSocket event handler
    m_websocket->onEvent(handle_websocket_event);
    m_server->addHandler(m_websocket);
    
    // Set up HTTP routes
    m_server->on("/", HTTP_GET, handle_root);
    
    // Start server
    m_server->begin();
    Serial.println("[WiFi] Web server started on port 80");
    Serial.println("[WiFi] WebSocket endpoint: /ws");
    Serial.printf("[WiFi] Open http://%s in your browser\n", ip.toString().c_str());
    
    m_initialized = true;
    return true;
}

String WiFiManager::get_ssid() {
    return m_ssid;
}

String WiFiManager::get_password() {
    return m_password;
}

uint16_t WiFiManager::get_client_count() {
    if (m_websocket == nullptr) return 0;
    return m_websocket->count();
}

void WiFiManager::broadcast_json(const char* json) {
    if (m_websocket == nullptr || json == nullptr) return;
    
    // Send to all connected clients
    m_websocket->textAll(json);
}

bool WiFiManager::is_initialized() {
    return m_initialized;
}

void WiFiManager::handle_root(AsyncWebServerRequest* request) {
    request->send(200, "text/html; charset=utf-8", HTML_PAGE);
}

void WiFiManager::handle_websocket_event(AsyncWebSocket* server, AsyncWebSocketClient* client,
                                         AwsEventType type, void* arg, uint8_t* data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("[WebSocket] Client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
            break;
            
        case WS_EVT_DISCONNECT:
            Serial.printf("[WebSocket] Client #%u disconnected\n", client->id());
            break;
            
        case WS_EVT_DATA: {
            // Currently not expecting client-side messages, but handle gracefully
            AwsFrameInfo* info = (AwsFrameInfo*)arg;
            if (info->opcode == WS_TEXT) {
                // Could implement commands from client if needed
                // For now, just log the message
                Serial.printf("[WebSocket] Message from client #%u: %.*s\n", client->id(), (int)len, data);
            }
            break;
        }
        
        case WS_EVT_ERROR:
            Serial.printf("[WebSocket] Error event on client #%u\n", client->id());
            break;
            
        default:
            break;
    }
}
