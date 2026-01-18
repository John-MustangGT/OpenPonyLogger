#include "wifi_manager.h"
#include "web_pages.h"
#include "config_manager.h"
#include "version_info.h"
#include <cstdio>
#include <ArduinoJson.h>

// Static member initialization
AsyncWebServer* WiFiManager::m_server = nullptr;
AsyncWebSocket* WiFiManager::m_websocket = nullptr;
bool WiFiManager::m_initialized = false;
String WiFiManager::m_ssid = "";
String WiFiManager::m_password = "";

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
    m_server->on("/api/config", HTTP_GET, handle_config_get);
    m_server->on("/api/config", HTTP_POST, [](AsyncWebServerRequest* request){}, nullptr, handle_config_post);
    m_server->on("/api/about", HTTP_GET, handle_about);
    
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
    request->send(200, "text/html; charset=utf-8", HTML_MAIN_PAGE);
}

void WiFiManager::handle_config_get(AsyncWebServerRequest* request) {
    logging_config_t config = ConfigManager::get_current();
    
    JsonDocument doc;
    doc["main_loop_hz"] = config.main_loop_hz;
    doc["gps_hz"] = config.gps_hz;
    doc["imu_hz"] = config.imu_hz;
    doc["obd_hz"] = config.obd_hz;
    
    // Add PID configurations
    JsonArray pids = doc["pids"].to<JsonArray>();
    for (const auto& pid_pair : config.pid_configs) {
        JsonObject pid_obj = pids.add<JsonObject>();
        char pid_hex[8];
        snprintf(pid_hex, sizeof(pid_hex), "0x%02X", pid_pair.second.pid);
        pid_obj["pid"] = pid_hex;
        pid_obj["pid_dec"] = pid_pair.second.pid;
        pid_obj["enabled"] = pid_pair.second.enabled;
        pid_obj["rate_hz"] = pid_pair.second.rate_hz;
        pid_obj["name"] = pid_pair.second.name;
    }
    
    String json_str;
    serializeJson(doc, json_str);
    request->send(200, "application/json", json_str);
}

void WiFiManager::handle_config_post(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, (const char*)data, len);
    
    if (error) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
        return;
    }
    
    logging_config_t config;
    config.main_loop_hz = doc["main_loop_hz"] | 10;
    config.gps_hz = doc["gps_hz"] | 10;
    config.imu_hz = doc["imu_hz"] | 10;
    config.obd_hz = doc["obd_hz"] | 10;
    
    bool success = ConfigManager::update(config);
    
    String response = success ? 
        "{\"success\":true,\"message\":\"Configuration saved\"}" :
        "{\"success\":false,\"error\":\"Validation failed\"}";
    
    request->send(success ? 200 : 400, "application/json", response);
}

void WiFiManager::handle_about(AsyncWebServerRequest* request) {
    JsonDocument doc;
<<<<<<< Updated upstream
    doc["version"] = PROJECT_VERSION;
    doc["git_sha"] = GIT_COMMIT_SHA;
    doc["build_date"] = BUILD_TIMESTAMP;
    doc["project_name"] = PROJECT_NAME;
    doc["license"] = PROJECT_LICENSE;
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
=======
    
    #ifdef GIT_COMMIT_SHA
        doc["git_sha"] = GIT_COMMIT_SHA;
    #else
        doc["git_sha"] = "unknown";
    #endif
    
    #ifdef PROJECT_VERSION
        doc["version"] = PROJECT_VERSION;
    #else
        doc["version"] = "1.0.0";
    #endif
    
    // Device status (placeholder - could check actual hardware)
    doc["devices"]["gps"] = true;
    doc["devices"]["imu"] = true;
    doc["devices"]["battery"] = true;
    
    String json_str;
    serializeJson(doc, json_str);
    request->send(200, "application/json", json_str);
>>>>>>> Stashed changes
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

