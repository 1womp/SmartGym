# Guia De Troubleshooting SmartGym

Idioma: [English](TROUBLESHOOTING.md) | Espanol

## Build Falla

Ejecuta:

```powershell
python -m platformio run --environment BOARD_VIEWE_UEDX80480070E_WB_A
```

Revisa:

- PlatformIO instalado.
- Nombre de environment correcto.
- Dependencias descargadas.
- Cambios locales completos.

## Puerto COM Ocupado

Sintomas:

- Upload falla con access denied.
- Monitor serial no abre.

Solucion:

- Cierra monitores seriales.
- Cierra Arduino IDE u otra terminal.
- Desconecta/reconecta ESP32.
- Reintenta upload.

## Upload Falla

- Confirma que el cable USB soporta datos.
- Confirma board/puerto correcto.
- Mantén BOOT si tu board lo requiere.
- Baja velocidad de upload si hace falta.

## Touch No Responde

- Confirma inicializacion LVGL/display.
- Revisa logs de boot.
- Reinicia el dispositivo.

## RFID No Detectado

- Confirma cableado MFRC522.
- Revisa pines SPI RFID.
- Acerca la tarjeta.
- Evita escanear durante upload activo.

## Wi-Fi No Conecta

- Revisa SSID/password configurados localmente.
- Confirma red 2.4 GHz.
- Revisa logs Wi-Fi.

## Firebase Falla

- Confirma URL/token/config.
- Confirma Wi-Fi.
- Revisa reglas RTDB.
- Revisa logs `[CloudWrite]`, `[SYNC]`, `[USER_SYNC]`.

## Cola Sync Atorada

Busca:

- `[SYNC] queue upload delayed`
- `[SYNC] upload memory mode=critical`
- logs de queue count

Mantén el dispositivo encendido hasta que drene la cola.

## NVS NOT_ENOUGH_SPACE

Causa:

- demasiadas escrituras en cola o payloads

Mitigacion:

- firmware usa checks de capacidad y chunking
- deja drenar la cola
- evita saltar summary repetidamente en builds viejos

## repSets Retrasados Por Heap

Causa:

- presion de memoria Firebase/TLS
- PATCH grande de repSet

Comportamiento esperado:

- firmware divide repSets bajo memoria restringida
- upload debe llegar a `webapp upload phase=complete`

## Grafica O ROM Incorrecto

- Recalibra ROM.
- Revisa cableado del sensor.
- Confirma direccion/inversion.
- Revisa si se cargo calibracion de usuario.

## Rep No Contada

Causas posibles:

- ROM corto.
- Top/bottom no alcanzado.
- Movimiento rapido o ruidoso.
- Calibracion incorrecta.

Haz reps suaves con rango completo.

## Freeze Al Cancelar Calibracion

- Revisa logs `[CAL]`, `[CAL_UI]` y destruccion de UI.
- Reinicia si hace falta.
- Verifica que no se hayan introducido punteros LVGL obsoletos.

## Dashboard Sin Datos

Revisa:

- UID RFID correcto.
- Ruta bajo `athleteWeeklySessions/{uid}`.
- Upload llego a `phase=complete`.
- Consola del navegador por permisos Firebase.
- Config Firebase dashboard coincide con la base del firmware.
