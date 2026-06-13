# Start Here

Language: English | [Espanol](START_HERE.es.md)

This page is the fastest way to find the right SmartGym information without
digging through folders.

## What SmartGym Is

SmartGym is an ESP32-S3 prototype that instruments a selectorized gym machine.
It reads machine motion, normalizes range of motion, detects reps and sets,
records user sessions, stores calibration data, and syncs finished workouts to
Firebase for dashboard analysis.

## First Read These

| Need | Open |
| --- | --- |
| Understand the project in 5 minutes | [Project README](../README.md) |
| Use the device during a demo | [User Manual](USER_MANUAL.md) |
| Understand ROM, calibration, equations, and errors | [Measurement and Calibration Guide](MEASUREMENT_AND_CALIBRATION_GUIDE.md) |
| Understand firmware modules and data flow | [System Architecture](SYSTEM_ARCHITECTURE.md) |
| Build or modify firmware | [Developer Guide](DEVELOPER_GUIDE.md) |
| Understand Firebase paths and upload behavior | [Firebase Guide](FIREBASE_GUIDE.md) |
| Understand dashboard seed/test data | [Firebase Dashboard Seed Guide](FIREBASE_DASHBOARD_SEED_GUIDE.md) |
| Fix common hardware, sync, or UI problems | [Troubleshooting Guide](TROUBLESHOOTING.md) |
| Find CAD and printable hardware files | [Hardware CAD README](../hardware/cad/README.md) |
| Run the React dashboard | [Dashboard README](../dashboard/README.md) |
| Test the separate VL53L0X prototype | [Weight Detection README](../weight_detection/README.md) |

## If You Are Building the Full Project

Follow this order:

1. Read the [Project README](../README.md).
2. Build the firmware using the commands in the README.
3. Read the [Measurement and Calibration Guide](MEASUREMENT_AND_CALIBRATION_GUIDE.md).
4. Wire and mount hardware using [Hardware Notes](../hardware/README.md) and [CAD files](../hardware/cad/README.md).
5. Configure Firebase using the [Firebase Guide](FIREBASE_GUIDE.md).
6. Use the [Firebase Dashboard Seed Guide](FIREBASE_DASHBOARD_SEED_GUIDE.md) if you need demo data.
7. Run the dashboard using [Dashboard README](../dashboard/README.md).
8. Use the [Troubleshooting Guide](TROUBLESHOOTING.md) during hardware tests.

## How the Main Data Flow Works

```text
Motion sensor -> ESP32-S3 -> ROM normalization -> rep detection
RFID -> user profile -> calibration -> recommendation
Session recorder -> NVS queue -> Firebase RTDB -> React dashboard
```

## Core Concepts

| Concept | Short Explanation | Full Details |
| --- | --- | --- |
| ROM | The user's calibrated movement range, normalized from bottom to top. | [Measurement and Calibration Guide](MEASUREMENT_AND_CALIBRATION_GUIDE.md) |
| Pin load | The physical machine weight-pin position. It belongs to the machine, not the user. | [Measurement and Calibration Guide](MEASUREMENT_AND_CALIBRATION_GUIDE.md) |
| Recommended load | User-specific advice from calibration or session history. It does not move the pin. | [Measurement and Calibration Guide](MEASUREMENT_AND_CALIBRATION_GUIDE.md) |
| Rep detection | A ROM state machine that detects bottom, ascent, top, descent, and completion. | [Developer Guide](DEVELOPER_GUIDE.md) |
| Firebase queue | Local NVS-backed retry queue for safe uploads. | [Firebase Guide](FIREBASE_GUIDE.md) |
| Dashboard | React app that reads Firebase workout history. | [Dashboard README](../dashboard/README.md) |

## Where Files Live

| Area | Folder |
| --- | --- |
| ESP32-S3 firmware | `src/`, `lib/`, `boards/`, `platformio.ini` |
| Documentation | `docs/` |
| Dashboard | `dashboard/` |
| CAD and STL files | `hardware/cad/` |
| Hardware notes | `hardware/` |
| VL53L0X prototype sketch | `weight_detection/` |
| Utility scripts | `scripts/`, `tools/` |
| Firebase/sample data | `sample_data/` |

## What To Check Before a Demo

1. Firmware builds and uploads.
2. Wi-Fi connects.
3. RFID scans a known user.
4. User sync finishes before START is enabled.
5. ROM calibration is loaded or a safe fallback is shown.
6. Pin load and recommendation are displayed separately.
7. Session starts, reps count, and summary appears.
8. Firebase upload reaches complete.
9. Dashboard shows the session.

## Most Common Confusions

| Confusion | Correct Mental Model |
| --- | --- |
| Recommendation changed, so pin load changed. | No. Recommendation is advice; pin load is the physical machine state. |
| A movement always counts as a rep. | No. A rep must satisfy ROM, timing, and phase-order checks. |
| Firebase upload happens immediately in one request. | No. It is queued locally and uploaded in phases. |
| The dashboard owns the data model. | No. Firmware writes the data; dashboard reads it. |
| The VL53L0X prototype automatically controls the main firmware. | No. It is a separate prototype unless integrated intentionally. |

## Security Reminder

Do not commit real Wi-Fi passwords, Firebase secrets, service accounts, tokens,
or private `.env` files. Use example files and local environment variables.
