# Hardware Notes

## Main Firmware Hardware

- ESP32-S3 VIEWE 7 inch display board.
- RGB touchscreen display using ESP32 Display Panel and LVGL.
- MFRC522 RFID reader.
- Analog ROM/motion sensor connected to GPIO 17.
- Wi-Fi network for Firebase RTDB sync.

## Firmware Pin Notes

Current firmware constants:

- Motion sensor input: GPIO 17.
- Sensor sample interval: 5 ms.
- RFID SS: GPIO 10.
- RFID SCK: GPIO 12.
- RFID MISO: GPIO 13.
- RFID MOSI: GPIO 11.
- RFID reset is not used as a dedicated pin.

## Weight Detection Prototype

The separate `weight_detection/` module uses a VL53L0X sensor:

- VCC -> 3.3 V.
- GND -> GND.
- SDA -> GPIO 21.
- SCL -> GPIO 22.

This prototype is not part of the production firmware path yet.
