#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"

static const char *TAG = "UDDI";

// HTML content for the web interface
static const char *html_page = 
"<!DOCTYPE html>"
"<html>"
"<head>"
"<meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
"<title>U.D.D.I Service Bench</title>"
"<style>"
"body { font-family: Arial, sans-serif; margin: 20px; background: #1a1a1a; color: #fff; }"
"h1 { color: #00ff88; text-align: center; }"
".container { max-width: 800px; margin: 0 auto; }"
".card { background: #2a2a2a; padding: 20px; margin: 15px 0; border-radius: 10px; box-shadow: 0 4px 6px rgba(0,0,0,0.3); }"
".status { font-size: 24px; font-weight: bold; color: #00ff88; }"
".label { color: #888; margin-bottom: 5px; }"
"button { background: #00ff88; color: #000; border: none; padding: 12px 24px; "
"font-size: 16px; border-radius: 5px; cursor: pointer; margin: 5px; }"
"button:hover { background: #00cc6a; }"
"button:active { transform: scale(0.95); }"
".value { font-size: 32px; font-weight: bold; margin: 10px 0; }"
".unit { font-size: 18px; color: #888; }"
"</style>"
"</head>"
"<body>"
"<div class='container'>"
"<h1>🛠️ U.D.D.I Service Bench</h1>"
"<div class='card'>"
"<div class='label'>🔋 Battery Voltage</div>"
"<div class='value' id='voltage'>--.- <span class='unit'>V</span></div>"
"<button onclick='resetBattery()'>Reset Battery</button>"
"</div>"
"<div class='card'>"
"<div class='label'>⚡ Motor RPM</div>"
"<div class='value' id='rpm'>---- <span class='unit'>RPM</span></div>"
"<button onclick='startMotor()'>Start Motor</button>"
"<button onclick='stopMotor()'>Stop Motor</button>"
"</div>"
"<div class='card'>"
"<div class='label'>⏱️ System Uptime</div>"
"<div class='status' id='uptime'>0s</div>"
"</div>"
"<div class='card'>"
"<div class='label'>📶 WiFi Configuration</div>"
"<button onclick='scanNetworks()'>Scan Networks</button>"
"<button onclick='clearWiFi()' style='background:#ff4444;margin-left:5px;'>Clear Saved WiFi</button><br>"
"<select id='wifiList' style='width:100%;margin:5px 0;padding:8px;background:#1a1a1a;color:#fff;border:1px solid #444;border-radius:5px;'>"
"<option value=''>Select Network...</option>"
"</select><br>"
"<input type='text' id='ssid' placeholder='SSID' style='width:100%;margin:5px 0;padding:8px;background:#1a1a1a;color:#fff;border:1px solid #444;border-radius:5px;'/><br>"
"<input type='password' id='password' placeholder='Password' style='width:100%;margin:5px 0;padding:8px;background:#1a1a1a;color:#fff;border:1px solid #444;border-radius:5px;'/><br>"
"<button onclick='connectWiFi()'>Connect to WiFi</button>"
"<div id='wifiStatus' style='margin-top:10px;color:#00ff88;'></div>"
"</div>"
"<div class='card'>"
"<div class='label'>📡 OTA Update</div>"
"<form id='otaForm' enctype='multipart/form-data'>"
"<input type='file' id='firmware' accept='.bin' style='margin:10px 0;'/><br>"
"<button type='button' onclick='uploadFirmware()'>Upload Firmware</button>"
"</form>"
"<div id='otaStatus' style='margin-top:10px;color:#00ff88;'></div>"
"</div>"
"</div>"
"<script>"
"let startTime = Date.now();"
"function updateData() {"
"  fetch('/api/status')"
"  .then(r => r.json())"
"  .then(data => {"
"    document.getElementById('voltage').innerHTML = data.battery.toFixed(1) + ' <span class=\"unit\">V</span>';" 
"    document.getElementById('rpm').innerHTML = data.rpm + ' <span class=\"unit\">RPM</span>';" 
"    let uptime = Math.floor((Date.now() - startTime) / 1000);"
"    document.getElementById('uptime').textContent = uptime + 's';"
"  });"
"}"
"function updateWiFiStatus() {"
"  fetch('/api/wifi/status')"
"  .then(r => r.json())"
"  .then(data => {"
"    const status = document.getElementById('wifiStatus');"
"    if (data.connected) {"
"      status.innerHTML = '✓ Connected to <strong>' + data.ssid + '</strong><br>IP: ' + data.ip;"
"      status.style.color = '#00ff88';"
"    } else {"
"      status.textContent = data.message;"
"      status.style.color = '#ff8800';"
"    }"
"  });"
"}"
"function resetBattery() { fetch('/api/battery/reset', {method: 'POST'}); }"
"function startMotor() { fetch('/api/motor/start', {method: 'POST'}); }"
"function stopMotor() { fetch('/api/motor/stop', {method: 'POST'}); }"
"function scanNetworks() {"
"  const status = document.getElementById('wifiStatus');"
"  const list = document.getElementById('wifiList');"
"  status.textContent = 'Scanning...';"
"  fetch('/api/wifi/scan')"
"  .then(r => r.json())"
"  .then(networks => {"
"    list.innerHTML = '<option value=\"\">' + 'Select Network...' + '</option>';"
"    networks.forEach(n => {"
"      const opt = document.createElement('option');"
"      opt.value = n.ssid;"
"      opt.textContent = n.ssid + ' (' + n.rssi + ' dBm)';"
"      list.appendChild(opt);"
"    });"
"    status.textContent = 'Found ' + networks.length + ' networks';"
"  })"
"  .catch(e => { status.textContent = 'Scan failed: ' + e; });"
"}"
"document.getElementById('wifiList').addEventListener('change', function() {"
"  document.getElementById('ssid').value = this.value;"
"});"
"function connectWiFi() {"
"  const ssid = document.getElementById('ssid').value;"
"  const password = document.getElementById('password').value;"
"  const status = document.getElementById('wifiStatus');"
"  if (!ssid) { alert('Please enter SSID'); return; }"
"  status.textContent = 'Connecting...';"
"  fetch('/api/wifi/connect', {"
"    method: 'POST',"
"    headers: {'Content-Type': 'application/json'},"
"    body: JSON.stringify({ssid: ssid, password: password})"
"  })"
"  .then(r => r.text())"
"  .then(msg => { status.textContent = msg; updateWiFiStatus(); })"
"  .catch(e => { status.textContent = 'Connection failed: ' + e; });"
"}"
"function clearWiFi() {"
"  const status = document.getElementById('wifiStatus');"
"  if (!confirm('Clear saved WiFi credentials?')) return;"
"  status.textContent = 'Clearing...';"
"  fetch('/api/wifi/clear', { method: 'POST' })"
"  .then(r => r.text())"
"  .then(msg => { status.textContent = msg; })"
"  .catch(e => { status.textContent = 'Clear failed: ' + e; });"
"}"
"function uploadFirmware() {"
"  const file = document.getElementById('firmware').files[0];"
"  if (!file) { alert('Please select a firmware file'); return; }"
"  const status = document.getElementById('otaStatus');"
"  status.textContent = 'Uploading...';"
"  const formData = new FormData();"
"  formData.append('firmware', file);"
"  fetch('/api/ota/update', { method: 'POST', body: formData })"
"  .then(r => r.text())"
"  .then(msg => { status.textContent = msg; })"
"  .catch(e => { status.textContent = 'Upload failed: ' + e; });"
"}"
"setInterval(updateData, 500);"
"setInterval(updateWiFiStatus, 1000);"
"updateData();"
"updateWiFiStatus();"
"</script>"
"</body>"
"</html>";

