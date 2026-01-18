#include "wifi_manager.h"
#include "web_pages.h"
#include "config_manager.h"
#include "icar_ble_driver.h"
#include "version_info.h"
#include "log_file_manager_flash.h"  // Flash-based file manager
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
    
    // Ensure ConfigManager is initialized
    if (!ConfigManager::init()) {
        Serial.println("[WiFi] WARNING: ConfigManager not ready, using defaults");
    }
    
    // Get configuration with fallback
    logging_config_t config = ConfigManager::get_current();
    
    // Validate network config has valid data, use defaults if not
    if (config.network.ssid[0] == '\0' || strlen(config.network.ssid) == 0) {
        Serial.println("[WiFi] WARNING: Invalid SSID in config, using default");
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
    
    Serial.printf("[WiFi] Starting AP with SSID: %s\n", m_ssid.c_str());
    
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
        Serial.println("[WiFi] WARNING: Invalid IP in config, using 192.168.4.1");
        ap_ip = IPAddress(192, 168, 4, 1);
        netmask = IPAddress(255, 255, 255, 0);
    }
    
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
    m_server->on("/api/restart", HTTP_POST, handle_restart);
    
    // Log file management routes
    m_server->on("/api/logs", HTTP_GET, handle_logs_list);
    m_server->on("/api/logs/download", HTTP_GET, handle_log_download);
    m_server->on("/api/logs/delete", HTTP_POST, handle_log_delete);
    m_server->on("/api/logs/delete-all", HTTP_POST, handle_logs_delete_all);
    
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
        Serial.printf("[WiFi] Invalid JSON length: %d\n", len);
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
    Serial.println("[WiFi] handle_config_get called");
    
    if (!ConfigManager::init()) {
        Serial.println("[WiFi] ERROR: Config manager not initialized");
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
    
    Serial.printf("[WiFi] Network config - SSID: %s, IP: %s\n", config.network.ssid, ip_str);
    
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
    
    String json_str;
    serializeJson(doc, json_str);
    
    Serial.printf("[WiFi] Sending config response (%d bytes)\n", json_str.length());
    request->send(200, "application/json", json_str);
}

void WiFiManager::handle_config_post(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
    Serial.printf("[WiFi] handle_config_post called (len=%d, total=%d)\n", len, total);
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, (const char*)data, len);
    
    if (error) {
        Serial.printf("[WiFi] JSON parse error: %s\n", error.c_str());
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
        return;
    }
    
    logging_config_t config = ConfigManager::get_current();  // Start with current config
    config.main_loop_hz = doc["main_loop_hz"] | 10;
    config.gps_hz = doc["gps_hz"] | 10;
    config.imu_hz = doc["imu_hz"] | 10;
    config.obd_hz = doc["obd_hz"] | 10;
    config.obd_ble_enabled = doc["obd_ble_enabled"] | true;
    
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
    
    #ifdef PROJECT_VERSION
        doc["version"] = PROJECT_VERSION;
    #else
        doc["version"] = "1.0.0";
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
    
    String json_str;
    serializeJson(doc, json_str);
    request->send(200, "application/json", json_str);
}

void WiFiManager::handle_restart(AsyncWebServerRequest* request) {
    Serial.println("[WiFi] Restart requested via web interface");
    request->send(200, "application/json", "{\"success\":true,\"message\":\"Restarting device...\"}");
    
    // Delay restart to allow response to be sent
    delay(500);
    ESP.restart();
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
    
    // Force rescan if requested
    bool force_rescan = request->hasParam("rescan") && request->getParam("rescan")->value() == "true";
    LogFileManager::scan_log_files(force_rescan);
    
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
    AsyncWebServerResponse* response = request->beginChunkedResponse(
        "application/octet-stream",
        [filename](uint8_t* buffer, size_t maxLen, size_t index) -> size_t {
            static size_t flash_offset = 0;
            
            // Reset on first chunk
            if (index == 0) {
                flash_offset = 0;
                Serial.println("[WiFi] Starting flash stream...");
            }
            
            // Read from flash partition
            size_t bytes_read = LogFileManager::read_flash(flash_offset, buffer, maxLen);
            flash_offset += bytes_read;
            
            if (bytes_read == 0) {
                Serial.printf("[WiFi] Stream complete: %d total bytes\n", flash_offset);
            }
            
            return bytes_read;
        }
    );
    
    response->addHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
    request->send(response);
}
                    if (!download_file.available()) {
                        // End of file
                        download_file.close();
                        file_opened = false;
                        
                        if (delete_after) {
                            Serial.printf("[WiFi] Deleting after download: %s\n", filename.c_str());
                            LogFileManager::delete_file(filename);
                        }
                        
                        LogFileManager::set_download_active(false);
                        Serial.printf("[WiFi] Download complete: %s\n", filename.c_str());
                        return bytes_written;
                    }
                    
                    // Read and decompress next block
                    decompressed_size = LogFileManager::read_and_decompress_block(
                        download_file, current_block, decompressed_buffer, sizeof(decompressed_buffer)
                    );
                    
                    if (decompressed_size == 0) {
                        // Error or end of valid blocks
                        download_file.close();
                        file_opened = false;
                        LogFileManager::set_download_active(false);
                        return bytes_written;
                    }
                    
                    decompressed_offset = 0;
                }
                
                // Copy from decompressed buffer
                size_t remaining = decompressed_size - decompressed_offset;
                size_t space_left = maxLen - bytes_written;
                size_t to_copy = min(remaining, space_left);
                
                memcpy(buffer + bytes_written, decompressed_buffer + decompressed_offset, to_copy);
                bytes_written += to_copy;
                decompressed_offset += to_copy;
            }
            
            return bytes_written;
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
    
    if (LogFileManager::delete_file(filename)) {
        request->send(200, "application/json", "{\"success\":true,\"message\":\"File deleted\"}");
    } else {
        request->send(500, "application/json", "{\"success\":false,\"error\":\"Delete failed\"}");
    }
}

void WiFiManager::handle_logs_delete_all(AsyncWebServerRequest* request) {
    // Suspend logging during bulk delete
    LogFileManager::set_download_active(true);
    
    uint32_t deleted_count = LogFileManager::delete_all_files();
    
    LogFileManager::set_download_active(false);
    
    JsonDocument doc;
    doc["success"] = true;
    doc["deleted_count"] = deleted_count;
    
    String json_str;
    serializeJson(doc, json_str);
    request->send(200, "application/json", json_str);
}

