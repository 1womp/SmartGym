# Arquitectura Del Sistema SmartGym

Idioma: [English](SYSTEM_ARCHITECTURE.md) | Espanol

## Arquitectura General

```mermaid
flowchart LR
  Motion[Sensor de movimiento] --> SensorManager
  SensorManager --> ROM[Normalizacion ROM]
  ROM --> RepDetector
  RepDetector --> SessionRecorder
  RFID[Lector RFID] --> UserSync[Sync de usuario]
  UserSync --> Recommendation
  Recommendation --> UI[UI LVGL]
  SessionRecorder --> UploadQueue[Cola NVS]
  UploadQueue --> Firebase[Firebase RTDB]
  Firebase --> Dashboard[Dashboard React]
```

El sistema se divide en firmware embebido, Firebase RTDB y visualizacion en dashboard.

## Flujo De Datos

1. El sensor se muestrea cada 5 ms.
2. `SensorManager` filtra el valor analogico.
3. El firmware convierte la lectura a porcentaje ROM.
4. La calibracion de usuario mapea ROM crudo a ROM normalizado.
5. `RepDetector` identifica fases y repeticiones completas.
6. `SessionRecorder` guarda reps, sets, peso, ROM, tiempos y calidad.
7. `SmartGymTouchApp` calcula resumen y recomendacion.
8. La cola de subida guarda payloads Firebase.
9. `FirebaseService` envia escrituras seguras a RTDB.
10. El dashboard lee Firebase para graficas y resumenes.

## Flujo De Login

```mermaid
sequenceDiagram
  participant U as Usuario
  participant R as RFID
  participant D as Dispositivo
  participant F as Firebase
  U->>R: Escanea tarjeta
  R->>D: UID
  D->>D: Muestra popup de carga
  D->>F: Lee usersByRfid/{uid}
  F-->>D: Perfil
  D->>F: Lee calibrations/{uid}/{machineTypeId}
  F-->>D: Calibracion o missing
  D->>D: Resuelve recomendacion
  D->>D: Aplica mapeo ROM
  D->>D: Oculta popup
```

Las entradas quedan bloqueadas hasta que el sync termina o cae a un fallback seguro.

## Flujo De Entrenamiento

```mermaid
sequenceDiagram
  participant U as Usuario
  participant D as Dispositivo
  participant S as SessionRecorder
  participant Q as Cola
  participant F as Firebase
  U->>D: Presiona START
  D->>S: Inicia sesion con pin load
  U->>D: Hace reps
  D->>S: Registra reps y sets
  U->>D: Termina o auto-finish
  D->>S: Cierra sesion
  D->>Q: Encola subida
  Q->>F: Sube por fases
```

## Pin Load Vs Recomendacion

Pin load es estado fisico de la maquina. Permanece con la maquina entre usuarios. La recomendacion es consejo especifico por usuario.

El firmware nunca cambia pin load por una recomendacion. El usuario mueve fisicamente el pin y actualiza la UI con botones.

## Calibracion

La calibracion usa el pin load actual y cargas sugeridas. El usuario confirma la carga fisica antes de recolectar reps. El firmware analiza repeticiones suaves, estima si la carga es adecuada y guarda datos de calibracion por usuario y maquina.

## Sync

El final de sesion agenda sync. Trabajo pesado de Firebase no ocurre dentro del cierre inmediato.

Fases:

- Core de sesion.
- Metadata.
- Timeline.
- Resumenes.
- Detalles.
- Rep sets.

La cola NVS permite reintentos y evita perdida de datos si Firebase o Wi-Fi fallan temporalmente.

## Frecuencias

- Tick principal: 4 ms.
- Muestreo sensor: 5 ms, aproximadamente 200 Hz.
- Grafica: 16 ms, aproximadamente 60 Hz.
- UI lenta: 140 ms.
- UI debug: 160 ms.
- Servicio cloud normal: 5000 ms.
- Cadencia de finish sync: aproximadamente 1200 ms.
- Heartbeat: 60000 ms.
- Timeout user sync: 12000 ms.

## Estrategia De Memoria

El ESP32-S3 tiene poca memoria durante escrituras TLS/Firebase. El firmware mide heap interno y bloque interno mas grande, y elige modo:

- Normal: payloads mas grandes.
- Constrained: payloads pequenos y repSets fragmentados.
- Critical: payloads muy pequenos o backoff.

Objetos LVGL opcionales se liberan antes de entrenamiento/subida para mejorar heap disponible.