// Simulated sensor data
static float battery_voltage = 12.6;
static int motor_rpm = 0;

// WiFi connection status tracking
static bool wifi_connected = false;
static char wifi_connected_ssid[33] = "";
static char wifi_ip_address[16] = "";
static char wifi_status_message[64] = "Not connected";

// HTTP GET handler for main page
static esp_err_t root_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_page, strlen(html_page));
    return ESP_OK;
}

// HTTP GET handler for status API
static esp_err_t status_handler(httpd_req_t *req)
{
    char json[200];
    snprintf(json, sizeof(json), 
        "{\"battery\":%.1f,\"rpm\":%d}",
        battery_voltage, motor_rpm);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
    return ESP_OK;
}

// HTTP GET handler for WiFi status API
static esp_err_t wifi_status_handler(httpd_req_t *req)
{
    char json[256];
    snprintf(json, sizeof(json), 
        "{\"connected\":%s,\"ssid\":\"%s\",\"ip\":\"%s\",\"message\":\"%s\"}",
        wifi_connected ? "true" : "false",
        wifi_connected_ssid,
        wifi_ip_address,
        wifi_status_message);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
    return ESP_OK;
}

// HTTP POST handler for battery reset
static esp_err_t battery_reset_handler(httpd_req_t *req)
{
    battery_voltage = 12.6 + (rand() % 10) * 0.1;
    ESP_LOGI(TAG, "Battery reset! Voltage: %.1fV", battery_voltage);
    
    httpd_resp_send(req, "OK", 2);
    return ESP_OK;
}

