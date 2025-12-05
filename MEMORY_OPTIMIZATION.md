# ESP32-C6 Memory Optimization Guide

## Current Usage
- Flash: 84.2% (883KB / 1MB)  
- RAM: 10.3% (33KB / 327KB) ← RAM is fine!

## Optimization Strategies (in order of impact)

### 1. ✅ GZIP Compression (IMPLEMENTED - saves ~3.8KB)
**Savings: 3,809 bytes (66% compression)**
- HTML compressed from 5,756 → 1,947 bytes
- Browser automatically decompresses
- To apply: See instructions below

### 2. 📦 Use OTA Partitions (saves ~50KB)
**Savings: ~50,000 bytes**
```ini
# In platformio.ini, you already have OTA partitions
# But you can optimize further:
board_build.partitions = partitions_minimal_ota.csv
```

Create `partitions_minimal_ota.csv`:
```csv
# Name,   Type, SubType, Offset,  Size
nvs,      data, nvs,     0x9000,  0x4000
phy_init, data, phy,     0xd000,  0x1000
factory,  app,  factory, 0x10000, 0x180000  # 1.5MB for factory app
ota_0,    app,  ota_0,   0x190000,0x180000  # 1.5MB for OTA
ota_1,    app,  ota_1,   0x310000,0x180000  # 1.5MB for OTA backup
otadata,  data, ota,     0x8000,  0x2000
```

This gives you 1.5MB instead of 1MB per partition!

### 3. 🎯 Compiler Optimization Flags
**Savings: ~20-50KB**
Add to `platformio.ini`:
```ini
build_flags = 
    -Os                    # Optimize for size
    -ffunction-sections    # Place functions in separate sections
    -fdata-sections        # Place data in separate sections
    -Wl,--gc-sections      # Remove unused sections
    -DCORE_DEBUG_LEVEL=0   # Disable debug logging
```

### 4. 🔧 Remove Unused WiFi Features
**Savings: ~10-30KB**
```cpp
// Disable unused WiFi features
#define CONFIG_ESP32_WIFI_AMPDU_TX_ENABLED 0
#define CONFIG_ESP32_WIFI_AMPDU_RX_ENABLED 0
```

### 5. 📚 Modular Design (Future)
For when you need >1MB total:
- Split into "service mode" and "drive mode" firmwares
- OTA switch between modes
- Each mode ~500KB instead of >1MB combined

## How to Apply GZIP Compression

### Option A: Manual (Safest)
1. Open `src/main.cpp`
2. Find line starting with `// HTML content for the web interface`
3. Delete everything from that line to the line with just `";`
4. Replace with contents of `html_compressed.h` (already generated)
5. Find `root_handler` function and change:
```cpp
// OLD:
httpd_resp_set_type(req, "text/html");
httpd_resp_send(req, html_page, strlen(html_page));

// NEW:
httpd_resp_set_type(req, "text/html");
httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
httpd_resp_send(req, (const char *)html_page_gz, html_page_gz_len);
```

### Option B: Script (Faster)
```bash
chmod +x apply_compression.sh
./apply_compression.sh
```

## Expected Results After Compression
- Flash: ~80% (instead of 84.2%)
- Frees up ~40KB for future features
- HTML loads slightly faster (less data transfer)

## Future Growth Path
When you hit 90%+ flash:
1. Apply all optimizations above → gain ~100KB
2. If still not enough → split into mode-specific firmwares
3. Service Mode: WiFi config + diagnostics (~400KB)
4. Drive Mode: Motor control + sensors (~600KB)  
5. OTA switch between modes as needed

## Monitoring Commands
```bash
# Check current size
platformio run | grep -E "(Flash:|RAM:)"

# Detailed section sizes
~/.platformio/packages/toolchain-riscv32-esp/bin/riscv32-esp-elf-size -A .pio/build/esp32c6/firmware.elf | head -20
```
