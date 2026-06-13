# Guia De Firebase SmartGym

Idioma: [English](FIREBASE_GUIDE.md) | Espanol

SmartGym usa Firebase Realtime Database para perfiles, calibraciones, historial de sesiones, resumenes, graficas de dashboard y heartbeat del dispositivo.

## Advertencia De Seguridad

No importes un JSON completo en la raiz de Firebase salvo que quieras reemplazar toda la base.

No commitees secretos, service accounts, tokens privados, contrasenas Wi-Fi ni `.env`. La configuracion web publica tambien debe restringirse con reglas Firebase y restricciones de dominio/app.

Patron de importacion seguro:

- Importar usuarios en `usersByRfid`.
- Importar calibraciones en `calibrations`.
- Importar configuracion de maquinas en `machineConfigs`.
- Importar sesiones en `athleteWeeklySessions/{uid}/{weekKey}`.

## Nodos Principales RTDB

### usersByRfid

```text
usersByRfid/{uid}
```

Guarda perfil asociado a RFID: nombre, edad, altura, peso, genero y meta preferida.

### calibrations

```text
calibrations/{uid}/{machineTypeId}
```

Guarda:

- bandera de calibracion
- carga sugerida/recomendada
- bottom/top/rango ROM
- confianza
- accion
- razon
- recomendaciones por meta
- snapshot de motion target
- sets de calibracion
- next recommended weight

### machineConfigs

```text
machineConfigs/{machineId}
```

Configuracion cloud de maquina cuando existe. El firmware tambien incluye un catalogo embebido para poder arrancar offline.

### athleteWeeklySessions

```text
athleteWeeklySessions/{uid}/{weekKey}
```

Nodo principal de historial.

Estructura tipica:

```text
athleteWeeklySessions/{uid}/{weekKey}/days/{dayKey}/sessions/{sessionId}
athleteWeeklySessions/{uid}/{weekKey}/days/{dayKey}/daySummary
athleteWeeklySessions/{uid}/{weekKey}/days/{dayKey}/timeline/{timelineKey}
athleteWeeklySessions/{uid}/{weekKey}/weekSummary
```

### devices

```text
devices/{deviceId}
```

Guarda heartbeat e identidad/estado del dispositivo.

## Raiz De Sesion

Incluye:

- ID de sesion.
- UID de usuario.
- Maquina y tipo.
- Meta.
- Inicio/fin.
- Duracion.
- Pin load.
- Carga recomendada.
- Totales de reps/sets.
- Reps validas/invalidas.
- Metricas ROM y velocidad.
- Calidad.
- Links a detalles pesados.

## daySummary

Resumen diario para lecturas rapidas del dashboard.

## weekSummary

Resumen semanal del usuario/semana activa.

## timeline

Entradas ligeras ordenadas para tarjetas del dashboard. Tambien funcionan como marcador de idempotencia para evitar duplicados en reintentos.

## representativeReps

Guarda reps seleccionadas:

- Mejor ROM.
- Mejor velocidad.
- Primera valida.
- Ultima valida.

## setDetails

Guarda resumen por set:

- numero de set
- reps objetivo
- reps validas
- reps invalidas
- carga inicio/fin
- si cambio peso
- resumen ROM/velocidad

## repSets

Guarda datos profundos por rep:

```text
repSets/set1/reps/0
repSets/set1/reps/1
```

El firmware puede subirlos en PATCH pequenos, pero la forma final en Firebase no cambia.

## Fases De Subida

1. Root/core de sesion.
2. Metadata diaria.
3. Timeline.
4. Day summary.
5. Week summary.
6. Representative reps.
7. Set details.
8. Rep sets.

## Subida Segura En Memoria

El firmware usa cola NVS y fragmentacion segun memoria. Con heap restringido, repSets se dividen en patches pequenos para evitar bloqueos Firebase/TLS.

## Compatibilidad Dashboard

El dashboard lee:

- `usersByRfid/{rfid}`
- `athleteWeeklySessions/{rfid}`
- `athleteWeeklySessions/{rfid}/{week}/days`
- `recommendedRoutineProgress/{rfid}/{selectedDate}`

No renombres rutas del firmware sin actualizar el dashboard.
