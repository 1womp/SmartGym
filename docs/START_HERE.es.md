# Empieza Aqui

Idioma: [English](START_HERE.md) | Espanol

Esta pagina es la forma mas rapida de encontrar informacion de SmartGym sin buscar manualmente por carpetas.

## Que Es SmartGym

SmartGym es un prototipo con ESP32-S3 que instrumenta una maquina de gimnasio selectorizada. Lee movimiento, normaliza rango de movimiento, detecta repeticiones y series, registra sesiones, guarda calibraciones y sincroniza entrenamientos terminados con Firebase para analizarlos en un dashboard.

## Lee Primero Esto

| Necesidad | Abrir |
| --- | --- |
| Entender el proyecto en 5 minutos | [README del proyecto](../README.es.md) |
| Usar el dispositivo durante una demo | [Manual de usuario](USER_MANUAL.es.md) |
| Entender ROM, calibracion, ecuaciones y errores | [Guia de medicion y calibracion](MEASUREMENT_AND_CALIBRATION_GUIDE.es.md) |
| Entender modulos de firmware y flujo de datos | [Arquitectura del sistema](SYSTEM_ARCHITECTURE.es.md) |
| Compilar o modificar firmware | [Guia de desarrollador](DEVELOPER_GUIDE.es.md) |
| Entender Firebase y subidas | [Guia de Firebase](FIREBASE_GUIDE.es.md) |
| Entender datos seed/demo del dashboard | [Guia de seed Firebase para dashboard](FIREBASE_DASHBOARD_SEED_GUIDE.es.md) |
| Resolver problemas de hardware, sync o UI | [Troubleshooting](TROUBLESHOOTING.es.md) |
| Encontrar CAD y STL | [CAD de hardware](../hardware/cad/README.es.md) |
| Ejecutar dashboard React | [Dashboard](../dashboard/README.es.md) |
| Probar prototipo VL53L0X | [Weight Detection](../weight_detection/README.es.md) |

## Si Vas A Construir Todo El Proyecto

1. Lee el [README del proyecto](../README.es.md).
2. Compila el firmware con los comandos del README.
3. Lee la [Guia de medicion y calibracion](MEASUREMENT_AND_CALIBRATION_GUIDE.es.md).
4. Monta hardware usando [Hardware](../hardware/README.es.md) y [CAD](../hardware/cad/README.es.md).
5. Configura Firebase usando la [Guia de Firebase](FIREBASE_GUIDE.es.md).
6. Usa la [Guia de seed Firebase para dashboard](FIREBASE_DASHBOARD_SEED_GUIDE.es.md) si necesitas datos demo.
7. Ejecuta el dashboard usando [Dashboard](../dashboard/README.es.md).
8. Usa [Troubleshooting](TROUBLESHOOTING.es.md) durante pruebas de hardware.

## Flujo Principal De Datos

```text
Sensor de movimiento -> ESP32-S3 -> normalizacion ROM -> deteccion de reps
RFID -> perfil de usuario -> calibracion -> recomendacion
Session recorder -> cola NVS -> Firebase RTDB -> dashboard React
```

## Conceptos Clave

| Concepto | Explicacion corta | Detalles |
| --- | --- | --- |
| ROM | Rango de movimiento calibrado de usuario, normalizado de abajo a arriba. | [Guia de medicion y calibracion](MEASUREMENT_AND_CALIBRATION_GUIDE.es.md) |
| Pin load | Posicion fisica del pin de peso. Pertenece a la maquina, no al usuario. | [Guia de medicion y calibracion](MEASUREMENT_AND_CALIBRATION_GUIDE.es.md) |
| Carga recomendada | Consejo especifico por usuario. No mueve el pin fisico. | [Guia de medicion y calibracion](MEASUREMENT_AND_CALIBRATION_GUIDE.es.md) |
| Deteccion de reps | Maquina de estados basada en ROM: abajo, subida, arriba, bajada. | [Guia de desarrollador](DEVELOPER_GUIDE.es.md) |
| Cola Firebase | Cola local en NVS para reintentos seguros. | [Guia de Firebase](FIREBASE_GUIDE.es.md) |
| Dashboard | App React que lee historial de Firebase. | [Dashboard](../dashboard/README.es.md) |

## Donde Viven Los Archivos

| Area | Carpeta |
| --- | --- |
| Firmware ESP32-S3 | `src/`, `lib/`, `boards/`, `platformio.ini` |
| Documentacion | `docs/` |
| Dashboard | `dashboard/` |
| CAD y STL | `hardware/cad/` |
| Notas de hardware | `hardware/` |
| Sketch VL53L0X | `weight_detection/` |
| Utilidades | `scripts/`, `tools/` |
| Datos de ejemplo | `sample_data/` |

## Checklist Antes De Demo

1. Firmware compila y sube.
2. Wi-Fi conecta.
3. RFID lee un usuario conocido.
4. User sync termina antes de habilitar START.
5. Calibracion ROM se carga o se muestra fallback seguro.
6. Pin load y recomendacion aparecen separados.
7. La sesion inicia, cuenta reps y muestra resumen.
8. La subida a Firebase llega a complete.
9. El dashboard muestra la sesion.

## Confusiones Comunes

| Confusion | Modelo correcto |
| --- | --- |
| Cambio la recomendacion, entonces cambio el pin. | No. La recomendacion es consejo; el pin load es estado fisico. |
| Todo movimiento cuenta como repeticion. | No. La rep debe pasar ROM, tiempo y orden de fases. |
| Firebase sube todo en una sola request. | No. Se guarda localmente y se sube por fases. |
| El dashboard define el modelo de datos. | No. El firmware escribe; el dashboard lee. |
| El VL53L0X controla automaticamente el firmware. | No. Es prototipo separado hasta integrarse intencionalmente. |

## Seguridad

No commitees contrasenas Wi-Fi, secretos de Firebase, service accounts, tokens ni archivos `.env` privados. Usa ejemplos y variables de entorno locales.
