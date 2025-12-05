# Universal Drone Diagnostics Interface (UDDI)

ESP32-C6 based web service bench for drone diagnostics and testing, featuring real-time monitoring via WebSocket.

## Hardware

- **Board**: Seeed Studio XIAO ESP32-C6
- **MCU**: ESP32-C6 (RISC-V, 160MHz)
- **RAM**: 512KB SRAM
- **Flash**: 4MB
- **Connectivity**: WiFi 6, Bluetooth 5

## Features

### Core Functionality
- **Dual WiFi Mode**: Access Point (ServiceBench) + Station mode for home network
- **Web Dashboard**: Real-time interface at `http://192.168.4.1`
- **WiFi Management**: Scan networks, connect to home WiFi, persistent credentials
- **Live Status Updates**: Connection state, IP address, SSID display
- **OTA Firmware Updates**: Wireless firmware updates via web interface
- **REST API**: HTTP endpoints for device control and status

### Testing Capabilities
- **Battery Testing**: Voltage monitoring and reset operations
- **Motor Testing**: RPM monitoring and control
- **System Monitoring**: Uptime tracking and diagnostics

## Web Interface

The service bench provides a responsive dark-themed dashboard with:
- 🔋 **Battery Voltage**: Live monitoring with reset controls
- ⚡ **Motor RPM**: Real-time RPM display with start/stop controls
- ⏱️ **System Uptime**: Session duration tracking
- 📶 **WiFi Configuration**: 
  - Network scanner with signal strength (RSSI)
  - Dropdown network selection
  - Password-protected connection
  - Real-time connection status (✓ Connected / Disconnected)
  - IP address display when connected
  - Clear credentials button
- 📡 **OTA Updates**: Firmware upload interface (.bin files)

## Project Structure

```
UDDI/
├── src/
│   └── main.cpp              # Main application code
├── boards/
│   └── seeed_xiao_esp32c6.json  # Custom board definition
├── platformio.ini            # PlatformIO configuration
└── README.md                 # This file
```

## Setup & Configuration

### Platform Configuration

This project uses **ESP-IDF framework** for native ESP32-C6 support:

```ini
platform = espressif32
board = esp32-c6-devkitc-1
framework = espidf
board_build.mcu = esp32c6
```

### Why ESP-IDF Instead of Arduino?

The Arduino framework for ESP32-C6 has limited support in PlatformIO. ESP-IDF provides:
- Native ESP32-C6 support from Espressif
- More stable WiFi and networking stack
- Better hardware abstraction layer
### Dependencies

ESP-IDF includes all necessary components:
- **WiFi stack** - Built-in ESP32-C6 WiFi support
- **FreeRTOS** - Real-time operating system
- **NVS** - Non-volatile storage
- **ESP HTTP Server** - Native web server (to be implemented)
- **lwIP** - Lightweight TCP/IP stack
- **ArduinoJson** (v7.x) - JSON serialization/deserialization
- **WiFiManager** (v2.0.17) - WiFi configuration management
- **Adafruit SSD1306** & **GFX Library** - Display support (optional)

## Building the Project

### Prerequisites

- PlatformIO installed (via VS Code extension or CLI)
- USB-C cable for connecting XIAO ESP32-C6

### Build Commands

```bash
# Build the project
~/.platformio/penv/bin/platformio run

# Upload to device
~/.platformio/penv/bin/platformio run --target upload

# Monitor serial output
~/.platformio/penv/bin/platformio device monitor
```

Or use the PlatformIO VS Code extension buttons.

## Usage

### Initial Setup

1. **Power on** the ESP32-C6 device
2. **Connect** to the `ServiceBench` WiFi network (password: `tech1234`)
3. **Open browser** to `http://192.168.4.1`
4. **Configure WiFi** (optional):
   - Click "Scan Networks"
   - Select your home WiFi from dropdown
   - Enter password
   - Click "Connect to WiFi"
   - Device will connect while maintaining AP mode
5. **Access from home network** (after WiFi setup):
   - Still accessible at `192.168.4.1` via ServiceBench AP
   - Also accessible at the IP shown in WiFi status (e.g., `http://10.0.0.17`)

### Testing Operations

- **Battery Voltage**: Monitor live readings, click "Reset Battery" to simulate new battery
- **Motor RPM**: Click "Start Motor" / "Stop Motor" to control motor simulation
- **System Uptime**: Automatically tracks session duration
- **WiFi Management**: Clear saved credentials to return to AP-only mode

