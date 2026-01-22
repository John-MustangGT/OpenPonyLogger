#include "wifi_manager.h"
#include "web_pages.h"
#include "config_manager.h"
#include "icar_ble_driver.h"
#include "version_info.h"
#include "log_file_manager_flash.h"  // Flash-based file manager
#include <cstdio>
#include <ArduinoJson.h>
#include <esp_log.h>

static const char* TAG = "WiFi";

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

    ESP_LOGI(TAG, "Initializing WiFi in AP mode...");

    // Ensure ConfigManager is initialized
    if (!ConfigManager::init()) {
        ESP_LOGW(TAG, "ConfigManager not ready, using defaults");
    }

    // Get configuration with fallback
    logging_config_t config = ConfigManager::get_current();

    // Validate network config has valid data, use defaults if not
    if (config.network.ssid[0] == '\0' || strlen(config.network.ssid) == 0) {
        ESP_LOGW(TAG, "Invalid SSID in config, using default");
        strncpy(config.network.ssid, "PonyLogger", sizeof(config.network.ssid) - 1);
        config.network.ssid[sizeof(config.network.ssid) - 1] = '\0';
    }

    // Generate SSID from config + MAC address suffix
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    char ssid_buffer[32];
    snprintf(ssid_buffer, sizeof(ssid_buffer), "%s-%02X%02X",
             config.network.ssid, mac[4], mac[5]);
    m_ssid = String(ssid_buffer);
    m_password = String(config.network.password);

    ESP_LOGI(TAG, "Starting AP with SSID: %s", m_ssid.c_str());

    // Configure AP mode
    WiFi.mode(WIFI_AP);
    WiFi.softAP(m_ssid.c_str(), m_password.length() > 0 ? m_password.c_str() : nullptr);

    // Validate IP configuration
    IPAddress ap_ip(config.network.ip[0], config.network.ip[1],
                     config.network.ip[2], config.network.ip[3]);
    IPAddress netmask(config.network.subnet[0], config.network.subnet[1],
                      config.network.subnet[2], config.network.subnet[3]);

    // Use defaults if IP is invalid (0.0.0.0)
    if (config.network.ip[0] == 0 && config.network.ip[1] == 0 &&
        config.network.ip[2] == 0 && config.network.ip[3] == 0) {
        ESP_LOGW(TAG, "Invalid IP in config, using 192.168.4.1");
        ap_ip = IPAddress(192, 168, 4, 1);
        netmask = IPAddress(255, 255, 255, 0);
    }

    WiFi.softAPConfig(ap_ip, ap_ip, netmask);

    IPAddress ip = WiFi.softAPIP();
    ESP_LOGI(TAG, "AP IP Address: %s", ip.toString().c_str());

    // Create web server
    m_server = new AsyncWebServer(80);

    if (m_server == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate AsyncWebServer");
        return false;
    }

    // Create WebSocket handler
    m_websocket = new AsyncWebSocket("/ws");

    if (m_websocket == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate AsyncWebSocket");
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
    m_server->on("/api/restart", HTTP_POST, handle_restart);
    m_server->on("/api/divorce", HTTP_POST, handle_divorce);

    // Log file management routes
    m_server->on("/api/logs", HTTP_GET, handle_logs_list);
    m_server->on("/api/logs/download", HTTP_GET, handle_log_download);
    m_server->on("/api/logs/delete", HTTP_POST, handle_log_delete);
    m_server->on("/api/logs/delete-all", HTTP_POST, handle_logs_delete_all);

    // Start server
    m_server->begin();
    ESP_LOGI(TAG, "Web server started on port 80");
    ESP_LOGI(TAG, "WebSocket endpoint: /ws");
    ESP_LOGI(TAG, "Open http://%s in your browser", ip.toString().c_str());

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

bool WiFiManager::has_clients() {
    return get_client_count() > 0;
}

void WiFiManager::broadcast_json(const char* json) {
    if (m_websocket == nullptr || json == nullptr) return;

    // Verify we have clients before broadcasting
    if (m_websocket->count() == 0) return;

    // Check json string length to prevent crashes
    size_t len = strlen(json);
    if (len == 0 || len > 2048) {
        ESP_LOGW(TAG, "Invalid JSON length: %zu", len);
        return;
    }

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
    ESP_LOGD(TAG, "handle_config_get called");

    if (!ConfigManager::init()) {
        ESP_LOGE(TAG, "Config manager not initialized");
        request->send(500, "application/json", "{\"success\":false,\"error\":\"Config not initialized\"}");
        return;
    }

    logging_config_t config = ConfigManager::get_current();

    // Use larger document size for all the data
    JsonDocument doc;
    doc["main_loop_hz"] = config.main_loop_hz;
    doc["gps_hz"] = config.gps_hz;
    doc["imu_hz"] = config.imu_hz;
    doc["obd_hz"] = config.obd_hz;
    doc["obd_ble_enabled"] = config.obd_ble_enabled;
    doc["log_level"] = config.log_level;
    doc["dynamics_auto_enabled"] = config.dynamics_auto_enabled;
    doc["dynamics_start_speed_mph"] = config.dynamics_start_speed_mph;
    doc["dynamics_stop_timeout_sec"] = config.dynamics_stop_timeout_sec;
    doc["data_auto_enabled"] = config.data_auto_enabled;
    doc["data_stop_timeout_sec"] = config.data_stop_timeout_sec;

    // Add network configuration with null-termination safety
    JsonObject network = doc["network"].to<JsonObject>();
    network["ssid"] = String(config.network.ssid);
    network["password"] = String(config.network.password);
    char ip_str[16];
    snprintf(ip_str, sizeof(ip_str), "%d.%d.%d.%d",
             config.network.ip[0], config.network.ip[1], config.network.ip[2], config.network.ip[3]);
    network["ip"] = ip_str;
    char subnet_str[16];
    snprintf(subnet_str, sizeof(subnet_str), "%d.%d.%d.%d",
             config.network.subnet[0], config.network.subnet[1], config.network.subnet[2], config.network.subnet[3]);
    network["subnet"] = subnet_str;

    ESP_LOGD(TAG, "Network config - SSID: %s, IP: %s", config.network.ssid, ip_str);

    // Add PID configurations only if map is not empty
    JsonArray pids = doc["pids"].to<JsonArray>();
    if (!config.pid_configs.empty()) {
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
    }

    // Add per-module log levels
    JsonObject module_log_levels = doc["module_log_levels"].to<JsonObject>();
    for (const auto& entry : config.module_log_levels) {
        module_log_levels[entry.first] = entry.second;
    }

    String json_str;
    serializeJson(doc, json_str);

    ESP_LOGD(TAG, "Sending config response (%zu bytes)", json_str.length());
    request->send(200, "application/json", json_str);
}

void WiFiManager::handle_config_post(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
    ESP_LOGD(TAG, "handle_config_post called (len=%zu, total=%zu)", len, total);

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, (const char*)data, len);

    if (error) {
        ESP_LOGE(TAG, "JSON parse error: %s", error.c_str());
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
        return;
    }
    
    logging_config_t config = ConfigManager::get_current();  // Start with current config
    config.main_loop_hz = doc["main_loop_hz"] | 10;
    config.gps_hz = doc["gps_hz"] | 10;
    config.imu_hz = doc["imu_hz"] | 10;
    config.obd_hz = doc["obd_hz"] | 10;
    config.obd_ble_enabled = doc["obd_ble_enabled"] | true;

    // Parse log level if provided
    if (doc.containsKey("log_level")) {
        config.log_level = doc["log_level"] | 3;
    }

    // Parse per-module log levels if provided
    if (doc.containsKey("module_log_levels")) {
        JsonObject module_levels = doc["module_log_levels"];
        config.module_log_levels.clear();
        for (JsonPair kv : module_levels) {
            String module_name = kv.key().c_str();
            uint8_t level = kv.value().as<uint8_t>();
            config.module_log_levels[module_name] = level;
        }
    }

    // Parse auto-logging configuration if provided
    if (doc.containsKey("dynamics_auto_enabled")) {
        config.dynamics_auto_enabled = doc["dynamics_auto_enabled"] | false;
    }
    if (doc.containsKey("dynamics_start_speed_mph")) {
        config.dynamics_start_speed_mph = doc["dynamics_start_speed_mph"] | 5;
    }
    if (doc.containsKey("dynamics_stop_timeout_sec")) {
        config.dynamics_stop_timeout_sec = doc["dynamics_stop_timeout_sec"] | 30;
    }
    if (doc.containsKey("data_auto_enabled")) {
        config.data_auto_enabled = doc["data_auto_enabled"] | false;
    }
    if (doc.containsKey("data_stop_timeout_sec")) {
        config.data_stop_timeout_sec = doc["data_stop_timeout_sec"] | 300;
    }

    // Parse network configuration if provided
    if (doc.containsKey("network")) {
        JsonObject network = doc["network"];
        if (network.containsKey("ssid")) {
            strncpy(config.network.ssid, network["ssid"] | "PonyLogger", sizeof(config.network.ssid) - 1);
            config.network.ssid[sizeof(config.network.ssid) - 1] = '\0';
        }
        if (network.containsKey("password")) {
            strncpy(config.network.password, network["password"] | "", sizeof(config.network.password) - 1);
            config.network.password[sizeof(config.network.password) - 1] = '\0';
        }
        if (network.containsKey("ip")) {
            const char* ip_str = network["ip"];
            sscanf(ip_str, "%hhu.%hhu.%hhu.%hhu", 
                   &config.network.ip[0], &config.network.ip[1], 
                   &config.network.ip[2], &config.network.ip[3]);
        }
        if (network.containsKey("subnet")) {
            const char* subnet_str = network["subnet"];
            sscanf(subnet_str, "%hhu.%hhu.%hhu.%hhu", 
                   &config.network.subnet[0], &config.network.subnet[1], 
                   &config.network.subnet[2], &config.network.subnet[3]);
        }
    }
    
    bool success = ConfigManager::update(config);

    // Apply log levels immediately without restart
    if (success) {
        ConfigManager::apply_log_levels(config);
        ESP_LOGI(TAG, "Log levels applied: global=%d, modules=%zu",
                 config.log_level, config.module_log_levels.size());
    }

    String response = success ?
        "{\"success\":true,\"message\":\"Configuration saved\"}" :
        "{\"success\":false,\"error\":\"Validation failed\"}";

    request->send(success ? 200 : 400, "application/json", response);
}

