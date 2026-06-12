#include "RepDetector.h"

void RepDetector::begin(float lowThreshold, float highThreshold, float minValidRom) {
  // low/highThreshold delimitan el recorrido util y minValidRom define el ROM
  // minimo para que una repeticion cuente como valida.
  lowThreshold_ = lowThreshold;
  highThreshold_ = highThreshold;
  minValidRom_ = minValidRom;
  reset();
}

RepMetrics RepDetector::update(float romPercent, uint32_t nowMs) {
  RepMetrics result;

  if (!initialized_) {
    // La primera muestra solo inicializa referencias; todavia no hay
    // suficiente contexto para inferir direccion de movimiento.
    initialized_ = true;
    lastRom_ = romPercent;
    lastUpdateMs_ = nowMs;
    return result;
  }

  const uint32_t deltaMs = max<uint32_t>(1, nowMs - lastUpdateMs_);
  const float velocityPctPerSec = ((romPercent - lastRom_) * 1000.0f) / deltaMs;
  peakVelocityPctPerSec_ = max(peakVelocityPctPerSec_, abs(velocityPctPerSec));
  if (velocityPctPerSec < 0.0f) {
    peakEccentricVelocityPctPerSec_ =
        max(peakEccentricVelocityPctPerSec_, abs(velocityPctPerSec));
  }

  trackRepWindow(romPercent);

  // La deteccion usa una maquina de estados simple:
  // Bottom -> Ascending -> Top -> Descending -> Bottom
  // Esto hace el comportamiento mas predecible que usar solo umbrales sueltos.
  switch (state_) {
    case MotionState::Bottom:
      if (romPercent > lowThreshold_ && velocityPctPerSec > 0.0f) {
        state_ = MotionState::Ascending;
        repStartMs_ = nowMs;
        concentricStartMs_ = nowMs;
        resetRepWindow(romPercent);
      }
      break;

    case MotionState::Ascending:
      if (romPercent >= highThreshold_) {
        state_ = MotionState::Top;
        topReachedMs_ = nowMs;
      } else if (velocityPctPerSec < -5.0f) {
        state_ = MotionState::Descending;
      }
      break;

    case MotionState::Top:
      if (velocityPctPerSec < -5.0f) {
        state_ = MotionState::Descending;
      }
      break;

    case MotionState::Descending:
      if (romPercent <= lowThreshold_) {
        state_ = MotionState::Bottom;
        result.completed = true;
        // Cuando cerramos el ciclo en Bottom calculamos todas las metricas de
        // la rep usando el rango y tiempos acumulados durante el recorrido.
        result.romPercent = repMaxRom_ - repMinRom_;
        result.minRomPercent = repMinRom_;
        result.maxRomPercent = repMaxRom_;
        result.durationMs = nowMs - repStartMs_;
        result.concentricTimeMs = topReachedMs_ > concentricStartMs_
                                      ? topReachedMs_ - concentricStartMs_
                                      : 0;
        result.peakVelocityPctPerSec = peakVelocityPctPerSec_;
        result.peakEccentricVelocityPctPerSec = peakEccentricVelocityPctPerSec_;
        result.warningFastEccentric =
            result.peakEccentricVelocityPctPerSec >= eccentricWarningThresholdPctPerSec_;

        if (result.romPercent < minValidRom_) {
          result.invalidFlags |= RepInvalidShortRom;
        }
        if (result.durationMs <= 250) {
          result.invalidFlags |= RepInvalidTooFast;
        }
        if (repMaxRom_ < highThreshold_) {
          result.invalidFlags |= RepInvalidTopNotReached;
        }
        if (result.concentricTimeMs == 0) {
          result.invalidFlags |= RepInvalidNoConcentricPhase;
        }

        // Una rep es valida solo si no se disparo ningun motivo de invalidez.
        // Algunas advertencias tecnicas, como la bajada rapida, no invalidan.
        result.valid = result.invalidFlags == RepInvalidNone;
        resetRepWindow(romPercent);
      }
      break;
  }

  lastRom_ = romPercent;
  lastUpdateMs_ = nowMs;
  return result;
}

void RepDetector::reset() {
  state_ = MotionState::Bottom;
  initialized_ = false;
  lastRom_ = 0.0f;
  lastUpdateMs_ = 0;
  repStartMs_ = 0;
  concentricStartMs_ = 0;
  topReachedMs_ = 0;
  resetRepWindow(0.0f);
}

void RepDetector::trackRepWindow(float romPercent) {
  // Guardamos minimo y maximo vistos dentro de la rep para estimar ROM real.
  repMinRom_ = min(repMinRom_, romPercent);
  repMaxRom_ = max(repMaxRom_, romPercent);
}

void RepDetector::resetRepWindow(float romPercent) {
  // Cada nueva rep reinicia sus acumuladores de rango y velocidad.
  repMinRom_ = romPercent;
  repMaxRom_ = romPercent;
  peakVelocityPctPerSec_ = 0.0f;
  peakEccentricVelocityPctPerSec_ = 0.0f;
}