### Firmware Updates

1. Build new firmware: `platformio run`
2. Navigate to OTA Update section in web interface
3. Select `.pio/build/esp32c6/firmware.bin`
4. Click "Upload Firmware"
5. Device will update and reboot automatically

## API Endpoints

### System Status

#### GET /
Main web interface (compressed HTML, gzip-encoded)

#### GET /api/status
Returns current sensor readings:
```json
{
  "battery": 12.6,
  "rpm": 3200
}
```

#### GET /api/wifi/status
Returns WiFi connection state:
```json
{
  "connected": true,
  "ssid": "Gordon Wifi",
  "ip": "10.0.0.17",
  "message": "✓ Connected! IP: 10.0.0.17"
}
```

### Device Control

#### POST /api/battery/reset
Resets battery voltage to random value (12.6-13.5V)

#### POST /api/motor/start
Starts motor simulation (RPM increases)

#### POST /api/motor/stop
Stops motor simulation (RPM returns to 0)

### WiFi Management

#### GET /api/wifi/scan
Scans for available WiFi networks:
```json
[
  {"ssid": "Network1", "rssi": -45},
  {"ssid": "Network2", "rssi": -67}
]
```

#### POST /api/wifi/connect
Connects to WiFi network:
```json
{
  "ssid": "YourNetwork",
  "password": "YourPassword"
}
```
Response: "WiFi credentials saved. Connecting..."

#### POST /api/wifi/clear
Clears saved WiFi credentials and returns to AP-only mode

### Firmware Management

#### POST /api/ota/update
Upload new firmware (.bin file)
- Content-Type: multipart/form-data
- Field name: "firmware"
- Response: "OTA update successful! Rebooting..." or error message

## Technical Implementation

### WiFi Configuration (APSTA Mode)
```cpp
// Create both AP and Station interfaces
esp_netif_create_default_wifi_ap();
esp_netif_create_default_wifi_sta();

// Configure for simultaneous AP + STA
wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
esp_wifi_init(&cfg);
esp_wifi_set_mode(WIFI_MODE_APSTA);

// AP Configuration
wifi_config_t ap_config = {
    .ap = {
        .ssid = "ServiceBench",
        .password = "tech1234",
        .max_connection = 4,
        .authmode = WIFI_AUTH_WPA2_PSK
    }
};

// Station credentials loaded from NVS
esp_wifi_set_config(WIFI_IF_AP, &ap_config);
esp_wifi_start();
```

### HTTP Server Architecture
- **Native ESP-IDF HTTP Server**: Lightweight, non-blocking
- **Compressed Content**: HTML served as gzip (3.8KB savings)
- **RESTful API**: JSON responses for all endpoints
- **Connection Header**: `Content-Encoding: gzip` for compressed responses

### Real-time Updates
- Status polling: 500ms interval for sensor data
- WiFi status polling: 1000ms interval for connection state
- Event-driven WiFi state changes
- Automatic UI updates without page refresh

### Persistent Storage (NVS)
```cpp
// WiFi credentials stored in NVS
nvs_handle_t nvs_handle;
nvs_open("storage", NVS_READWRITE, &nvs_handle);
nvs_set_str(nvs_handle, "ssid", ssid);
nvs_set_str(nvs_handle, "password", password);
nvs_commit(nvs_handle);
```

### OTA Update Process
1. Receive firmware via HTTP POST (multipart/form-data)
2. Write to inactive OTA partition
3. Validate partition integrity
4. Set boot partition to new firmware
5. Reboot device
6. On successful boot, mark partition as valid

### Memory Usage
- **RAM**: ~34KB (10.3% of 327KB)
- **Flash**: ~883KB (84.2% of 1MB partition)
- **Partition Scheme**: OTA-enabled (factory + ota_0 + ota_1)
  - Factory: 1MB @ 0x10000
  - OTA_0: 1MB @ 0x110000  
  - OTA_1: 1MB @ 0x210000

### WiFi Architecture
- **APSTA Mode**: Simultaneous AP + Station
  - AP: `192.168.4.1` (ServiceBench network - always accessible)
  - STA: Connects to home WiFi (credentials saved in NVS)
- **Event-Driven**: WiFi event handlers for connection management
- **Disconnect Reasons**: Detailed logging with 40+ reason codes decoded
- **Auto-Reconnect**: Persistent connection attempts with saved credentials

