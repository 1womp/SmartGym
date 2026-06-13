# Guia De Seed Firebase Para Dashboard

Idioma: [English](FIREBASE_DASHBOARD_SEED_GUIDE.md) | Espanol

Este documento explica el archivo grande de datos de ejemplo para probar el dashboard:

- `sample_data/firebase_rtdb_dashboard_seed_2026_w12_w17.json`

El archivo fue generado por:

- `tools/generate_dashboard_seed.py`

## Proposito

Este seed no es solo una demo pequena. Sirve para validar un dashboard SmartGym completo con datos sinteticos pero realistas.

Incluye suficiente informacion para probar:

- lista de usuarios
- estado actual del dispositivo
- vistas semanales y diarias
- progreso por maquina
- detalle de sets y repeticiones
- tendencias de metricas corporales
- retroalimentacion post-sesion como `RPE` y notas

## Tamano Del Dataset

Dataset generado actualmente:

- `5` usuarios
- `5` maquinas
- `6` semanas
- `147` dias de entrenamiento
- `673` sesiones de maquina

Cada usuario entrena aproximadamente `4-6` dias por semana. Cada dia de entrenamiento contiene `4-5` sesiones de maquina.

## Nodos Principales

El seed contiene estos nodos principales:

- `usersByRfid`
- `calibrations`
- `calibrationHistory`
- `machineConfigs`
- `devices`
- `bodyMetricsHistory`
- `sessionFeedback`
- `athleteWeeklySessions`

## Que Significa Cada Nodo

### `usersByRfid`

Tabla de perfiles de atletas. Cada usuario incluye:

- UID RFID
- nombre visible
- datos corporales
- meta principal

Se usa para el selector de atleta, encabezado de perfil y filtros por objetivo.

### `calibrations`

Calibracion efectiva por usuario y tipo de maquina. Cada registro puede contener:

- `suggestedWeightKg`
- `userRomPercent`
- `hasCalibration`

Se usa para revisar si el usuario ya calibro, mostrar carga recomendada y guiar el flujo de preparacion de maquina.

### `calibrationHistory`

Historial de eventos de calibracion. Sirve para:

- graficas de tendencia
- revisar cambios de carga sugerida
- auditar recalibraciones

### `machineConfigs`

Metadata de las maquinas y fuente de recomendaciones base. Cada maquina incluye:

- IDs de maquina
- valores ROM default
- peso default de calibracion
- recomendaciones por meta
- campos de configuracion del encoder
- taxonomia:
  - `exerciseCategory`
  - `primaryMuscleGroup`
  - `secondaryMuscleGroup`

Se usa para tarjetas de maquina, agrupacion por ejercicio, dashboards musculares y pantallas administrativas.

### `devices`

Estado vivo de los dispositivos ESP32. Cada dispositivo puede incluir:

- identidad del dispositivo
- maquina asignada
- usuario activo
- estado de la app
- estado Wi-Fi
- estado de calibracion del encoder

Se usa para monitoreo en piso, diagnostico y para saber que maquina esta activa.

### `bodyMetricsHistory`

Historial de check-in corporal manejado por la web app. Cada fecha puede contener:

- peso corporal
- porcentaje de grasa
- porcentaje de musculo esqueletico
- cintura
- sueno
- energia
- notas

### `sessionFeedback`

Retroalimentacion subjetiva post-entrenamiento. Puede contener:

- `rpe`
- `energyScore`
- `sorenessScore`
- `painScore`
- `readinessScore`
- `notes`

Se usa para pantallas de revision de sesion, notas de entrenador y comparacion entre datos objetivos y subjetivos.

### `athleteWeeklySessions`

Arbol principal del historial de entrenamiento.

Forma de ruta:

```text
athleteWeeklySessions/{uid}/{weekKey}/days/{dayKey}
```

Dentro de cada dia existen:

- `meta`
- `daySummary`
- `timeline`
- `sessions`

## Estructura De Una Sesion