void WiFiManager::handle_about(AsyncWebServerRequest* request) {
    JsonDocument doc;

    #ifdef GIT_COMMIT_SHA
        doc["git_sha"] = GIT_COMMIT_SHA;
    #else
        doc["git_sha"] = "unknown";
    #endif

    #ifdef GIT_BRANCH
        doc["git_branch"] = GIT_BRANCH;
    #else
        doc["git_branch"] = "unknown";
    #endif

    #ifdef GIT_TAG
        doc["version"] = GIT_TAG;
    #else
        doc["version"] = "v0.0.0";
    #endif

    #ifdef BUILD_TIMESTAMP
        doc["build_time"] = BUILD_TIMESTAMP;
    #else
        doc["build_time"] = "unknown";
    #endif
    
    // Memory information with safety checks
    JsonObject memory = doc["memory"].to<JsonObject>();
    
    uint32_t heap_size = ESP.getHeapSize();
    uint32_t heap_free = ESP.getFreeHeap();
    memory["heap_total"] = heap_size;
    memory["heap_free"] = heap_free;
    memory["heap_used"] = (heap_size > heap_free) ? (heap_size - heap_free) : 0;
    memory["heap_min_free"] = ESP.getMinFreeHeap();
    
    uint32_t psram_size = ESP.getPsramSize();
    uint32_t psram_free = ESP.getFreePsram();
    memory["psram_total"] = psram_size;
    memory["psram_free"] = psram_free;
    memory["psram_used"] = (psram_size > psram_free) ? (psram_size - psram_free) : 0;
    memory["psram_min_free"] = ESP.getMinFreePsram();
    
    memory["flash_total"] = ESP.getFlashChipSize();
    memory["sketch_size"] = ESP.getSketchSize();
    memory["sketch_free"] = ESP.getFreeSketchSpace();
    
    // Device status (placeholder - could check actual hardware)
    doc["devices"]["gps"] = true;
    doc["devices"]["imu"] = true;
    doc["devices"]["battery"] = true;
    
    // OBD/ELM-327 status
    bool obd_connected = false;
    try {
        obd_connected = IcarBleDriver::is_connected();
    } catch (...) {
        obd_connected = false;
    }
    
    doc["devices"]["obd"] = obd_connected;
    if (obd_connected) {
        doc["obd_info"]["device_name"] = String(IcarBleDriver::get_device_name());
        doc["obd_info"]["address"] = String(IcarBleDriver::get_device_address());
        
        const char* vin = IcarBleDriver::get_vin();
        const char* ecm = IcarBleDriver::get_ecm_name();
        
        if (vin && strlen(vin) > 0) {
            doc["obd_info"]["vin"] = String(vin);
        }
        if (ecm && strlen(ecm) > 0) {
            doc["obd_info"]["ecm_name"] = String(ecm);
        }
    }

    // Marriage status
    logging_config_t config = ConfigManager::get_current();
    doc["marriage"]["is_married"] = config.is_married;
    if (config.is_married) {
        doc["marriage"]["vin"] = String(config.married_vin);
        if (strlen(config.married_ecu) > 0) {
            doc["marriage"]["ecu_name"] = String(config.married_ecu);
        }
    }

    String json_str;
    serializeJson(doc, json_str);
    request->send(200, "application/json", json_str);
}