## Troubleshooting

### Build Issues

**Problem**: `Unknown board ID 'seeed_xiao_esp32c6'`
- **Solution**: Ensure `board_dir = boards` is set in `platformio.ini` and the custom board JSON exists

**Problem**: `This board doesn't support arduino framework!`
- **Solution**: Use the Tasmota platform as specified in `platformio.ini`

**Problem**: Library conflicts with AsyncTCP
- **Solution**: Remove ESPAsyncWebServer-esphome and use only mathieucarbou's versions

### Upload Issues

**Problem**: Device not detected
- **Solution**: 
  - Press BOOT button while connecting USB
  - Check USB cable supports data transfer
  - Install CH340 or CP2102 drivers if needed

### Runtime Issues

**Problem**: Cannot connect to ServiceBench WiFi
- **Solution**: 
  - Device creates its own AP - connect TO the device's network
  - SSID: `ServiceBench`, Password: `tech1234`
  - Device IP is always `192.168.4.1` on AP mode

**Problem**: WiFi scan shows 0 networks
- **Solution**: 
  - Ensure device is in APSTA mode (fixed in current version)
  - Check that WiFi is enabled on scanning device
  - Device needs station interface active to scan

**Problem**: Connection to home WiFi fails (reason 15: 4-way handshake timeout)
- **Solution**: 
  - Double-check WiFi password
  - Ensure WPA2/WPA3 compatibility
  - Check router allows new device connections

**Problem**: Dropdown menu drops first character of SSID
- **Solution**: 
  - Fixed in current version (JSON parsing offset corrected)
  - Update to latest firmware if issue persists

**Problem**: Clear WiFi button returns 404
- **Solution**: 
  - Fixed in current version (endpoint registered)
  - Upload latest firmware to enable feature

**Problem**: OTA update fails
- **Solution**:
  - Ensure .bin file is correct (from `.pio/build/esp32c6/`)
  - File must be <1MB (current partition limit)
  - Strong WiFi signal required during upload
## Development Notes

### ESP-IDF vs Arduino Framework

**Attempted Approach 1: Arduino Framework with Tasmota Platform**
- Used Tasmota's Arduino-ESP32 3.x platform
- Created custom board definitions
- Result: Compiled successfully but device bootlooped
- Issue: Arduino framework not stable enough for ESP32-C6 on this hardware

**Current Approach 2: ESP-IDF Framework (Native)**
- Uses Espressif's official ESP-IDF
- Better hardware support and stability
- More verbose but more control
- Production-ready for ESP32-C6

### ESP-IDF Programming Model

ESP-IDF uses FreeRTOS and C/C++:
- `app_main()` instead of `setup()` and `loop()`
- Tasks instead of loop-based programming
- Event-driven architecture
- Component-based systemke Arduino IDE
- More strict C++ compilation

## Memory Optimization

At 84% flash usage, optimization strategies are available:

### Implemented
- ✅ OTA partition scheme for wireless updates
- ✅ Efficient WiFi event handling
- ✅ Compressed HTML ready (saves 3.8KB - see `MEMORY_OPTIMIZATION.md`)

### Available Optimizations
- 📦 GZIP HTML compression: ~4KB savings
- 🎯 Compiler flags (`-Os`, `-ffunction-sections`): ~20-50KB savings
- 📏 Larger OTA partitions (1.5MB): ~500KB more space
- 🔧 Disable unused WiFi features: ~10-30KB savings

### Future Growth Strategy
When approaching 90% flash:
- Split into mode-specific firmwares (Service Mode / Drive Mode)
- OTA switch between modes as needed
- Each mode ~500KB instead of >1MB combined

See `MEMORY_OPTIMIZATION.md` for detailed guide and implementation steps.

## Future Enhancements

### Planned Features
- [ ] Real sensor integration (battery ADC, motor tachometer)
- [ ] Data logging to NVS/SD card
- [ ] Multiple test profiles
- [ ] Battery health analytics
- [ ] Motor efficiency calculations
- [ ] Bluetooth remote control
- [ ] mDNS support (access via `servicebench.local`)
- [ ] HTTPS support with self-signed certificates

### Code Quality
- [ ] Unit tests for WiFi handlers
- [ ] Integration tests for API endpoints
- [ ] Error recovery mechanisms
- [ ] Watchdog timer implementation

## License

This project is for educational and commercial use.

## Author

Built for universal drone service operations on ESP32-C6 hardware.
