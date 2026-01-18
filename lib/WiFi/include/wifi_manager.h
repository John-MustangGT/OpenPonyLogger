#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>

/**
 * @brief WiFi manager for AP mode with WebSocket support
 * Starts as access point "PonyLogger-XXXX" (where XXXX is last 4 MAC address digits)
 * Serves a web page at http://192.168.4.1 with real-time sensor data via WebSocket
 */
class WiFiManager {
public:
    /**
     * @brief Initialize WiFi in AP mode and start web server
     * @return true if successful, false otherwise
     */
    static bool init();
    
    /**
     * @brief Get the AP SSID
     * @return SSID string (e.g., "PonyLogger-1234")
     */
    static String get_ssid();
    
    /**
     * @brief Get the AP password
     * @return Password string (empty for open network)
     */
    static String get_password();
    
    /**
     * @brief Get the number of connected WebSocket clients
     * @return Number of clients
     */
    static uint16_t get_client_count();
    
    /**
     * @brief Send JSON data to all connected WebSocket clients
     * @param json JSON string to send (e.g., "{\"type\": \"sensor\", ...}")
     */
    static void broadcast_json(const char* json);
    
    /**
     * @brief Check if WiFi is initialized
     * @return true if initialized and running, false otherwise
     */
    static bool is_initialized();

private:
    static AsyncWebServer* m_server;
    static AsyncWebSocket* m_websocket;
    static bool m_initialized;
    static String m_ssid;
    static String m_password;
    
    /**
     * @brief Handle HTTP request for root path (/)
     */
    static void handle_root(AsyncWebServerRequest* request);
    
    /**
     * @brief Handle GET request for configuration
     */
    static void handle_config_get(AsyncWebServerRequest* request);
    
    /**
     * @brief Handle POST request to save configuration
     */
    static void handle_config_post(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total);
    
    /**
     * @brief Handle GET request for about/version information
     */
    static void handle_about(AsyncWebServerRequest* request);
    
    /**
     * @brief Handle WebSocket events (connect, disconnect, message)
     */
    static void handle_websocket_event(AsyncWebSocket* server, AsyncWebSocketClient* client,
                                       AwsEventType type, void* arg, uint8_t* data, size_t len);
};

#endif // WIFI_MANAGER_H
