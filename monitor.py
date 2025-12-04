#!/usr/bin/env python3
"""
ESP32-C6 Service Bench Monitor
Real-time serial monitor with formatted output
"""

import serial
import serial.tools.list_ports
import time
import sys
from datetime import datetime

def find_esp32_port():
    """Find the ESP32 serial port automatically"""
    ports = serial.tools.list_ports.comports()
    for port in ports:
        if 'usbmodem' in port.device.lower() or 'usb' in port.device.lower():
            return port.device
    return None

def format_timestamp():
    """Return formatted timestamp"""
    return datetime.now().strftime("%H:%M:%S.%f")[:-3]

def main():
    # Find port
    port = find_esp32_port()
    if not port:
        print("❌ ESP32-C6 not found. Please connect the device.")
        sys.exit(1)
    
    print(f"🔌 Connected to: {port}")
    print(f"📡 Baud rate: 115200")
    print("=" * 60)
    
    try:
        ser = serial.Serial(port, 115200, timeout=1)
        time.sleep(2)  # Wait for connection
        
        print(f"[{format_timestamp()}] 🎧 Listening for messages...\n")
        
        while True:
            if ser.in_waiting > 0:
                try:
                    line = ser.readline().decode('utf-8', errors='ignore').strip()
                    if line:
                        timestamp = format_timestamp()
                        
                        # Format different message types
                        if "WiFi AP Started" in line:
                            print(f"[{timestamp}] ✅ {line}")
                        elif "IP:" in line or "IP Address:" in line:
                            print(f"[{timestamp}] 🌐 {line}")
                        elif "SSID:" in line:
                            print(f"[{timestamp}] 📶 {line}")
                        elif "Password:" in line:
                            print(f"[{timestamp}] 🔐 {line}")
                        elif "MAC Address:" in line:
                            print(f"[{timestamp}] 🏷️  {line}")
                        elif "WebSocket client" in line:
                            print(f"[{timestamp}] 🔌 {line}")
                        elif "server started" in line.lower():
                            print(f"[{timestamp}] 🚀 {line}")
                        elif "error" in line.lower() or "failed" in line.lower():
                            print(f"[{timestamp}] ❌ {line}")
                        elif "Starting" in line:
                            print(f"[{timestamp}] ⚡ {line}")
                        else:
                            print(f"[{timestamp}] {line}")
                            
                except UnicodeDecodeError:
                    pass
                    
    except KeyboardInterrupt:
        print("\n\n👋 Disconnected")
        ser.close()
    except Exception as e:
        print(f"\n❌ Error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
