#include "SensorManager.h"

// Traduce ADC crudo a una señal usable para deteccion de movimiento y reps.
void SensorManager::begin(uint8_t analogPin, uint16_t sampleIntervalMs) {
  pin_ = analogPin;
  sampleIntervalMs_ = sampleIntervalMs;
  analogReadResolution(12);
  initialized_ = false;
}

bool SensorManager::update(uint32_t nowMs) {
  if (nowMs - lastSampleMs_ < sampleIntervalMs_) {
    return false;
  }

  const uint32_t deltaMs = max<uint32_t>(1, nowMs - lastSampleMs_);
  const float previousPositionMm = reading_.positionMm;
  lastSampleMs_ = nowMs;
  reading_.raw = analogRead(pin_);

  if (!initialized_) {
    reading_.filtered = static_cast<float>(reading_.raw);
    initialized_ = true;
  } else {
    // EMA corta suficiente para quitar ruido del ADC sin volver demasiado
    // lenta la respuesta al movimiento real.
    reading_.filtered = (emaAlpha_ * static_cast<float>(reading_.raw)) +
                        ((1.0f - emaAlpha_) * reading_.filtered);
  }

  if (autoRange_) {
    // Durante el MVP el firmware aprende min/max observados para evitar una
    // calibracion manual estricta del sensor en cada arranque.
    minValue_ = min(minValue_, static_cast<int>(reading_.filtered));
    maxValue_ = max(maxValue_, static_cast<int>(reading_.filtered));
  }

  const int range = max(1, maxValue_ - minValue_);
  float normalizedInstant =
      (static_cast<float>(reading_.raw) - static_cast<float>(minValue_)) / static_cast<float>(range);
  normalizedInstant = clampf(normalizedInstant, 0.0f, 1.0f);

  float normalizedFiltered =
      (reading_.filtered - static_cast<float>(minValue_)) / static_cast<float>(range);
  normalizedFiltered = clampf(normalizedFiltered, 0.0f, 1.0f);

  if (invertDirection_) {
    normalizedInstant = 1.0f - normalizedInstant;
    normalizedFiltered = 1.0f - normalizedFiltered;
  }

  // A partir del normalizado construimos las magnitudes que usa todo el resto
  // del sistema: posicion fisica aproximada, ROM 0-100 y velocidad lineal.
  reading_.positionMm = normalizedFiltered * strokeLengthMm_;
  reading_.romPercentInstant = clampf(normalizedInstant * 100.0f, 0.0f, 100.0f);
  reading_.romPercent = clampf(normalizedFiltered * 100.0f, 0.0f, 100.0f);
  reading_.velocityMmPerSec =
      ((reading_.positionMm - previousPositionMm) * 1000.0f) / deltaMs;
  return true;
}

SensorReading SensorManager::getReading() const {
  return reading_;
}

void SensorManager::setCalibrationRange(int minValue, int maxValue) {
  minValue_ = min(minValue, maxValue);
  maxValue_ = max(minValue, maxValue);
}

void SensorManager::enableAutoRange(bool enabled) {
  autoRange_ = enabled;
}

void SensorManager::setStrokeLengthMm(float strokeLengthMm) {
  strokeLengthMm_ = max(1.0f, strokeLengthMm);
}

void SensorManager::setInvertDirection(bool invertDirection) {
  invertDirection_ = invertDirection;
}

float SensorManager::clampf(float value, float minValue, float maxValue) const {
  if (value < minValue) {
    return minValue;
  }
  if (value > maxValue) {
    return maxValue;
  }
  return value;
}
