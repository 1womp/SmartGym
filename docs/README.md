# SmartGym Documentation

This folder contains the documentation package for the SmartGym v1.0 prototype. Each major guide is available as editable Markdown and as a formatted PDF.

## Start Here

- [Project README](../README.md)
- [User Manual](USER_MANUAL.md) | [PDF](USER_MANUAL.pdf)
- [System Architecture](SYSTEM_ARCHITECTURE.md) | [PDF](SYSTEM_ARCHITECTURE.pdf)
- [Developer Guide](DEVELOPER_GUIDE.md) | [PDF](DEVELOPER_GUIDE.pdf)
- [Firebase Guide](FIREBASE_GUIDE.md) | [PDF](FIREBASE_GUIDE.pdf)
- [Troubleshooting Guide](TROUBLESHOOTING.md) | [PDF](TROUBLESHOOTING.pdf)

## Reference Documents

| Document | Markdown | PDF |
| --- | --- | --- |
| Firebase Dashboard Seed Guide | [Open](FIREBASE_DASHBOARD_SEED_GUIDE.md) | [PDF](FIREBASE_DASHBOARD_SEED_GUIDE.pdf) |

## Recommended Reading Order

1. Read the [User Manual](USER_MANUAL.md) to understand how the device is used.
2. Read the [System Architecture](SYSTEM_ARCHITECTURE.md) for the full data flow.
3. Read the [Developer Guide](DEVELOPER_GUIDE.md) before changing firmware.
4. Read the [Firebase Guide](FIREBASE_GUIDE.md) before importing, editing, or migrating database data.
5. Use the [Troubleshooting Guide](TROUBLESHOOTING.md) during demos and hardware testing.

## Regenerating PDFs

Run this from the repository root:

```powershell
python tools/generate_docs_pdfs.py
```
