# Guia De Desarrollador SmartGym

Idioma: [English](DEVELOPER_GUIDE.md) | Espanol

## Entorno De Desarrollo

El firmware es un proyecto PlatformIO Arduino para una pantalla ESP32-S3 VIEWE.

Environment requerido:

```text
BOARD_VIEWE_UEDX80480070E_WB_A
```

Build:

```powershell
python -m platformio run --environment BOARD_VIEWE_UEDX80480070E_WB_A
```

Para builds locales, proporciona Wi-Fi/Firebase privados mediante build flags o configuracion local no trackeada. No commitees SSID, passwords, tokens, service accounts ni configuracion privada.

Upload y monitor:

```powershell
python -m platformio run --target upload --target monitor --environment BOARD_VIEWE_UEDX80480070E_WB_A
```

## Layout Del Repositorio

- `src/`: codigo de aplicacion.
- `lib/`: modulos firmware.
- `boards/`: configuracion de board.
- `dashboard/`: dashboard React.
- `weight_detection/`: prototipo VL53L0X.
- `docs/`: documentacion.
- `hardware/`: notas de cableado/CAD.
- `screenshots/`: imagenes de demo.

## Arquitectura Principal

`SmartGymTouchApp` coordina estado de UI, entrenamiento, calibracion, user sync, cola Firebase y guards de memoria.

Estados principales:

- Idle/Ready.
- Calibration.
- Training.
- Summary.

Pantallas:

- Main.
- Summary.
- Idle.
- Calibration.
- Calibration gate.
- Debug.
- Profile.

## Modulos

### SmartGymTouchApp

Coordina:

- creacion/refresco LVGL
- RFID
- sync de perfil/calibracion
- normalizacion ROM
- reps
- calibracion
- inicio/fin de sesion
- agenda de Firebase
- chunking por memoria

### SensorManager

Lee el sensor analogico y lo convierte a ROM filtrado. Muestreo: 5 ms, aproximadamente 200 Hz.

### RepDetector

Maquina de estados:

- Bottom
- Ascending
- Top
- Descending

Calcula duracion, rango ROM, velocidades, warnings e invalid flags.

### SessionRecorder

Guarda sesion activa, sets, reps, carga por rep, tiempos, ROM, calidad y payloads JSON.

Limites actuales:

- Max sets por sesion: 8.
- Max reps por sesion: 80.

### FirebaseService

Maneja rutas RTDB, JSON, lecturas, escrituras y transporte FirebaseClient.

### MachineRegistry

Guarda catalogo embebido y templates de movimiento para:

- `hypertrophy`
- `strength`
- `endurance`
- `test`

`test` se conserva para validacion device/Firebase.

### UserRegistry

Guarda perfiles locales y calibraciones usuario-maquina.

Limites:

- perfiles locales: 8
- calibraciones por perfil: 8

### LocalPersistenceStore

Usa NVS/Preferences para:

- cache de usuarios
- cache de sesiones
- cola de upload pendiente

### RfidService

Lee MFRC522 y formatea UIDs en hex mayusculo separado por `-`.

## UI

El firmware usa LVGL. Refrescos:

- tick principal: 4 ms
- chart: 16 ms
- UI lenta: 140 ms
- debug UI: 160 ms

Todo objeto LVGL de pantalla/modal debe resetearse a `nullptr` al destruir su padre para evitar crashes por punteros obsoletos.

## Calibracion

La calibracion es especifica por usuario y machine type. Flujo:

- confirmacion de carga inicial
- controles fisicos de pin load
- recoleccion de reps
- analisis de set
- siguiente carga opcional
- resultado y guardado

Se guarda en:

```text
calibrations/{uid}/{machineTypeId}
```

## Recomendacion

Recomendacion esta separada de pin load:

- pin load: estado fisico de maquina
- recomendacion: consejo especifico por usuario

La recomendacion no debe sobrescribir el pin load.

## Firebase Sync

El final de sesion agenda upload en cola.

Fases:

1. Core de sesion.
2. Day metadata.
3. Day timeline.
4. Day summary.
5. Week summary.
6. Representative reps.
7. Set details.
8. Rep sets.

## Chunking

Modos de memoria:

- Normal.
- Constrained.
- Critical.

Limites de repSet:

- Normal: target 1900 bytes, max 2100, hasta 6 reps.
- Constrained: target 900 bytes, max 1100, hasta 3 reps.
- Critical: target 650 bytes, max 850, hasta 2 reps.

La forma final en Firebase se mantiene:

```text
repSets/set1/reps/0
repSets/set1/reps/1
```

## Agregar Una Maquina

1. Agregar seed en `MachineRegistry`.
2. Definir id, type id, nombre, categoria, musculos, stroke, min/max/incremento y peso default.
3. Confirmar templates de metas y timing.
4. Recompilar firmware.
5. Verificar compatibilidad Firebase si hay overrides cloud.

## Goals

Publicos:

- `hypertrophy`
- `strength`
- `endurance`

Interno/debug:

- `test`

Legacy:

- `general`, normalizado para runtime nuevo.

No elimines `test`; sirve para pruebas device/Firebase.

## Tags De Logs

- `[BOOT]`: arranque.
- `[BOOTMEM]`: memoria.
- `[RFID]`: tarjetas.
- `[USER_SYNC]`: perfil/calibracion/recomendacion.
- `[UI_LOADING]`: popup.
- `[WEIGHT]`: pin load/recomendacion.
- `[SESSION]`: sesion, reps, sets, finish.
- `[CAL]`: logica calibracion.
- `[CAL_UI]`: UI calibracion.
- `[SYNC]`: upload/cola.
- `[SYNCGATE]`: scheduling cloud.
- `[CloudWrite]`: Firebase writes.
- `[GRAPH]`: grafica/timing.

## Memoria

Firebase/TLS puede consumir mucha memoria. Libera UI opcional antes de entrenar/subir, evita payloads grandes en memoria critica y no cambies la forma final de Firebase sin actualizar dashboard.
