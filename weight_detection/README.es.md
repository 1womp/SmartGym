# Prototipo De Deteccion De Peso

Idioma: [English](README.md) | Espanol

Esta carpeta contiene un prototipo independiente para detectar la posicion fisica del stack de pesas con un sensor de distancia VL53L0X.

Esta separado intencionalmente del firmware principal SmartGym. El firmware principal actualmente usa un modelo manual de pin load, donde el usuario actualiza la carga mostrada con botones tactiles.

## Que Hace

- Lee distancia desde VL53L0X por I2C.
- Promedia 30 muestras por ciclo.
- Mapea rangos de distancia a posiciones del stack.
- Mapea posiciones a peso detectado en libras.
- Confirma peso final despues de detecciones consistentes.
- Imprime distancia, stack, peso detectado, peso confirmado y estado por Serial.

## Hardware

- ESP32 o board Arduino compatible.
- Sensor VL53L0X.
- Cableado recomendado:
  - VCC -> 3.3 V
  - GND -> GND
  - SDA -> GPIO 21
  - SCL -> GPIO 22

## Dependencias

Instala la libreria Adafruit VL53L0X en Arduino IDE o PlatformIO:

- `Adafruit_VL53L0X`

## Como Correrlo

1. Abre `weight_detection_vl53l0x.ino` en Arduino IDE.
2. Instala la libreria VL53L0X.
3. Selecciona board y puerto ESP32.
4. Sube el sketch.
5. Abre Serial Monitor a 115200 baud.

## Salida Esperada

Serial imprime:

- distancia promedio en cm
- peso detectado en lb
- peso final confirmado
- contador de confirmacion
- numero de stack
- mensajes de estado

## Relacion Con Firmware SmartGym

El modulo puede convertirse en reemplazo o apoyo para entrada manual de pin load. Antes de integrarlo, los rangos distancia-peso deben calibrarse para la geometria exacta de la maquina.

## Limitaciones

- Rangos de distancia hard-coded y especificos por maquina.
- Valores actuales en libras, mientras la UI principal usa kg.
- No esta integrado con Firebase.
- No cambia recomendaciones SmartGym.
- No debe mezclarse al firmware hasta estabilizar montaje fisico y conversion.
