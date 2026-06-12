#pragma once

#include <stdint.h>
#include <Arduino.h>

// Este modulo solo conoce ROM normalizado y tiempo. Asi puede reutilizarse con
// otros sensores mientras alguien produzca una curva 0-100% estable.
// Bitmask con causas de invalidez. Una misma rep puede acumular varias.
enum RepInvalidFlags : uint8_t {
  RepInvalidNone = 0,
  RepInvalidShortRom = 1 << 0,
  RepInvalidTooFast = 1 << 1,
  RepInvalidTopNotReached = 1 << 2,
  RepInvalidNoConcentricPhase = 1 << 3,
};

// Resultado del analisis de una repeticion completa.
// Incluye validez, metricas y advertencias tecnicas que no necesariamente
// invalidan la rep.
struct RepMetrics {
  bool completed = false;
  bool valid = false;
  bool warningFastEccentric = false;
  float romPercent = 0.0f;
  float minRomPercent = 0.0f;
  float maxRomPercent = 0.0f;
  uint32_t durationMs = 0;
  uint32_t concentricTimeMs = 0;
  float peakVelocityPctPerSec = 0.0f;
  float peakEccentricVelocityPctPerSec = 0.0f;
  uint8_t invalidFlags = RepInvalidNone;
};

// Detector de repeticiones basado en una maquina de estados simple y un valor
// de ROM normalizado. No depende del sensor fisico concreto.
class RepDetector {
 public:
  // Configura umbrales de recorrido y ROM minimo aceptable.
  void begin(float lowThreshold = 20.0f, float highThreshold = 80.0f,
             float minValidRom = 60.0f);
  // Se llama en cada loop con el ROM actual para detectar cierres de rep.
  RepMetrics update(float romPercent, uint32_t nowMs);
  void reset();

 private:
  // Estados del patron Bottom -> Ascending -> Top -> Descending.
  enum class MotionState {
    Bottom,
    Ascending,
    Top,
    Descending
  };

  MotionState state_ = MotionState::Bottom;
  bool initialized_ = false;
  float lowThreshold_ = 20.0f;
  float highThreshold_ = 80.0f;
  float minValidRom_ = 60.0f;
  float lastRom_ = 0.0f;
  uint32_t lastUpdateMs_ = 0;
  uint32_t repStartMs_ = 0;
  uint32_t concentricStartMs_ = 0;
  uint32_t topReachedMs_ = 0;
  float repMinRom_ = 100.0f;
  float repMaxRom_ = 0.0f;
  float peakVelocityPctPerSec_ = 0.0f;
  float peakEccentricVelocityPctPerSec_ = 0.0f;
  float eccentricWarningThresholdPctPerSec_ = 220.0f;

  // Helpers internos para acumular min/max del recorrido en una rep.
  void trackRepWindow(float romPercent);
  void resetRepWindow(float romPercent);
};
