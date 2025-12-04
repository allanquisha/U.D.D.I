# Universal Drone Service Bench (UDDI)

ESP32-C6 based web service bench for drone diagnostics and testing, featuring real-time monitoring via WebSocket.

## Hardware

- **Board**: Seeed Studio XIAO ESP32-C6
- **MCU**: ESP32-C6 (RISC-V, 160MHz)
- **RAM**: 320KB
- **Flash**: 8MB
- **Connectivity**: WiFi 6, Bluetooth 5

## Features

- **WiFi Access Point**: Creates `ServiceBench` network (password: `tech123`)
- **Web Interface**: Real-time dashboard at `http://192.168.4.1`
- **WebSocket Communication**: Live updates every 500ms
- **REST API**: HTTP endpoints for device control
- **Battery Testing**: Voltage monitoring and reset operations
- **Motor Testing**: RPM monitoring and control
- **Revenue Tracking**: Service operation logging

## Web Interface

The service bench provides a responsive web dashboard with:
- 🔋 Battery voltage monitoring and status
- ⚡ Motor RPM testing and control
- 📊 Real-time test status display
- 💰 Daily revenue tracking
- ⏱️ System uptime counter

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

1. **Power on** the ESP32-C6 device
2. **Connect** to the `ServiceBench` WiFi network (password: `tech123`)
3. **Open browser** to `http://192.168.4.1`
4. **Use the interface** to:
   - Monitor battery voltage
   - Start/stop motor tests
   - Reset batteries
   - Track service revenue

## API Endpoints

### GET /
Main web interface HTML page

### GET /api/status
Returns current system status as JSON:
```json
{
  "battery": {
    "voltage": 12.6,
    "connected": true
  },
  "motor": {
    "rpm": 3200,
    "connected": true
  },
  "system": {
    "test": "Motor Test",
    "revenue": 125.50,
    "uptime": 3600
  }
}
```

### POST /api/battery/reset
Initiates battery reset procedure (adds $25 to revenue)

### WebSocket /ws
Real-time updates with the same JSON structure as `/api/status`

WebSocket commands:
```json
{"command": "startMotor"}
{"command": "stopMotor"}
```

## Technical Implementation

### WiFi Access Point
```cpp
WiFi.softAP("ServiceBench", "tech123");
// IP: 192.168.4.1
```

### WebSocket Real-time Updates
- Updates broadcast every 500ms
- Simulated sensor readings (battery voltage, motor RPM)
- Client connection management with automatic cleanup

### Memory Usage
- **RAM**: ~33KB (10.2% of 320KB)
- **Flash**: ~666KB (19.9% of 3.3MB available program space)

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

**Problem**: Cannot connect to WiFi
- **Solution**: Device is in AP mode - connect TO the device's network, not your home WiFi

**Problem**: LittleFS mount failed
- **Solution**: First upload may require filesystem initialization - this is normal
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

## Future Enhancements

- [ ] Persistent storage for revenue data
- [ ] Multiple test profiles
- [ ] Data logging to SD card
- [ ] Battery health analytics
- [ ] Motor efficiency calculations
- [ ] OLED display integration
- [ ] Bluetooth remote control

## License

This project is for educational and commercial use.

## Author

Built for universal drone service operations on ESP32-C6 hardware.