Cada sesion de maquina vive en:

```text
athleteWeeklySessions/{uid}/{weekKey}/days/{dayKey}/sessions/{sessionId}
```

Una sesion incluye:

- `identity`
- `machine`
- `timing`
- `plan`
- `summary`
- `analysis`
- `setOverview`
- `setDetails`
- `repSets`
- `representativeReps`

### `summary`

Bloque rapido de resultado de sesion. Ejemplos:

- `setsCompleted`
- `validReps`
- `invalidReps`
- `volumeLoadKg`
- `avgRomPercent`
- `avgPeakVelocityPctPerSec`

### `analysis`

Bloque mas rico para dashboard y coaching. Ejemplos:

- `romComplianceRate`
- `idealRomHitRate`
- `avgRestSecondsPerSet`
- `fatigueRomDrop`
- `fatigueVelocityDrop`
- contadores de razones invalidas

### `setOverview`

Resumen por set. Se puede usar para tarjetas rapidas, comparacion de fatiga y revision de entrenador.

### `setDetails`

Objeto mas explicito set por set. Se usa para tablas o vistas de analisis mas profundas.

### `repSets`

Detalle de repeticiones agrupado por set. Cada repeticion puede incluir:

- validez
- ROM
- duracion
- tiempo concentrico
- velocidad concentrica pico
- velocidad excentrica pico
- banderas de advertencia

Esta forma debe mantenerse compatible con lo que escribe el firmware.

## Como Se Construyo El Seed

El generador crea uso sintetico de gimnasio con reglas simples:

- los usuarios entrenan `4-6` veces por semana
- cada dia usa `4-5` maquinas
- las sesiones varian por meta, maquina y calibracion
- algunas repeticiones son validas y otras invalidas
- algunas sesiones tienen fatiga o advertencias
- metricas corporales y feedback estan separados de los datos del dispositivo

El seed es bueno para probar UI, filtros y graficas. No debe interpretarse como un modelo fisiologico exacto.

## Mejor Forma De Explorarlo

Para entender el archivo rapido, revisalo en este orden:

1. `usersByRfid`
2. `machineConfigs`
3. un usuario dentro de `athleteWeeklySessions`
4. una semana
5. un dia
6. una sesion
7. `summary`, `analysis`, `setDetails` y `repSets` de esa sesion

Ese recorrido da el modelo mental completo sin leer todo el JSON.

## Pruebas Recomendadas Para Dashboard

Este seed sirve especialmente para validar:

1. graficas de actividad semanal
2. resumenes de sesiones y volumen
3. filtros por maquina y grupo muscular
4. tendencias de metricas corporales
5. paginas de detalle con analisis de repeticiones
6. paneles de revision con `RPE` y notas
7. monitoreo de dispositivos usando `devices`

## Regenerar El Seed

Desde la raiz del repositorio:

```powershell
@'
import runpy
runpy.run_path('tools/generate_dashboard_seed.py', run_name='__main__')
'@ | python -
```

El archivo de salida se reescribe en:

- `sample_data/firebase_rtdb_dashboard_seed_2026_w12_w17.json`

## Uso Seguro

No importes este seed en la raiz de una base real si no quieres reemplazar datos. Usalo en una base de pruebas o importa solo nodos especificos.

Orden recomendado:

1. Crear un proyecto/base de prueba.
2. Importar el seed completo solo en ambiente demo.
3. Ejecutar el dashboard.
4. Confirmar usuarios, semanas, sesiones, graficas y detalles.
5. No mezclarlo con datos reales de usuarios.

## Nota Importante

Este seed contiene datos de dos dominios:

- datos propiedad del firmware:
  - maquinas
  - sesiones
  - calibraciones
  - dispositivos
- datos propiedad de la web app:
  - `bodyMetricsHistory`
  - `sessionFeedback`

La separacion es intencional. Permite validar el flujo completo del producto, no solo la subida de datos del ESP32.
