# Instrucciones De Seed Firebase

Idioma: [English](README.md) | Espanol

Esta carpeta contiene archivos seed generados para flujos locales, de desarrollo y de prueba.

## Archivos

- `firebase_seed/test_database_full_seed.json`
  - Seed completo de Realtime Database solo para desarrollo/pruebas.
  - Incluye nodos como `usersByRfid`, `calibrations`, `devices`, `athleteWeeklySessions`, `machineConfigs` y `machineCatalog`.
- `firebase_seed/machineCatalog_value_only.json`
  - JSON solo de valor para el nodo `/machineCatalog`.
  - Es seguro para importacion manual a nivel de nodo si la ruta seleccionada es exactamente `/machineCatalog`.
- `tools/firebase_seed/machine_catalog_v1.json`
  - Documento fuente de catalogo de maquinas usado por los generadores.

## Reglas De Seguridad

- No importes `test_database_full_seed.json` en produccion.
- No importes ningun seed en la raiz (`/`) de una base de produccion.
- Importar JSON en la raiz puede reemplazar toda la base de datos.
- El firmware debe funcionar siempre con defaults embebidos del catalogo de maquinas.
- El catalogo en la nube es opcional y solo debe sobrescribir campos compatibles.
- Nunca ejecutes imports completos de raiz contra datos de produccion.

## Importacion Segura En Firebase Console

Para importar solo `machineCatalog`:

1. Abre Realtime Database en Firebase Console.
2. Selecciona o crea el nodo hijo `machineCatalog`.
3. Confirma que la ruta seleccionada sea exactamente `/machineCatalog`.
4. Importa `firebase_seed/machineCatalog_value_only.json`.
5. No importes este archivo en `/`.

Advertencia:
Importar JSON en la raiz de la base puede reemplazar toda la base.

## Ejemplos Opcionales Con CLI

Catalogo de maquinas solamente:

```bash
firebase database:set /machineCatalog firebase_seed/machineCatalog_value_only.json
```

Reset completo de base de prueba:

```bash
firebase database:set / firebase_seed/test_database_full_seed.json
```

No ejecutes el reset de raiz contra produccion.

## Regenerar Seeds

Ejecuta el generador existente:

```bash
python scripts/generate_firebase_week_seed.py
```

Esto actualiza:

- `sample_data/firebase_rtdb_two_week_seed_2026_w16_w17.json`
- `sample_data/firebase_rtdb_week_seed_2026_w16.json`
- `firebase_seed/test_database_full_seed.json`
- `firebase_seed/machineCatalog_value_only.json`

## Nota De Sincronizacion

Los valores de `machineCatalog` deben mantenerse alineados con los defaults de firmware en `MachineRegistry`.
