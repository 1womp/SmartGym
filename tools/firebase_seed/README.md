# Firebase Seed: Machine Catalog v1

Language: English | [Espanol](README.es.md)

This folder contains an optional Firebase Realtime Database seed for machine and goal timing configuration.

- Seed file: `tools/firebase_seed/machine_catalog_v1.json`
- Database key: `/machineCatalog`
- Schema version: `1`

## Important behavior

- Firmware uses embedded machine/goal config by default.
- Cloud machine catalog is an optional override.
- If cloud config is missing or fails to load, firmware must continue with embedded fallback.
- Do not store credentials, tokens, or database URLs in this repository.

## Manual upload (not automatic)

From the repository root, run one of these commands with Firebase CLI:

```bash
firebase database:update / tools/firebase_seed/machine_catalog_v1.json
```

or update only the catalog node:

```bash
firebase database:set /machineCatalog --data "$(cat tools/firebase_seed/machine_catalog_v1.json | jq -c '.machineCatalog')"
```

If you are on Windows PowerShell, extract the `machineCatalog` node first, then pass it to `firebase database:set /machineCatalog`.
