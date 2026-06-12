# SmartGym Display Bring-up

This project is intentionally separate from `SmartGym`.

## Purpose

Use this repo-sized sandbox to validate the new VIEWE 7-inch ESP32-S3 board:

- upload flow
- serial over USB
- flash / PSRAM assumptions
- panel initialization
- GT911 touch
- LVGL rendering

## Why Separate

The new display board changes too many things at once:

- ESP32 -> ESP32-S3
- GPIO map
- display stack
- touch stack
- USB/serial behavior
- memory profile

Keeping bring-up separate makes debugging much easier and avoids contaminating
the working SmartGym MVP while hardware setup is still unstable.

## Migration Strategy

1. Get this project stable with basic panel/touch/LVGL tests.
2. Keep SmartGym unchanged while testing.
3. Once stable, migrate the display knowledge back into SmartGym as a proper HMI layer.
4. Reuse existing SmartGym domain modules later:
   - `AppController`
   - `RepDetector`
   - `CalibrationService`
   - `SessionRecorder`
   - `FirebaseService`
   - `UserRegistry`
   - `MachineRegistry`

## Next Implementation Step

After confirming this project flashes and reports PSRAM correctly, the next
task should be a minimal panel-on + touch test screen.

## Cloud Reconciliation Policy (Current Firmware)

When an RFID card is scanned and cloud is enabled, the device runs a
reconciliation pass before training:

1. Pull latest user profile (`usersByRfid/{uid}`).
2. Pull latest calibration (`calibrations/{uid}/{machineTypeId}`).
3. Pull latest machine config (`machineConfigs/{machineId}`) during poll cycle.
4. Merge conflict rule:
   - If cloud timestamp is newer, local profile/calibration is updated.
   - If local timestamp is newer, local data is pushed back to cloud.
5. Session uploads are idempotent:
   - deterministic paths by `sessionId` and timeline key
   - payloads include `schemaVersion` and `idempotencyKey`.
