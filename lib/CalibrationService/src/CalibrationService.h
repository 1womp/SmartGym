#pragma once

#include <Arduino.h>
#include "RepDetector.h"

// Resultado acumulado de la calibracion automatica. Resume las reps validas
// usadas para proponer un peso inicial antes de comenzar la rutina formal.
struct CalibrationSummary {
  bool ready = false;
  uint8_t validRepCount = 0;
  float avgRomPercent = 0.0f;
  float avgConcentricTimeMs = 0.0f;
  float avgPeakVelocityPctPerSec = 0.0f;
  float baseWeightKg = 0.0f;
  float suggestedWeightKg = 0.0f;
};

class CalibrationService {
 public:
  static constexpr uint8_t kTargetReps = 5;

  // Inicia una ventana corta de captura. Solo consume reps validas completas.
  void start(float baseWeightKg);
  bool isActive() const;
  bool isComplete() const;
  uint8_t getCapturedRepCount() const;
  CalibrationSummary processRep(const RepMetrics& rep);
  CalibrationSummary getSummary() const;
  void reset();

 private:
  bool active_ = false;
  CalibrationSummary summary_;
  RepMetrics validReps_[kTargetReps];
  uint8_t validRepCount_ = 0;

  float computeSuggestedWeightKg() const;
};
