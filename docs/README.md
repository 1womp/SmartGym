# SmartGym Documentation

Language: English | [Espanol](README.es.md)

This folder contains the documentation package for the SmartGym v1.0 prototype. Each major guide is available as editable Markdown and as a formatted PDF.

## Start Here

- [Start Here](START_HERE.md)
- [Project README](../README.md)
- [User Manual](USER_MANUAL.md) | [PDF](USER_MANUAL.pdf)
- [System Architecture](SYSTEM_ARCHITECTURE.md) | [PDF](SYSTEM_ARCHITECTURE.pdf)
- [Developer Guide](DEVELOPER_GUIDE.md) | [PDF](DEVELOPER_GUIDE.pdf)
- [Measurement and Calibration Guide](MEASUREMENT_AND_CALIBRATION_GUIDE.md)
- [Firebase Guide](FIREBASE_GUIDE.md) | [PDF](FIREBASE_GUIDE.pdf)
- [Troubleshooting Guide](TROUBLESHOOTING.md) | [PDF](TROUBLESHOOTING.pdf)

## Reference Documents

| Document | Markdown | PDF |
| --- | --- | --- |
| Start Here | [Open](START_HERE.md) | PDF can be generated locally |
| Measurement and Calibration Guide | [Open](MEASUREMENT_AND_CALIBRATION_GUIDE.md) | PDF can be generated locally |
| Firebase Dashboard Seed Guide | [Open](FIREBASE_DASHBOARD_SEED_GUIDE.md) | [PDF](FIREBASE_DASHBOARD_SEED_GUIDE.pdf) |
| Firebase Dashboard Seed Guide ES | [Abrir](FIREBASE_DASHBOARD_SEED_GUIDE.es.md) | PDF can be generated locally |

## Find Information By Task

| Task | Best Document |
| --- | --- |
| Understand the whole project quickly | [Start Here](START_HERE.md) |
| Operate the physical device | [User Manual](USER_MANUAL.md) |
| Explain ROM, calibration, formulas, noise, and errors | [Measurement and Calibration Guide](MEASUREMENT_AND_CALIBRATION_GUIDE.md) |
| Understand firmware modules and runtime flow | [System Architecture](SYSTEM_ARCHITECTURE.md) |
| Build, upload, debug, or modify firmware | [Developer Guide](DEVELOPER_GUIDE.md) |
| Understand Firebase paths and upload phases | [Firebase Guide](FIREBASE_GUIDE.md) |
| Understand Firebase/dashboard seed data | [Firebase Dashboard Seed Guide](FIREBASE_DASHBOARD_SEED_GUIDE.md) |
| Diagnose failed builds, RFID, ROM, sync, or dashboard issues | [Troubleshooting Guide](TROUBLESHOOTING.md) |
| Find 3D-printable files and CAD sources | [Hardware CAD README](../hardware/cad/README.md) |
| Run the dashboard | [Dashboard README](../dashboard/README.md) |
| Use the standalone ToF prototype | [Weight Detection README](../weight_detection/README.md) |

## Recommended Reading Order

1. Read [Start Here](START_HERE.md) for the project map.
2. Read the [User Manual](USER_MANUAL.md) to understand how the device is used.
3. Read the [Measurement and Calibration Guide](MEASUREMENT_AND_CALIBRATION_GUIDE.md) to understand ROM, rep detection, calibration, and error sources.
4. Read the [System Architecture](SYSTEM_ARCHITECTURE.md) for the full data flow.
5. Read the [Developer Guide](DEVELOPER_GUIDE.md) before changing firmware.
6. Read the [Firebase Guide](FIREBASE_GUIDE.md) before importing, editing, or migrating database data.
7. Read the [Firebase Dashboard Seed Guide](FIREBASE_DASHBOARD_SEED_GUIDE.md) before loading demo dashboard data.
8. Use the [Troubleshooting Guide](TROUBLESHOOTING.md) during demos and hardware testing.

## Regenerating PDFs

Run this from the repository root:

```powershell
python tools/generate_docs_pdfs.py
```
