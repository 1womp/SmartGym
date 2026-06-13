# Firebase Seed Instructions

Language: English | [Espanol](README.es.md)

This folder contains generated seed files for local/dev/test workflows.

## Files

- `firebase_seed/test_database_full_seed.json`
  - Full Realtime Database seed for development/testing only.
  - Includes roots like `usersByRfid`, `calibrations`, `devices`,
    `athleteWeeklySessions`, `machineConfigs`, and `machineCatalog`.
- `firebase_seed/machineCatalog_value_only.json`
  - Value-only JSON intended for the `/machineCatalog` node only.
  - Safe for manual node-level import when the selected path is exactly `/machineCatalog`.
- `tools/firebase_seed/machine_catalog_v1.json`
  - Source machine catalog document used by seed generators.

## Safety Rules

- Do not import `test_database_full_seed.json` into production.
- Do not import any seed file at database root (`/`) in production.
- Importing JSON at root can replace the entire database.
- Firmware must always work with embedded machine catalog defaults.
- Cloud machine catalog is optional and should only override matching fields.
- Never run full-root imports against production data.

## Firebase Console Safe Import (machineCatalog only)

1. Open Realtime Database in Firebase Console.
2. Select or create the `machineCatalog` child node.
3. Confirm the selected path is exactly `/machineCatalog`.
4. Import `firebase_seed/machineCatalog_value_only.json`.
5. Do not import this file at `/`.

Warning:
Importing JSON at database root can replace the entire database.

## Optional CLI Examples (manual only)

Machine catalog only (safe path-specific update):

```bash
firebase database:set /machineCatalog firebase_seed/machineCatalog_value_only.json
```

Full test database reset (dangerous, dev/test only):

```bash
firebase database:set / firebase_seed/test_database_full_seed.json
```

Do not run the root reset command against production.

## Regenerate Seeds

Run the existing test seed generator:

```bash
python scripts/generate_firebase_week_seed.py
```

This updates:

- `sample_data/firebase_rtdb_two_week_seed_2026_w16_w17.json`
- `sample_data/firebase_rtdb_week_seed_2026_w16.json`
- `firebase_seed/test_database_full_seed.json`
- `firebase_seed/machineCatalog_value_only.json`

## Sync Note

`machineCatalog` seed values should stay in sync with firmware `MachineRegistry` defaults.
