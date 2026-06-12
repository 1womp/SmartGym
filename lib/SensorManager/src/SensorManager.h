#pragma once

#include <Arduino.h>

// Lectura ya normalizada del sensor lineal. AppController y RepDetector
// trabajan con esta vista para no depender del ADC crudo.
struct SensorReading {
  int raw = 0;
  float filtered = 0.0f;
  float romPercentInstant = 0.0f;
  float romPercent = 0.0f;
  float positionMm = 0.0f;
  float velocityMmPerSec = 0.0f;
};

class SensorManager {
 public:
  // begin define pin y frecuencia de muestreo. El resto de la calibracion se
  // puede ajustar en caliente segun la maquina activa.
  void begin(uint8_t analogPin, uint16_t sampleIntervalMs = 20);
  bool update(uint32_t nowMs);
  SensorReading getReading() const;
  void setCalibrationRange(int minValue, int maxValue);
  void enableAutoRange(bool enabled);
  void setStrokeLengthMm(float strokeLengthMm);
  void setInvertDirection(bool invertDirection);

 private:
  uint8_t pin_ = 34;
  uint16_t sampleIntervalMs_ = 20;
  uint32_t lastSampleMs_ = 0;
  float emaAlpha_ = 0.50f;
  bool initialized_ = false;
  bool autoRange_ = true;
  int minValue_ = 1200;
  int maxValue_ = 2800;
  float strokeLengthMm_ = 500.0f;
  bool invertDirection_ = false;
  SensorReading reading_;

  float clampf(float value, float minValue, float maxValue) const;
};