// HTTP POST handler for motor start
static esp_err_t motor_start_handler(httpd_req_t *req)
{
    motor_rpm = 3200 + (rand() % 800);
    ESP_LOGI(TAG, "Motor started: %d RPM", motor_rpm);
    
    httpd_resp_send(req, "OK", 2);
    return ESP_OK;
}

// HTTP POST handler for motor stop
static esp_err_t motor_stop_handler(httpd_req_t *req)
{
    motor_rpm = 0;
    ESP_LOGI(TAG, "Motor stopped");
    
    httpd_resp_send(req, "OK", 2);
    return ESP_OK;
}

// HTTP POST handler to clear WiFi credentials
static esp_err_t wifi_clear_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Clearing WiFi credentials from NVS");
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("wifi", NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        nvs_erase_key(nvs_handle, "ssid");
        nvs_erase_key(nvs_handle, "password");
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        
        // Switch back to AP-only mode
        esp_wifi_set_mode(WIFI_MODE_AP);
        
        ESP_LOGI(TAG, "WiFi credentials cleared, switched to AP mode");
        httpd_resp_sendstr(req, "WiFi credentials cleared! Device in AP-only mode.");
    } else {
        ESP_LOGE(TAG, "Failed to open NVS");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to clear credentials");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

// HTTP POST handler for OTA update
static esp_err_t ota_update_handler(httpd_req_t *req)
{
    char buf[1024];
    esp_ota_handle_t ota_handle;
    const esp_partition_t *update_partition = NULL;
    const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_partition_t *running = esp_ota_get_running_partition();
    
    ESP_LOGI(TAG, "Starting OTA update...");
    
    if (configured != running) {
        ESP_LOGW(TAG, "Configured OTA boot partition at offset 0x%08lx, but running from offset 0x%08lx",
                 configured->address, running->address);
    }
    
    update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "No OTA partition found");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No OTA partition");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%lx",
             update_partition->subtype, update_partition->address);
    
    esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA begin failed");
        return ESP_FAIL;
    }
    
    int remaining = req->content_len;
    int received = 0;
    
    while (remaining > 0) {
        int recv_len = httpd_req_recv(req, buf, sizeof(buf) < remaining ? sizeof(buf) : remaining);
        if (recv_len <= 0) {
            if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive firmware");
            return ESP_FAIL;
        }
        
        err = esp_ota_write(ota_handle, buf, recv_len);
        if (err != ESP_OK) {
            esp_ota_abort(ota_handle);
            ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA write failed");
            return ESP_FAIL;
        }
        
        received += recv_len;
        remaining -= recv_len;
        
        if (received % 102400 == 0) {
            ESP_LOGI(TAG, "Written %d / %d bytes", received, req->content_len);
        }
    }
    
    ESP_LOGI(TAG, "Total written: %d bytes", received);
    
    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA end failed");
        return ESP_FAIL;
    }
    
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Set boot partition failed");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "OTA update successful! Rebooting in 3 seconds...");
    httpd_resp_sendstr(req, "Update successful! Device rebooting...");
    
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    esp_restart();
    
    return ESP_OK;
}

