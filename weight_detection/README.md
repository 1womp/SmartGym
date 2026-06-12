# Weight Detection Prototype

This folder contains a standalone teammate prototype for detecting the physical weight-stack position with a VL53L0X time-of-flight distance sensor.

It is intentionally kept separate from the main ESP32 SmartGym firmware. The production firmware currently uses a manual machine pin load model, where the user updates the displayed pin load with the touchscreen buttons.

## What It Does

- Reads distance from a VL53L0X sensor over I2C.
- Averages 30 samples per measurement cycle.
- Maps distance ranges to stack positions.
- Maps stack positions to detected weight values in pounds.
- Confirms the final detected weight after repeated consistent detections.
- Prints distance, detected stack, detected weight, final confirmed weight, and status to Serial.

## Hardware

- ESP32 or compatible Arduino board.
- VL53L0X distance sensor.
- Recommended wiring in the sketch:
  - VCC -> 3.3 V
  - GND -> GND
  - SDA -> GPIO 21
  - SCL -> GPIO 22

## Dependencies

Install the Adafruit VL53L0X library in Arduino IDE or PlatformIO:

- `Adafruit_VL53L0X`

## How to Run

1. Open `weight_detection_vl53l0x.ino` in Arduino IDE.
2. Install the VL53L0X library.
3. Select the ESP32 board and port.
4. Upload the sketch.
5. Open Serial Monitor at 115200 baud.

## Expected Output

The serial monitor prints:

- Average distance in cm.
- Detected weight in lb.
- Final confirmed weight.
- Confirmation counter.
- Detected stack number.
- Status messages.

## Relationship to SmartGym Firmware

The module can become a future replacement or helper for manual pin-load entry. Before integration, the distance-to-weight ranges must be calibrated for the exact machine geometry and weight stack.

## Limitations

- Distance ranges are hard-coded and machine-specific.
- Values are currently in pounds, while the main firmware UI uses kg.
- The module is not integrated with Firebase.
- The module does not change SmartGym recommendations.
- The module should not be merged into firmware until the physical sensor mounting and conversion model are stable.