void WiFiManager::handle_restart(AsyncWebServerRequest* request) {
    ESP_LOGI(TAG, "Restart requested via web interface");
    request->send(200, "application/json", "{\"success\":true,\"message\":\"Restarting device...\"}");

    // Delay restart to allow response to be sent
    delay(500);
    ESP.restart();
}

void WiFiManager::handle_divorce(AsyncWebServerRequest* request) {
    ESP_LOGI(TAG, "Divorce requested via web interface");

    if (!ConfigManager::is_married()) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Logger is not married to any vehicle\"}");
        return;
    }

    if (ConfigManager::divorce_from_vehicle()) {
        ESP_LOGI(TAG, "✓ Successfully divorced from vehicle");
        request->send(200, "application/json", "{\"success\":true,\"message\":\"Logger divorced from vehicle\"}");
    } else {
        ESP_LOGE(TAG, "Failed to divorce from vehicle");
        request->send(500, "application/json", "{\"success\":false,\"error\":\"Failed to save divorce status\"}");
    }
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

void WiFiManager::handle_logs_list(AsyncWebServerRequest* request) {
    if (!LogFileManager::init()) {
        request->send(500, "application/json", "{\"success\":false,\"error\":\"Log manager not initialized\"}");
        return;
    }
    
    // Scan for current session
    LogFileManager::scan_log_files();
    
    const std::vector<log_file_info_t>& files = LogFileManager::get_log_files();
    
    JsonDocument doc;
    doc["success"] = true;
    doc["total_files"] = files.size();
    doc["total_size"] = LogFileManager::get_total_log_size();
    doc["free_space"] = LogFileManager::get_free_space();
    
    JsonArray files_array = doc["files"].to<JsonArray>();
    
    for (const auto& file_info : files) {
        JsonObject file_obj = files_array.add<JsonObject>();
        file_obj["filename"] = file_info.filename;
        file_obj["size"] = file_info.file_size;
        file_obj["blocks"] = file_info.block_count;
        file_obj["gps_utc"] = (long long)file_info.gps_utc_timestamp;
        file_obj["esp_time_us"] = (long long)file_info.esp_timestamp_us;
        
        // Format UUID as string
        char uuid_str[37];
        snprintf(uuid_str, sizeof(uuid_str),
                "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                file_info.startup_id[0], file_info.startup_id[1], file_info.startup_id[2], file_info.startup_id[3],
                file_info.startup_id[4], file_info.startup_id[5], file_info.startup_id[6], file_info.startup_id[7],
                file_info.startup_id[8], file_info.startup_id[9], file_info.startup_id[10], file_info.startup_id[11],
                file_info.startup_id[12], file_info.startup_id[13], file_info.startup_id[14], file_info.startup_id[15]);
        file_obj["uuid"] = uuid_str;
    }
    
    String json_str;
    serializeJson(doc, json_str);
    request->send(200, "application/json", json_str);
}