// HTTP GET handler for WiFi scan
static esp_err_t wifi_scan_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Starting WiFi scan...");
    
    // Disconnect STA if it's trying to connect (this blocks scanning)
    esp_wifi_disconnect();
    vTaskDelay(200 / portTICK_PERIOD_MS);
    
    // Clear any previous scan results
    esp_wifi_scan_stop();
    vTaskDelay(100 / portTICK_PERIOD_MS);
    
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time = {
            .active = {
                .min = 100,
                .max = 300
            }
        }
    };
    
    esp_err_t err = esp_wifi_scan_start(&scan_config, true);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WiFi scan failed to start: %s", esp_err_to_name(err));
        httpd_resp_sendstr(req, "[]");
        return ESP_OK;
    }
    
    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    
    ESP_LOGI(TAG, "Scan complete, found %d networks", ap_count);
    
    if (ap_count == 0) {
        httpd_resp_sendstr(req, "[]");
        return ESP_OK;
    }
    
    wifi_ap_record_t *ap_records = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * ap_count);
    if (ap_records == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
        return ESP_FAIL;
    }
    
    esp_wifi_scan_get_ap_records(&ap_count, ap_records);
    
    // Build JSON response
    char *json = (char *)malloc(4096);
    if (json == NULL) {
        free(ap_records);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
        return ESP_FAIL;
    }
    
    int offset = sprintf(json, "[");
    for (int i = 0; i < ap_count && i < 20; i++) {
        offset += sprintf(json + offset, "%s{\"ssid\":\"%s\",\"rssi\":%d}",
                         i > 0 ? "," : "",
                         ap_records[i].ssid,
                         ap_records[i].rssi);
    }
    offset += sprintf(json + offset, "]");
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, offset);
    
    free(json);
    free(ap_records);
    
    ESP_LOGI(TAG, "WiFi scan complete: %d networks found", ap_count);
    return ESP_OK;
}

