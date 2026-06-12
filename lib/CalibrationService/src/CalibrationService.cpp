#include "CalibrationService.h"

void CalibrationService::start(float baseWeightKg) {
  // Reiniciar antes de arrancar evita mezclar restos de una calibracion vieja
  // con la nueva tanda de reps.
  reset();
  active_ = true;
  summary_.baseWeightKg = baseWeightKg;
}

bool CalibrationService::isActive() const {
  return active_;
}

bool CalibrationService::isComplete() const {
  return summary_.ready;
}

uint8_t CalibrationService::getCapturedRepCount() const {
  return validRepCount_;
}

CalibrationSummary CalibrationService::processRep(const RepMetrics& rep) {
  if (!active_ || !rep.completed || !rep.valid) {
    return summary_;
  }

  if (validRepCount_ < kTargetReps) {
    validReps_[validRepCount_] = rep;
    validRepCount_++;
  }

  float romSum = 0.0f;
  float concentricSum = 0.0f;
  float velocitySum = 0.0f;

  for (uint8_t i = 0; i < validRepCount_; ++i) {
    romSum += validReps_[i].romPercent;
    concentricSum += static_cast<float>(validReps_[i].concentricTimeMs);
    velocitySum += validReps_[i].peakVelocityPctPerSec;
  }

  summary_.validRepCount = validRepCount_;
  summary_.avgRomPercent = romSum / validRepCount_;
  summary_.avgConcentricTimeMs = concentricSum / validRepCount_;
  summary_.avgPeakVelocityPctPerSec = velocitySum / validRepCount_;

  if (validRepCount_ >= kTargetReps) {
    summary_.suggestedWeightKg = computeSuggestedWeightKg();
    summary_.ready = true;
    active_ = false;
  }

  return summary_;
}

CalibrationSummary CalibrationService::getSummary() const {
  return summary_;
}

void CalibrationService::reset() {
  active_ = false;
  summary_ = CalibrationSummary{};
  validRepCount_ = 0;
}

float CalibrationService::computeSuggestedWeightKg() const {
  float factor = 1.0f;

  // Heuristica MVP: si la ejecucion sale muy rapida, el usuario probablemente
  // tolera mas carga; si sale lenta, conviene bajar.
  if (summary_.avgPeakVelocityPctPerSec > 140.0f) {
    factor = 1.10f;
  } else if (summary_.avgPeakVelocityPctPerSec < 80.0f) {
    factor = 0.90f;
  }

  // Si el ROM promedio es corto, hacemos el ajuste mas conservador para no
  // sugerir demasiado peso con una tecnica incompleta.
  if (summary_.avgRomPercent < 75.0f) {
    factor = min(factor, 0.95f);
  }

  // El MVP propone un peso simple y acotado; la app/web puede despues aplicar
  // reglas mas finas sin cambiar la calibracion base del firmware.
  return summary_.baseWeightKg * factor;
}
