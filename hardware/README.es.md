# Notas De Hardware

Idioma: [English](README.md) | Espanol

## Hardware Principal Del Firmware

- Pantalla ESP32-S3 VIEWE de 7 pulgadas.
- Pantalla tactil RGB usando ESP32 Display Panel y LVGL.
- Lector RFID MFRC522.
- Sensor analogico ROM/motion conectado a GPIO 17.
- Red Wi-Fi para sincronizacion Firebase RTDB.

## Pines Del Firmware

Constantes actuales:

- Entrada de sensor de movimiento: GPIO 17.
- Intervalo de muestreo: 5 ms.
- RFID SS: GPIO 10.
- RFID SCK: GPIO 12.
- RFID MISO: GPIO 13.
- RFID MOSI: GPIO 11.
- RFID reset no usa pin dedicado.

## CAD E Impresion 3D

Archivos SolidWorks editables y exportes STL para mounts y enclosure estan en [`hardware/cad`](cad/).

Para ROM, conversion de senal, calibracion, fuentes de error y validacion, lee [`docs/MEASUREMENT_AND_CALIBRATION_GUIDE.es.md`](../docs/MEASUREMENT_AND_CALIBRATION_GUIDE.es.md).

## Prototipo De Deteccion De Peso

El modulo separado `weight_detection/` usa VL53L0X:

- VCC -> 3.3 V.
- GND -> GND.
- SDA -> GPIO 21.
- SCL -> GPIO 22.

Este prototipo no forma parte del firmware principal de produccion todavia.