// Event handler for WiFi station events
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "Station started, connecting...");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_connected = false;
        wifi_ip_address[0] = '\0';
        wifi_event_sta_disconnected_t* disconn = (wifi_event_sta_disconnected_t*) event_data;
        const char* reason_str = "Unknown";
        
        // Decode disconnection reason
        switch(disconn->reason) {
            case WIFI_REASON_UNSPECIFIED: reason_str = "Unspecified"; break;
            case WIFI_REASON_AUTH_EXPIRE: reason_str = "Auth expired"; break;
            case WIFI_REASON_AUTH_LEAVE: reason_str = "Auth leave"; break;
            case WIFI_REASON_ASSOC_EXPIRE: reason_str = "Assoc expired"; break;
            case WIFI_REASON_ASSOC_TOOMANY: reason_str = "Too many assoc"; break;
            case WIFI_REASON_NOT_AUTHED: reason_str = "Not authenticated"; break;
            case WIFI_REASON_NOT_ASSOCED: reason_str = "Not associated"; break;
            case WIFI_REASON_ASSOC_LEAVE: reason_str = "Assoc leave"; break;
            case WIFI_REASON_ASSOC_NOT_AUTHED: reason_str = "Assoc not authed"; break;
            case WIFI_REASON_DISASSOC_PWRCAP_BAD: reason_str = "Bad power cap"; break;
            case WIFI_REASON_DISASSOC_SUPCHAN_BAD: reason_str = "Bad sup channel"; break;
            case WIFI_REASON_BSS_TRANSITION_DISASSOC: reason_str = "BSS transition"; break;
            case WIFI_REASON_IE_INVALID: reason_str = "Invalid IE"; break;
            case WIFI_REASON_MIC_FAILURE: reason_str = "MIC failure"; break;
            case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT: reason_str = "4-way handshake timeout"; break;
            case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT: reason_str = "Group key timeout"; break;
            case WIFI_REASON_IE_IN_4WAY_DIFFERS: reason_str = "IE differs in 4-way"; break;
            case WIFI_REASON_GROUP_CIPHER_INVALID: reason_str = "Invalid group cipher"; break;
            case WIFI_REASON_PAIRWISE_CIPHER_INVALID: reason_str = "Invalid pairwise cipher"; break;
            case WIFI_REASON_AKMP_INVALID: reason_str = "Invalid AKMP"; break;
            case WIFI_REASON_UNSUPP_RSN_IE_VERSION: reason_str = "Unsupported RSN IE version"; break;
            case WIFI_REASON_INVALID_RSN_IE_CAP: reason_str = "Invalid RSN IE cap"; break;
            case WIFI_REASON_802_1X_AUTH_FAILED: reason_str = "802.1X auth failed"; break;
            case WIFI_REASON_CIPHER_SUITE_REJECTED: reason_str = "Cipher suite rejected"; break;
            case WIFI_REASON_INVALID_PMKID: reason_str = "Invalid PMKID"; break;
            case WIFI_REASON_BEACON_TIMEOUT: reason_str = "Beacon timeout"; break;
            case WIFI_REASON_NO_AP_FOUND: reason_str = "No AP found"; break;
            case WIFI_REASON_AUTH_FAIL: reason_str = "Authentication failed (wrong password?)"; break;
            case WIFI_REASON_ASSOC_FAIL: reason_str = "Association failed"; break;
            case WIFI_REASON_HANDSHAKE_TIMEOUT: reason_str = "Handshake timeout"; break;
            case WIFI_REASON_CONNECTION_FAIL: reason_str = "Connection failed"; break;
            case WIFI_REASON_AP_TSF_RESET: reason_str = "AP TSF reset"; break;
            case WIFI_REASON_ROAMING: reason_str = "Roaming"; break;
        }
        
        ESP_LOGW(TAG, "Disconnected from AP (reason: %d - %s), retrying...", disconn->reason, reason_str);
        
        // Show SSID that failed
        if (disconn->ssid_len > 0) {
            ESP_LOGW(TAG, "Failed SSID: %.*s", disconn->ssid_len, disconn->ssid);
            snprintf(wifi_status_message, sizeof(wifi_status_message), 
                     "Disconnected: %s", reason_str);
        }
        
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        wifi_connected = true;
        snprintf(wifi_ip_address, sizeof(wifi_ip_address), IPSTR, IP2STR(&event->ip_info.ip));
        snprintf(wifi_status_message, sizeof(wifi_status_message), 
                 "✓ Connected! IP: %s", wifi_ip_address);
        ESP_LOGI(TAG, "✓ Connected! Got IP: %s", wifi_ip_address);
        
        // Get the connected SSID
        wifi_config_t wifi_config;
        if (esp_wifi_get_config(WIFI_IF_STA, &wifi_config) == ESP_OK) {
            strncpy(wifi_connected_ssid, (char*)wifi_config.sta.ssid, sizeof(wifi_connected_ssid) - 1);
            wifi_connected_ssid[sizeof(wifi_connected_ssid) - 1] = '\0';
        }
    }
}

// HTTP POST handler for WiFi connect
static esp_err_t wifi_connect_handler(httpd_req_t *req)
{
    char buf[256];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request");
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    // Parse JSON (simple manual parsing)
    char ssid[33] = {0};
    char password[64] = {0};
    
    char *ssid_start = strstr(buf, "\"ssid\":\"");
    char *pass_start = strstr(buf, "\"password\":\"");
    
    if (ssid_start) {
        ssid_start += 8;  // Move past "ssid":" to the start of the value
        char *ssid_end = strchr(ssid_start, '\"');
        if (ssid_end) {
            int len = ssid_end - ssid_start;
            if (len < 33) {
                strncpy(ssid, ssid_start, len);
            }
        }
    }
    
    if (pass_start) {
        pass_start += 12;  // Move past "password":" to the start of the value
        char *pass_end = strchr(pass_start, '\"');
        if (pass_end) {
            int len = pass_end - pass_start;
            if (len < 64) {
                strncpy(password, pass_start, len);
            }
        }
    }
    
    if (strlen(ssid) == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "SSID required");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Connecting to WiFi: %s", ssid);
    
    // Save to NVS
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("wifi", NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        nvs_set_str(nvs_handle, "ssid", ssid);
        nvs_set_str(nvs_handle, "password", password);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        ESP_LOGI(TAG, "WiFi credentials saved to NVS");
    }
    
    // Configure WiFi station
    wifi_config_t wifi_config = {};
    strcpy((char*)wifi_config.sta.ssid, ssid);
    strcpy((char*)wifi_config.sta.password, password);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    
    // Switch to AP+STA mode
    esp_wifi_set_mode(WIFI_MODE_APSTA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    
    // Register event handler if not already registered
    static bool event_handler_registered = false;
    if (!event_handler_registered) {
        esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
        esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL);
        event_handler_registered = true;
    }
    
    esp_wifi_connect();
    
    httpd_resp_sendstr(req, "Connecting to WiFi... Check status in a few seconds");
    return ESP_OK;
}

