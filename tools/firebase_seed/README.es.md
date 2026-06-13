# Firebase Seed: Catalogo De Maquinas v1

Idioma: [English](README.md) | Espanol

Esta carpeta contiene un seed opcional para Firebase Realtime Database con configuracion de maquinas y tiempos por objetivo.

- Archivo seed: `tools/firebase_seed/machine_catalog_v1.json`
- Llave en base de datos: `/machineCatalog`
- Version de esquema: `1`

## Comportamiento Importante

- El firmware usa configuracion embebida de maquinas/objetivos por default.
- El catalogo en la nube es una sobrescritura opcional.
- Si la configuracion cloud no existe o falla al cargar, el firmware debe seguir con fallback embebido.
- No guardes credenciales, tokens ni URLs privadas de base de datos en este repositorio.

## Subida Manual

Desde la raiz del repositorio, con Firebase CLI:

```bash
firebase database:update / tools/firebase_seed/machine_catalog_v1.json
```

O actualiza solo el nodo del catalogo:

```bash
firebase database:set /machineCatalog --data "$(cat tools/firebase_seed/machine_catalog_v1.json | jq -c '.machineCatalog')"
```

En Windows PowerShell, extrae primero el nodo `machineCatalog` y despues pasalo a `firebase database:set /machineCatalog`.