void WiFiManager::handle_log_download(AsyncWebServerRequest* request) {
    if (!request->hasParam("file")) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing file parameter\"}");
        return;
    }
    
    String filename = request->getParam("file")->value();
    
    Serial.printf("[WiFi] Download requested: %s\n", filename.c_str());
    
    // Stream entire flash partition as .opl file
    // Note: 'index' parameter is the byte offset that AsyncWebServer tracks for us
    AsyncWebServerResponse* response = request->beginChunkedResponse(
        "application/octet-stream",
        [filename](uint8_t* buffer, size_t maxLen, size_t index) -> size_t {
            // index is the byte offset provided by AsyncWebServer - use it directly
            if (index == 0) {
                Serial.println("[WiFi] Starting flash stream...");
            }

            // Read from flash partition at the current offset
            size_t bytes_read = LogFileManager::read_flash(index, buffer, maxLen);

            if (bytes_read == 0) {
                Serial.printf("[WiFi] Stream complete: %zu total bytes\n", index);
            } else if (index % (32 * 1024) == 0 && index > 0) {
                // Progress indicator every 32KB
                Serial.printf("[WiFi] Streamed %zu KB...\n", index / 1024);
            }

            return bytes_read;
        }
    );
    
    response->addHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
    request->send(response);
}

void WiFiManager::handle_log_delete(AsyncWebServerRequest* request) {
    if (!request->hasParam("file")) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing file parameter\"}");
        return;
    }
    
    String filename = request->getParam("file")->value();
    
    // Flash version: can only erase entire partition
    if (LogFileManager::erase_all_data()) {
        request->send(200, "application/json", "{\"success\":true,\"message\":\"All data erased\"}");
    } else {
        request->send(500, "application/json", "{\"success\":false,\"error\":\"Erase failed\"}");
    }
}

void WiFiManager::handle_logs_delete_all(AsyncWebServerRequest* request) {
    // Suspend logging during erase
    LogFileManager::set_download_active(true);
    
    bool success = LogFileManager::erase_all_data();
    
    LogFileManager::set_download_active(false);
    
    JsonDocument doc;
    doc["success"] = success;
    doc["message"] = success ? "Partition erased" : "Erase failed";
    
    String json_str;
    serializeJson(doc, json_str);
    request->send(200, "application/json", json_str);
}