// Start web server
static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.lru_purge_enable = true;

    ESP_LOGI(TAG, "Starting HTTP server on port: %d", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Register URI handlers
        httpd_uri_t root_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &root_uri);

        httpd_uri_t status_uri = {
            .uri = "/api/status",
            .method = HTTP_GET,
            .handler = status_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &status_uri);

        httpd_uri_t battery_reset_uri = {
            .uri = "/api/battery/reset",
            .method = HTTP_POST,
            .handler = battery_reset_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &battery_reset_uri);

        httpd_uri_t motor_start_uri = {
            .uri = "/api/motor/start",
            .method = HTTP_POST,
            .handler = motor_start_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &motor_start_uri);

        httpd_uri_t motor_stop_uri = {
            .uri = "/api/motor/stop",
            .method = HTTP_POST,
            .handler = motor_stop_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &motor_stop_uri);

        httpd_uri_t ota_update_uri = {
            .uri = "/api/ota/update",
            .method = HTTP_POST,
            .handler = ota_update_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &ota_update_uri);

        httpd_uri_t wifi_status_uri = {
            .uri = "/api/wifi/status",
            .method = HTTP_GET,
            .handler = wifi_status_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &wifi_status_uri);

        httpd_uri_t wifi_scan_uri = {
            .uri = "/api/wifi/scan",
            .method = HTTP_GET,
            .handler = wifi_scan_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &wifi_scan_uri);

        httpd_uri_t wifi_connect_uri = {
            .uri = "/api/wifi/connect",
            .method = HTTP_POST,
            .handler = wifi_connect_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &wifi_connect_uri);

        httpd_uri_t wifi_clear_uri = {
            .uri = "/api/wifi/clear",
            .method = HTTP_POST,
            .handler = wifi_clear_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &wifi_clear_uri);

        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

extern "C" void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "ESP32-C6 Service Bench Starting (ESP-IDF)");
    ESP_LOGI(TAG, "========================================");
    
    // Initialize WiFi
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();  // Create STA interface for scanning

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handler for WiFi events
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    // Configure AP
    wifi_config_t wifi_config = {};
    strcpy((char*)wifi_config.ap.ssid, "ServiceBench");
    strcpy((char*)wifi_config.ap.password, "tech1234");  // Must be 8+ characters
    wifi_config.ap.ssid_len = strlen("ServiceBench");
    wifi_config.ap.channel = 1;
    wifi_config.ap.max_connection = 4;
    wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;

    if (strlen("tech1234") == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    // Start in APSTA mode to allow scanning while AP is active
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi AP Started Successfully!");
    ESP_LOGI(TAG, "SSID: ServiceBench");
    ESP_LOGI(TAG, "Password: tech1234");
    ESP_LOGI(TAG, "IP Address: 192.168.4.1");

    // Start web server
    httpd_handle_t server = start_webserver();
    if (server) {
        ESP_LOGI(TAG, "Web server started!");
        ESP_LOGI(TAG, "Open http://192.168.4.1 in your browser");
    }

    // Update sensor data periodically
    while(1) {
        // Simulate sensor readings
        battery_voltage = 12.0 + (rand() % 15) * 0.1;
        if (motor_rpm > 0) {
            motor_rpm = 3000 + (rand() % 1000);
        }
        
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}
