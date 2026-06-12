#include "SessionRecorder.h"

namespace {
// Traduccion minima de enum interno a la cadena estable que consumen Firebase
// y el futuro frontend.
const char* goalToText(TrainingGoal goal) {
  switch (goal) {
    case TrainingGoal::Strength:
      return "strength";
    case TrainingGoal::Hypertrophy:
      return "hypertrophy";
    case TrainingGoal::Endurance:
      return "endurance";
    case TrainingGoal::Test:
      return "test";
    case TrainingGoal::General:
    default:
      return "general";
  }
}

uint16_t countRepsInSet(const SessionHistoryRecord& record, uint8_t setNumber) {
  uint16_t count = 0;
  for (uint16_t i = 0; i < record.repCount; ++i) {
    if (record.reps[i].setNumber == setNumber) {
      count++;
    }
  }
  return count;
}

bool findSetWeightRange(const SessionHistoryRecord& record,
                        uint8_t setNumber,
                        float& outStart,
                        float& outEnd,
                        bool& outChanged) {
  // El peso queda por rep para permitir cambios a mitad del set, pero muchos
  // dashboards primero quieren un resumen rapido del set completo.
  bool found = false;
  outChanged = false;
  for (uint16_t i = 0; i < record.repCount; ++i) {
    const RepHistoryRecord& rep = record.reps[i];
    if (rep.setNumber != setNumber) {
      continue;
    }
    if (!found) {
      outStart = rep.selectedWeightKg;
      outEnd = rep.selectedWeightKg;
      found = true;
    } else {
      outEnd = rep.selectedWeightKg;
      if (fabsf(rep.selectedWeightKg - outStart) > 0.05f) {
        outChanged = true;
      }
    }
  }
  return found;
}

float computeSessionQualityScore(const SessionHistoryRecord& record) {
  const float safeTargetSets = record.targetSets > 0 ? static_cast<float>(record.targetSets) : 1.0f;
  const float plannedReps =
      safeTargetSets * static_cast<float>(record.targetRepsMax > 0 ? record.targetRepsMax : 1);
  const float repRatio = constrain(static_cast<float>(record.validReps) / max(1.0f, plannedReps), 0.0f, 1.2f);
  const float totalReps = max(1.0f, static_cast<float>(record.validReps + record.invalidReps));
  const float validRatio = constrain(static_cast<float>(record.validReps) / totalReps, 0.0f, 1.0f);

  const float romScore = constrain(record.avgRomPercent / 95.0f, 0.0f, 1.0f) * 100.0f;
  const float repScore = constrain(repRatio, 0.0f, 1.0f) * 100.0f;
  const float validScore = validRatio * 100.0f;
  const float velScore = constrain(record.avgPeakVelocityPctPerSec / 120.0f, 0.0f, 1.0f) * 100.0f;
  return (romScore * 0.35f) + (repScore * 0.25f) + (validScore * 0.25f) + (velScore * 0.15f);
}

const char* sessionQualityTierFromScore(float score) {
  if (score >= 85.0f) {
    return "excellent";
  }
  if (score >= 70.0f) {
    return "good";
  }
  if (score >= 55.0f) {
    return "ok";
  }
  return "bad";
}

void clearSessionRecordInPlace(SessionHistoryRecord& record) {
  record.sessionId.remove(0);
  record.userUid.remove(0);
  record.userDisplayName.remove(0);
  record.machineId.remove(0);
  record.machineTypeId.remove(0);
  record.machineDisplayName.remove(0);
  record.exerciseCategory.remove(0);
  record.primaryMuscleGroup.remove(0);
  record.secondaryMuscleGroup.remove(0);
  record.goal.remove(0);
  record.sessionQualityTier.remove(0);
  record.startedAtIso.remove(0);
  record.endedAtIso.remove(0);
  record.anonymous = false;
  record.calibrationBased = false;
  record.selectedWeightKg = 0.0f;
  record.suggestedWeightKg = 0.0f;
  record.userRomPercent = 0.0f;
  record.userRomBottomPct = 0.0f;
  record.userRomTopPct = 0.0f;
  record.machineIdealRomPercent = 0.0f;
  record.targetSets = 0;
  record.targetRepsMin = 0;
  record.targetRepsMax = 0;
  record.plannedRestSeconds = 0;
  record.setsCompleted = 0;
  record.validReps = 0;
  record.invalidReps = 0;
  record.fastEccentricWarnings = 0;
  record.avgRomPercent = 0.0f;
  record.bestRomPercent = 0.0f;
  record.avgConcentricTimeMs = 0.0f;
  record.avgPeakVelocityPctPerSec = 0.0f;
  record.avgPeakEccentricVelocityPctPerSec = 0.0f;
  record.sessionQualityScore = 0.0f;
  record.startedAtEpoch = 0;
  record.endedAtEpoch = 0;
  record.startMs = 0;
  record.endMs = 0;
  record.durationMs = 0;
  record.totalRestMs = 0;
  for (uint8_t i = 0; i < SessionHistoryRecord::kMaxSets; ++i) {
    record.sets[i] = SetHistoryRecord{};
  }
  record.setCount = 0;
  for (uint16_t i = 0; i < SessionHistoryRecord::kMaxReps; ++i) {
    record.reps[i] = RepHistoryRecord{};
  }
  record.repCount = 0;
}
}

void SessionRecorder::reset() {
  active_ = false;
  restActive_ = false;
  restSetNumber_ = 0;
  restStartMs_ = 0;
  clearSessionRecordInPlace(record_);
  totalRomSum_ = 0.0f;
  totalConcentricSum_ = 0.0f;
  totalPeakVelocitySum_ = 0.0f;
  totalPeakEccentricSum_ = 0.0f;
  for (uint8_t i = 0; i < SessionHistoryRecord::kMaxSets; ++i) {
    setAccumulators_[i] = SetAccumulator{};
  }
}

void SessionRecorder::start(const String& sessionId,
                            const String& userUid,
                            const String& userDisplayName,
                            const MachineProfile* machineProfile,
                            TrainingGoal goal,
                            bool anonymous,
                            float selectedWeightKg,
                            float suggestedWeightKg,
                            float userRomPercent,
                            float userRomBottomPct,
                            float userRomTopPct,
                            const GoalRecommendation* recommendation,
                            uint32_t startMs,
                            uint32_t startedAtEpoch,
                            const String& startedAtIso) {
  reset();
  active_ = true;
  record_.sessionId = sessionId;
  record_.userUid = userUid;
  record_.userDisplayName = userDisplayName;
  record_.goal = goalToText(goal);
  record_.anonymous = anonymous;
  record_.selectedWeightKg = selectedWeightKg;
  record_.suggestedWeightKg = suggestedWeightKg;
  record_.userRomPercent = userRomPercent;
  record_.userRomBottomPct = userRomBottomPct;
  record_.userRomTopPct = userRomTopPct;
  record_.startMs = startMs;
  record_.startedAtEpoch = startedAtEpoch;
  record_.startedAtIso = startedAtIso;

  if (machineProfile != nullptr) {
    record_.machineId = machineProfile->machineId;
    record_.machineTypeId = machineProfile->machineTypeId;
    record_.machineDisplayName = machineProfile->displayName;
    record_.exerciseCategory = machineProfile->exerciseCategory;
    record_.primaryMuscleGroup = machineProfile->primaryMuscleGroup;
    record_.secondaryMuscleGroup = machineProfile->secondaryMuscleGroup;
    record_.machineIdealRomPercent = machineProfile->idealRomPercent;
  }

  if (recommendation != nullptr) {
    // Capturamos el plan sugerido al inicio de la sesion para que el registro
    // siga explicando "que se esperaba hacer" incluso si luego el usuario se
    // desvia un poco en reps o descanso.
    record_.targetSets = recommendation->targetSets;
    record_.targetRepsMin = recommendation->repsMin;
    record_.targetRepsMax = recommendation->repsMax;
    record_.plannedRestSeconds = recommendation->restSeconds;
    record_.calibrationBased = suggestedWeightKg > 0.0f;
  }
}

void SessionRecorder::recordRep(const RepMetrics& rep,
                                uint8_t setNumber,
                                uint8_t repNumberInSet,
                                float selectedWeightKg,
                                uint32_t nowMs) {
  if (!active_) {
    return;
  }

  SetHistoryRecord* setRecord = getOrCreateSet(setNumber);
  if (setRecord != nullptr) {
    if (setRecord->startOffsetMs == 0) {
      setRecord->startOffsetMs = nowMs - record_.startMs;
    }
    setRecord->endOffsetMs = nowMs - record_.startMs;
  }

  if (record_.repCount < SessionHistoryRecord::kMaxReps) {
    RepHistoryRecord& repRecord = record_.reps[record_.repCount++];
    repRecord.setNumber = setNumber;
    repRecord.repNumberInSet = repNumberInSet;
    repRecord.valid = rep.valid;
    repRecord.warningFastEccentric = rep.warningFastEccentric;
    repRecord.selectedWeightKg = selectedWeightKg;
    repRecord.romPercent = rep.romPercent;
    repRecord.durationMs = rep.durationMs;
    repRecord.concentricTimeMs = rep.concentricTimeMs;
    repRecord.peakVelocityPctPerSec = rep.peakVelocityPctPerSec;
    repRecord.peakEccentricVelocityPctPerSec = rep.peakEccentricVelocityPctPerSec;
    repRecord.invalidFlags = rep.invalidFlags;
    repRecord.offsetMs = nowMs - record_.startMs;
  }

  if (rep.valid) {
    record_.validReps++;
    totalRomSum_ += rep.romPercent;
    totalConcentricSum_ += rep.concentricTimeMs;
    totalPeakVelocitySum_ += rep.peakVelocityPctPerSec;
    totalPeakEccentricSum_ += rep.peakEccentricVelocityPctPerSec;
    record_.avgRomPercent = totalRomSum_ / record_.validReps;
    record_.avgConcentricTimeMs = totalConcentricSum_ / record_.validReps;
    record_.avgPeakVelocityPctPerSec = totalPeakVelocitySum_ / record_.validReps;
    record_.avgPeakEccentricVelocityPctPerSec = totalPeakEccentricSum_ / record_.validReps;
    record_.bestRomPercent = max(record_.bestRomPercent, rep.romPercent);

    if (setRecord != nullptr) {
      setRecord->validReps++;
      SetAccumulator& accumulator = setAccumulators_[setNumber - 1];
      accumulator.romSum += rep.romPercent;
      accumulator.concentricSum += rep.concentricTimeMs;
      accumulator.peakVelSum += rep.peakVelocityPctPerSec;
      accumulator.peakEccSum += rep.peakEccentricVelocityPctPerSec;
      setRecord->avgRomPercent = accumulator.romSum / setRecord->validReps;
      setRecord->avgConcentricTimeMs = accumulator.concentricSum / setRecord->validReps;
      setRecord->avgPeakVelocityPctPerSec = accumulator.peakVelSum / setRecord->validReps;
      setRecord->avgPeakEccentricVelocityPctPerSec = accumulator.peakEccSum / setRecord->validReps;
    }
  } else {
    record_.invalidReps++;
    if (setRecord != nullptr) {
      setRecord->invalidReps++;
    }
  }

  if (rep.warningFastEccentric) {
    record_.fastEccentricWarnings++;
    if (setRecord != nullptr) {
      setRecord->fastEccentricWarnings++;
    }
  }
}

void SessionRecorder::completeSet(uint8_t setNumber,
                                  uint8_t targetRepsUsed,
                                  uint16_t plannedRestSeconds,
                                  uint32_t nowMs) {
  if (!active_) {
    return;
  }

  SetHistoryRecord* setRecord = getOrCreateSet(setNumber);
  if (setRecord == nullptr) {
    return;
  }

  setRecord->targetRepsMin = record_.targetRepsMin;
  setRecord->targetRepsMax = record_.targetRepsMax;
  setRecord->targetRepsUsed = targetRepsUsed;
  setRecord->plannedRestSeconds = plannedRestSeconds;
  setRecord->endOffsetMs = nowMs - record_.startMs;
  record_.setsCompleted = max(record_.setsCompleted, setNumber);
}

void SessionRecorder::beginRest(uint8_t setNumber, uint32_t nowMs) {
  if (!active_) {
    return;
  }

  restActive_ = true;
  restSetNumber_ = setNumber;
  restStartMs_ = nowMs;
}

void SessionRecorder::endRest(uint32_t nowMs) {
  if (!active_ || !restActive_ || restSetNumber_ == 0) {
    return;
  }

  SetHistoryRecord* setRecord = getOrCreateSet(restSetNumber_);
  if (setRecord != nullptr) {
    const uint32_t restMs = nowMs - restStartMs_;
    setRecord->actualRestSeconds = restMs / 1000U;
    record_.totalRestMs += restMs;
  }

  restActive_ = false;
  restSetNumber_ = 0;
  restStartMs_ = 0;
}

void SessionRecorder::finish(uint32_t endMs, uint32_t endedAtEpoch, const String& endedAtIso) {
  if (!active_) {
    return;
  }

  if (restActive_) {
    endRest(endMs);
  }

  record_.endMs = endMs;
  record_.endedAtEpoch = endedAtEpoch;
  record_.endedAtIso = endedAtIso;
  record_.durationMs = endMs >= record_.startMs ? endMs - record_.startMs : 0;
  record_.sessionQualityScore = computeSessionQualityScore(record_);
  record_.sessionQualityTier = sessionQualityTierFromScore(record_.sessionQualityScore);
  active_ = false;
}

bool SessionRecorder::isActive() const {
  return active_;
}

const SessionHistoryRecord& SessionRecorder::getRecord() const {
  return record_;
}

void SessionRecorder::loadRecord(const SessionHistoryRecord& record) {
  reset();
  record_ = record;
}

SetHistoryRecord* SessionRecorder::getOrCreateSet(uint8_t setNumber) {
  if (setNumber == 0 || setNumber > SessionHistoryRecord::kMaxSets) {
    return nullptr;
  }

  SetHistoryRecord& setRecord = record_.sets[setNumber - 1];
  if (setRecord.setNumber == 0) {
    setRecord.setNumber = setNumber;
    record_.setCount = max(record_.setCount, setNumber);
  }

  return &setRecord;
}

String SessionRecorder::escapeJson(const String& input) {
  String out;
  out.reserve(input.length() + 8);
  for (size_t i = 0; i < input.length(); ++i) {
    const char c = input.charAt(i);
    if (c == '\\' || c == '"') {
      out += '\\';
    }
    out += c;
  }
  return out;
}

String SessionRecorder::toJson() const {
  String json;
  json.reserve(8192);
  json += "{";
  json += "\"sessionId\":\"" + escapeJson(record_.sessionId) + "\",";
  json += "\"userUid\":\"" + escapeJson(record_.userUid) + "\",";
  json += "\"userDisplayName\":\"" + escapeJson(record_.userDisplayName) + "\",";
  json += "\"machineId\":\"" + escapeJson(record_.machineId) + "\",";
  json += "\"machineTypeId\":\"" + escapeJson(record_.machineTypeId) + "\",";
  json += "\"machineDisplayName\":\"" + escapeJson(record_.machineDisplayName) + "\",";
  json += "\"exerciseCategory\":\"" + escapeJson(record_.exerciseCategory) + "\",";
  json += "\"primaryMuscleGroup\":\"" + escapeJson(record_.primaryMuscleGroup) + "\",";
  json += "\"secondaryMuscleGroup\":\"" + escapeJson(record_.secondaryMuscleGroup) + "\",";
  json += "\"goal\":\"" + escapeJson(record_.goal) + "\",";
  json += "\"anonymous\":" + String(record_.anonymous ? "true" : "false") + ",";
  json += "\"calibrationBased\":" + String(record_.calibrationBased ? "true" : "false") + ",";
  json += "\"selectedWeightKg\":" + String(record_.selectedWeightKg, 2) + ",";
  json += "\"suggestedWeightKg\":" + String(record_.suggestedWeightKg, 2) + ",";
  json += "\"userRomPercent\":" + String(record_.userRomPercent, 2) + ",";
  json += "\"userRomBottomPct\":" + String(record_.userRomBottomPct, 2) + ",";
  json += "\"userRomTopPct\":" + String(record_.userRomTopPct, 2) + ",";
  json += "\"machineIdealRomPercent\":" + String(record_.machineIdealRomPercent, 2) + ",";
  json += "\"targetSets\":" + String(record_.targetSets) + ",";
  json += "\"targetRepsMin\":" + String(record_.targetRepsMin) + ",";
  json += "\"targetRepsMax\":" + String(record_.targetRepsMax) + ",";
  json += "\"plannedRestSeconds\":" + String(record_.plannedRestSeconds) + ",";
  json += "\"setsCompleted\":" + String(record_.setsCompleted) + ",";
  json += "\"validReps\":" + String(record_.validReps) + ",";
  json += "\"invalidReps\":" + String(record_.invalidReps) + ",";
  json += "\"fastEccentricWarnings\":" + String(record_.fastEccentricWarnings) + ",";
  json += "\"avgRomPercent\":" + String(record_.avgRomPercent, 2) + ",";
  json += "\"bestRomPercent\":" + String(record_.bestRomPercent, 2) + ",";
  json += "\"avgConcentricTimeMs\":" + String(record_.avgConcentricTimeMs, 2) + ",";
  json += "\"avgPeakVelocityPctPerSec\":" + String(record_.avgPeakVelocityPctPerSec, 2) + ",";
  json += "\"avgPeakEccentricVelocityPctPerSec\":" +
          String(record_.avgPeakEccentricVelocityPctPerSec, 2) + ",";
  json += "\"sessionQualityScore\":" + String(record_.sessionQualityScore, 2) + ",";
  json += "\"sessionQualityTier\":\"" + escapeJson(record_.sessionQualityTier) + "\",";
  json += "\"startedAtEpoch\":" + String(record_.startedAtEpoch) + ",";
  json += "\"endedAtEpoch\":" + String(record_.endedAtEpoch) + ",";
  json += "\"startedAtIso\":\"" + escapeJson(record_.startedAtIso) + "\",";
  json += "\"endedAtIso\":\"" + escapeJson(record_.endedAtIso) + "\",";
  json += "\"startMs\":" + String(record_.startMs) + ",";
  json += "\"endMs\":" + String(record_.endMs) + ",";
  json += "\"durationMs\":" + String(record_.durationMs) + ",";
  json += "\"totalRestMs\":" + String(record_.totalRestMs) + ",";

  json += "\"sets\":[";
  for (uint8_t i = 0; i < record_.setCount; ++i) {
    if (i > 0) {
      json += ",";
    }
    const SetHistoryRecord& set = record_.sets[i];
    json += "{";
    json += "\"setNumber\":" + String(set.setNumber) + ",";
    json += "\"targetRepsMin\":" + String(set.targetRepsMin) + ",";
    json += "\"targetRepsMax\":" + String(set.targetRepsMax) + ",";
    json += "\"targetRepsUsed\":" + String(set.targetRepsUsed) + ",";
    json += "\"validReps\":" + String(set.validReps) + ",";
    json += "\"invalidReps\":" + String(set.invalidReps) + ",";
    json += "\"fastEccentricWarnings\":" + String(set.fastEccentricWarnings) + ",";
    json += "\"avgRomPercent\":" + String(set.avgRomPercent, 2) + ",";
    json += "\"avgConcentricTimeMs\":" + String(set.avgConcentricTimeMs, 2) + ",";
    json += "\"avgPeakVelocityPctPerSec\":" + String(set.avgPeakVelocityPctPerSec, 2) + ",";
    json += "\"avgPeakEccentricVelocityPctPerSec\":" +
            String(set.avgPeakEccentricVelocityPctPerSec, 2) + ",";
    json += "\"startOffsetMs\":" + String(set.startOffsetMs) + ",";
    json += "\"endOffsetMs\":" + String(set.endOffsetMs) + ",";
    json += "\"plannedRestSeconds\":" + String(set.plannedRestSeconds) + ",";
    json += "\"actualRestSeconds\":" + String(set.actualRestSeconds);
    json += "}";
  }
  json += "],";

  json += "\"reps\":[";
  for (uint16_t i = 0; i < record_.repCount; ++i) {
    if (i > 0) {
      json += ",";
    }
    const RepHistoryRecord& rep = record_.reps[i];
    json += "{";
    json += "\"setNumber\":" + String(rep.setNumber) + ",";
    json += "\"repNumberInSet\":" + String(rep.repNumberInSet) + ",";
    json += "\"valid\":" + String(rep.valid ? "true" : "false") + ",";
    json += "\"warningFastEccentric\":" + String(rep.warningFastEccentric ? "true" : "false") + ",";
    json += "\"selectedWeightKg\":" + String(rep.selectedWeightKg, 2) + ",";
    json += "\"romPercent\":" + String(rep.romPercent, 2) + ",";
    json += "\"durationMs\":" + String(rep.durationMs) + ",";
    json += "\"concentricTimeMs\":" + String(rep.concentricTimeMs) + ",";
    json += "\"peakVelocityPctPerSec\":" + String(rep.peakVelocityPctPerSec, 2) + ",";
    json += "\"peakEccentricVelocityPctPerSec\":" + String(rep.peakEccentricVelocityPctPerSec, 2) +
            ",";
    json += "\"invalidFlags\":" + String(rep.invalidFlags) + ",";
    json += "\"offsetMs\":" + String(rep.offsetMs);
    json += "}";
  }
  json += "]";
  json += "}";
  return json;
}

String SessionRecorder::toAthleteAnalysisJson() const {
  // Documento canonico para Firebase: combina identidad, plan, resumen y
  // metricas derivadas para que el frontend no tenga que recalcular todo.
  String json;
  json.reserve(5120);

  uint16_t invalidShortRomCount = 0;
  uint16_t invalidTooFastCount = 0;
  uint16_t invalidTopNotReachedCount = 0;
  uint16_t invalidNoConcentricCount = 0;
  uint16_t validAtOrAboveUserRomCount = 0;
  uint16_t validBelowUserRomCount = 0;
  uint16_t validAtOrAboveIdealRomCount = 0;

  int16_t bestRomRepIndex = -1;
  int16_t bestVelocityRepIndex = -1;
  int16_t firstValidRepIndex = -1;
  int16_t lastValidRepIndex = -1;
  float bestRomSeen = -1.0f;
  float bestVelocitySeen = -1.0f;

  for (uint16_t i = 0; i < record_.repCount; ++i) {
    const RepHistoryRecord& rep = record_.reps[i];
    if (!rep.valid) {
      if (rep.invalidFlags & RepInvalidShortRom) {
        invalidShortRomCount++;
      }
      if (rep.invalidFlags & RepInvalidTooFast) {
        invalidTooFastCount++;
      }
      if (rep.invalidFlags & RepInvalidTopNotReached) {
        invalidTopNotReachedCount++;
      }
      if (rep.invalidFlags & RepInvalidNoConcentricPhase) {
        invalidNoConcentricCount++;
      }
      continue;
    }

    if (firstValidRepIndex < 0) {
      firstValidRepIndex = static_cast<int16_t>(i);
    }
    lastValidRepIndex = static_cast<int16_t>(i);

    if (rep.romPercent >= record_.userRomPercent) {
      validAtOrAboveUserRomCount++;
    } else {
      validBelowUserRomCount++;
    }
    if (rep.romPercent >= record_.machineIdealRomPercent) {
      validAtOrAboveIdealRomCount++;
    }
    if (rep.romPercent > bestRomSeen) {
      bestRomSeen = rep.romPercent;
      bestRomRepIndex = static_cast<int16_t>(i);
    }
    if (rep.peakVelocityPctPerSec > bestVelocitySeen) {
      bestVelocitySeen = rep.peakVelocityPctPerSec;
      bestVelocityRepIndex = static_cast<int16_t>(i);
    }
  }

  const SetHistoryRecord* firstSet = record_.setCount > 0 ? &record_.sets[0] : nullptr;
  const SetHistoryRecord* lastSet = record_.setCount > 0 ? &record_.sets[record_.setCount - 1] : nullptr;
  const uint32_t totalRepAttempts = static_cast<uint32_t>(record_.validReps) + static_cast<uint32_t>(record_.invalidReps);
  const float validRepRate = totalRepAttempts > 0
                                 ? (static_cast<float>(record_.validReps) * 100.0f) /
                                       static_cast<float>(totalRepAttempts)
                                 : 0.0f;
  const float romComplianceRate = record_.validReps > 0
                                      ? (static_cast<float>(validAtOrAboveUserRomCount) * 100.0f) /
                                            static_cast<float>(record_.validReps)
                                      : 0.0f;
  const float idealRomHitRate = record_.validReps > 0
                                    ? (static_cast<float>(validAtOrAboveIdealRomCount) * 100.0f) /
                                          static_cast<float>(record_.validReps)
                                    : 0.0f;
  const float avgRestSecondsPerSet = record_.setsCompleted > 0
                                         ? static_cast<float>(record_.totalRestMs) / 1000.0f /
                                               static_cast<float>(record_.setsCompleted)
                                         : 0.0f;
  const float volumeLoadKg = record_.selectedWeightKg * static_cast<float>(record_.validReps);
  const float fatigueRomDrop =
      (firstSet != nullptr && lastSet != nullptr) ? firstSet->avgRomPercent - lastSet->avgRomPercent : 0.0f;
  const float fatigueVelocityDrop =
      (firstSet != nullptr && lastSet != nullptr)
          ? firstSet->avgPeakVelocityPctPerSec - lastSet->avgPeakVelocityPctPerSec
          : 0.0f;
  const float sessionQualityScore = record_.sessionQualityScore > 0.0f
                                        ? record_.sessionQualityScore
                                        : computeSessionQualityScore(record_);
  const String sessionQualityTier =
      !record_.sessionQualityTier.isEmpty() ? record_.sessionQualityTier
                                            : String(sessionQualityTierFromScore(sessionQualityScore));

  json += "{";
  json += "\"sessionId\":\"" + escapeJson(record_.sessionId) + "\",";
  json += "\"identity\":{";
  json += "\"userUid\":\"" + escapeJson(record_.userUid) + "\",";
  json += "\"userDisplayName\":\"" + escapeJson(record_.userDisplayName) + "\",";
  json += "\"anonymous\":" + String(record_.anonymous ? "true" : "false");
  json += "},";
  json += "\"machine\":{";
  json += "\"machineId\":\"" + escapeJson(record_.machineId) + "\",";
  json += "\"machineTypeId\":\"" + escapeJson(record_.machineTypeId) + "\",";
  json += "\"machineDisplayName\":\"" + escapeJson(record_.machineDisplayName) + "\",";
  json += "\"exerciseCategory\":\"" + escapeJson(record_.exerciseCategory) + "\",";
  json += "\"primaryMuscleGroup\":\"" + escapeJson(record_.primaryMuscleGroup) + "\",";
  json += "\"secondaryMuscleGroup\":\"" + escapeJson(record_.secondaryMuscleGroup) + "\",";
  json += "\"machineIdealRomPercent\":" + String(record_.machineIdealRomPercent, 2);
  json += "},";
  json += "\"timing\":{";
  json += "\"startedAtEpoch\":" + String(record_.startedAtEpoch) + ",";
  json += "\"endedAtEpoch\":" + String(record_.endedAtEpoch) + ",";
  json += "\"startedAtIso\":\"" + escapeJson(record_.startedAtIso) + "\",";
  json += "\"endedAtIso\":\"" + escapeJson(record_.endedAtIso) + "\",";
  json += "\"startMs\":" + String(record_.startMs) + ",";
  json += "\"endMs\":" + String(record_.endMs) + ",";
  json += "\"durationMs\":" + String(record_.durationMs) + ",";
  json += "\"totalRestMs\":" + String(record_.totalRestMs);
  json += "},";
  json += "\"plan\":{";
  json += "\"goal\":\"" + escapeJson(record_.goal) + "\",";
  json += "\"calibrationBased\":" + String(record_.calibrationBased ? "true" : "false") + ",";
  json += "\"selectedWeightKg\":" + String(record_.selectedWeightKg, 2) + ",";
  json += "\"suggestedWeightKg\":" + String(record_.suggestedWeightKg, 2) + ",";
  json += "\"userRomPercent\":" + String(record_.userRomPercent, 2) + ",";
  json += "\"userRomBottomPct\":" + String(record_.userRomBottomPct, 2) + ",";
  json += "\"userRomTopPct\":" + String(record_.userRomTopPct, 2) + ",";
  json += "\"targetSets\":" + String(record_.targetSets) + ",";
  json += "\"targetRepsMin\":" + String(record_.targetRepsMin) + ",";
  json += "\"targetRepsMax\":" + String(record_.targetRepsMax) + ",";
  json += "\"plannedRestSeconds\":" + String(record_.plannedRestSeconds);
  json += "},";
  json += "\"summary\":{";
  json += "\"setsCompleted\":" + String(record_.setsCompleted) + ",";
  json += "\"setCount\":" + String(record_.setCount) + ",";
  json += "\"repCount\":" + String(record_.repCount) + ",";
  json += "\"validReps\":" + String(record_.validReps) + ",";
  json += "\"invalidReps\":" + String(record_.invalidReps) + ",";
  json += "\"fastEccentricWarnings\":" + String(record_.fastEccentricWarnings) + ",";
  json += "\"validRepRate\":" + String(validRepRate, 2) + ",";
  json += "\"volumeLoadKg\":" + String(volumeLoadKg, 2) + ",";
  json += "\"avgRomPercent\":" + String(record_.avgRomPercent, 2) + ",";
  json += "\"bestRomPercent\":" + String(record_.bestRomPercent, 2) + ",";
  json += "\"avgConcentricTimeMs\":" + String(record_.avgConcentricTimeMs, 2) + ",";
  json += "\"avgPeakVelocityPctPerSec\":" + String(record_.avgPeakVelocityPctPerSec, 2) + ",";
  json += "\"avgPeakEccentricVelocityPctPerSec\":" +
          String(record_.avgPeakEccentricVelocityPctPerSec, 2) + ",";
  json += "\"sessionQualityScore\":" + String(sessionQualityScore, 2) + ",";
  json += "\"sessionQualityTier\":\"" + escapeJson(sessionQualityTier) + "\"";
  json += "},";

  json += "\"analysis\":{";
  json += "\"firstSetAvgRomPercent\":" + String(firstSet != nullptr ? firstSet->avgRomPercent : 0.0f, 2) + ",";
  json += "\"lastSetAvgRomPercent\":" + String(lastSet != nullptr ? lastSet->avgRomPercent : 0.0f, 2) + ",";
  json += "\"firstSetAvgPeakVelocityPctPerSec\":" +
          String(firstSet != nullptr ? firstSet->avgPeakVelocityPctPerSec : 0.0f, 2) + ",";
  json += "\"lastSetAvgPeakVelocityPctPerSec\":" +
          String(lastSet != nullptr ? lastSet->avgPeakVelocityPctPerSec : 0.0f, 2) + ",";
  json += "\"validAtOrAboveUserRomCount\":" + String(validAtOrAboveUserRomCount) + ",";
  json += "\"validBelowUserRomCount\":" + String(validBelowUserRomCount) + ",";
  json += "\"validAtOrAboveIdealRomCount\":" + String(validAtOrAboveIdealRomCount) + ",";
  json += "\"romComplianceRate\":" + String(romComplianceRate, 2) + ",";
  json += "\"idealRomHitRate\":" + String(idealRomHitRate, 2) + ",";
  json += "\"avgRestSecondsPerSet\":" + String(avgRestSecondsPerSet, 2) + ",";
  json += "\"fatigueRomDrop\":" + String(fatigueRomDrop, 2) + ",";
  json += "\"fatigueVelocityDrop\":" + String(fatigueVelocityDrop, 2) + ",";
  json += "\"invalidShortRomCount\":" + String(invalidShortRomCount) + ",";
  json += "\"invalidTooFastCount\":" + String(invalidTooFastCount) + ",";
  json += "\"invalidTopNotReachedCount\":" + String(invalidTopNotReachedCount) + ",";
  json += "\"invalidNoConcentricCount\":" + String(invalidNoConcentricCount);
  json += "},";

  json += "\"setOverview\":[";
  for (uint8_t i = 0; i < record_.setCount; ++i) {
    if (i > 0) {
      json += ",";
    }
    const SetHistoryRecord& set = record_.sets[i];
    float weightStart = 0.0f;
    float weightEnd = 0.0f;
    bool weightChanged = false;
    const bool hasWeightRange = findSetWeightRange(record_, set.setNumber, weightStart, weightEnd, weightChanged);
    json += "{";
    json += "\"setNumber\":" + String(set.setNumber) + ",";
    json += "\"repCount\":" + String(countRepsInSet(record_, set.setNumber)) + ",";
    json += "\"targetRepsUsed\":" + String(set.targetRepsUsed) + ",";
    json += "\"validReps\":" + String(set.validReps) + ",";
    json += "\"invalidReps\":" + String(set.invalidReps) + ",";
    json += "\"fastEccentricWarnings\":" + String(set.fastEccentricWarnings) + ",";
    json += "\"selectedWeightKgStart\":" + String(hasWeightRange ? weightStart : 0.0f, 2) + ",";
    json += "\"selectedWeightKgEnd\":" + String(hasWeightRange ? weightEnd : 0.0f, 2) + ",";
    json += "\"weightChangedDuringSet\":" + String(weightChanged ? "true" : "false") + ",";
    json += "\"avgRomPercent\":" + String(set.avgRomPercent, 2) + ",";
    json += "\"avgConcentricTimeMs\":" + String(set.avgConcentricTimeMs, 2) + ",";
    json += "\"avgPeakVelocityPctPerSec\":" + String(set.avgPeakVelocityPctPerSec, 2) + ",";
    json += "\"avgPeakEccentricVelocityPctPerSec\":" +
            String(set.avgPeakEccentricVelocityPctPerSec, 2) + ",";
    json += "\"plannedRestSeconds\":" + String(set.plannedRestSeconds) + ",";
    json += "\"actualRestSeconds\":" + String(set.actualRestSeconds);
    json += "}";
  }
  json += "],";

  json += "\"paths\":{";
  json += "\"setDetails\":\"setDetails\",";
  json += "\"repSets\":\"repSets\"";
  json += "},";

  json += "\"representativeReps\":{";
  auto appendRep = [&](const char* key, int16_t repIndex) {
    json += "\"";
    json += key;
    json += "\":";
    if (repIndex < 0) {
      json += "null";
      return;
    }
    const RepHistoryRecord& rep = record_.reps[repIndex];
    json += "{";
    json += "\"setNumber\":" + String(rep.setNumber) + ",";
    json += "\"repNumberInSet\":" + String(rep.repNumberInSet) + ",";
    json += "\"selectedWeightKg\":" + String(rep.selectedWeightKg, 2) + ",";
    json += "\"romPercent\":" + String(rep.romPercent, 2) + ",";
    json += "\"durationMs\":" + String(rep.durationMs) + ",";
    json += "\"concentricTimeMs\":" + String(rep.concentricTimeMs) + ",";
    json += "\"peakVelocityPctPerSec\":" + String(rep.peakVelocityPctPerSec, 2) + ",";
    json += "\"peakEccentricVelocityPctPerSec\":" +
            String(rep.peakEccentricVelocityPctPerSec, 2);
    json += "}";
  };
  appendRep("firstValid", firstValidRepIndex);
  json += ",";
  appendRep("bestRom", bestRomRepIndex);
  json += ",";
  appendRep("bestVelocity", bestVelocityRepIndex);
  json += ",";
  appendRep("lastValid", lastValidRepIndex);
  json += "}";

  json += "}";
  return json;
}

String SessionRecorder::toSetDetailsJson() const {
  // setDetails separa el analisis por set para que la app pueda abrir tablas o
  // comparativas de fatiga sin descargar primero todas las reps.
  String json;
  json.reserve(2048);
  json += "{";
  json += "\"sessionId\":\"" + escapeJson(record_.sessionId) + "\",";
  json += "\"setCount\":" + String(record_.setCount) + ",";
  json += "\"sets\":{";
  for (uint8_t i = 0; i < record_.setCount; ++i) {
    if (i > 0) {
      json += ",";
    }
    const SetHistoryRecord& set = record_.sets[i];
    float weightStart = 0.0f;
    float weightEnd = 0.0f;
    bool weightChanged = false;
    const bool hasWeightRange = findSetWeightRange(record_, set.setNumber, weightStart, weightEnd, weightChanged);
    json += "\"set" + String(set.setNumber) + "\":{";
    json += "\"setNumber\":" + String(set.setNumber) + ",";
    json += "\"repCount\":" + String(countRepsInSet(record_, set.setNumber)) + ",";
    json += "\"targetRepsMin\":" + String(set.targetRepsMin) + ",";
    json += "\"targetRepsMax\":" + String(set.targetRepsMax) + ",";
    json += "\"targetRepsUsed\":" + String(set.targetRepsUsed) + ",";
    json += "\"validReps\":" + String(set.validReps) + ",";
    json += "\"invalidReps\":" + String(set.invalidReps) + ",";
    json += "\"fastEccentricWarnings\":" + String(set.fastEccentricWarnings) + ",";
    json += "\"selectedWeightKgStart\":" + String(hasWeightRange ? weightStart : 0.0f, 2) + ",";
    json += "\"selectedWeightKgEnd\":" + String(hasWeightRange ? weightEnd : 0.0f, 2) + ",";
    json += "\"weightChangedDuringSet\":" + String(weightChanged ? "true" : "false") + ",";
    json += "\"avgRomPercent\":" + String(set.avgRomPercent, 2) + ",";
    json += "\"avgConcentricTimeMs\":" + String(set.avgConcentricTimeMs, 2) + ",";
    json += "\"avgPeakVelocityPctPerSec\":" + String(set.avgPeakVelocityPctPerSec, 2) + ",";
    json += "\"avgPeakEccentricVelocityPctPerSec\":" +
            String(set.avgPeakEccentricVelocityPctPerSec, 2) + ",";
    json += "\"startOffsetMs\":" + String(set.startOffsetMs) + ",";
    json += "\"endOffsetMs\":" + String(set.endOffsetMs) + ",";
    json += "\"plannedRestSeconds\":" + String(set.plannedRestSeconds) + ",";
    json += "\"actualRestSeconds\":" + String(set.actualRestSeconds);
    json += "}";
  }
  json += "}";
  json += "}";
  return json;
}

String SessionRecorder::toRepSetJson(uint8_t setNumber) const {
  // repSets/setN mantiene el detalle profundo, pero particionado por set para
  // que un solo blob no crezca demasiado para RTDB ni para la app.
  String json;
  json.reserve(3072);
  const SetHistoryRecord* setRecord =
      (setNumber > 0 && setNumber <= record_.setCount) ? &record_.sets[setNumber - 1] : nullptr;
  json += "{";
  json += "\"sessionId\":\"" + escapeJson(record_.sessionId) + "\",";
  json += "\"setNumber\":" + String(setNumber) + ",";
  uint16_t count = 0;
  for (uint16_t i = 0; i < record_.repCount; ++i) {
    if (record_.reps[i].setNumber == setNumber) {
      count++;
    }
  }
  json += "\"count\":" + String(count) + ",";
  json += "\"setSummary\":";
  if (setRecord == nullptr || setRecord->setNumber == 0) {
    json += "null,";
  } else {
    float weightStart = 0.0f;
    float weightEnd = 0.0f;
    bool weightChanged = false;
    const bool hasWeightRange = findSetWeightRange(record_, setNumber, weightStart, weightEnd, weightChanged);
    json += "{";
    json += "\"repCount\":" + String(count) + ",";
    json += "\"targetRepsMin\":" + String(setRecord->targetRepsMin) + ",";
    json += "\"targetRepsMax\":" + String(setRecord->targetRepsMax) + ",";
    json += "\"targetRepsUsed\":" + String(setRecord->targetRepsUsed) + ",";
    json += "\"validReps\":" + String(setRecord->validReps) + ",";
    json += "\"invalidReps\":" + String(setRecord->invalidReps) + ",";
    json += "\"fastEccentricWarnings\":" + String(setRecord->fastEccentricWarnings) + ",";
    json += "\"selectedWeightKgStart\":" + String(hasWeightRange ? weightStart : 0.0f, 2) + ",";
    json += "\"selectedWeightKgEnd\":" + String(hasWeightRange ? weightEnd : 0.0f, 2) + ",";
    json += "\"weightChangedDuringSet\":" + String(weightChanged ? "true" : "false") + ",";
    json += "\"avgRomPercent\":" + String(setRecord->avgRomPercent, 2) + ",";
    json += "\"avgConcentricTimeMs\":" + String(setRecord->avgConcentricTimeMs, 2) + ",";
    json += "\"avgPeakVelocityPctPerSec\":" + String(setRecord->avgPeakVelocityPctPerSec, 2) + ",";
    json += "\"avgPeakEccentricVelocityPctPerSec\":" +
            String(setRecord->avgPeakEccentricVelocityPctPerSec, 2) + ",";
    json += "\"startOffsetMs\":" + String(setRecord->startOffsetMs) + ",";
    json += "\"endOffsetMs\":" + String(setRecord->endOffsetMs) + ",";
    json += "\"plannedRestSeconds\":" + String(setRecord->plannedRestSeconds) + ",";
    json += "\"actualRestSeconds\":" + String(setRecord->actualRestSeconds);
    json += "},";
  }
  json += "\"reps\":[";
  bool first = true;
  for (uint16_t i = 0; i < record_.repCount; ++i) {
    const RepHistoryRecord& rep = record_.reps[i];
    if (rep.setNumber != setNumber) {
      continue;
    }
    if (!first) {
      json += ",";
    }
    first = false;
    json += "{";
    json += "\"index\":" + String(i) + ",";
    json += "\"setNumber\":" + String(rep.setNumber) + ",";
    json += "\"repNumberInSet\":" + String(rep.repNumberInSet) + ",";
    json += "\"valid\":" + String(rep.valid ? "true" : "false") + ",";
    json += "\"warningFastEccentric\":" + String(rep.warningFastEccentric ? "true" : "false") + ",";
    json += "\"selectedWeightKg\":" + String(rep.selectedWeightKg, 2) + ",";
    json += "\"romPercent\":" + String(rep.romPercent, 2) + ",";
    json += "\"durationMs\":" + String(rep.durationMs) + ",";
    json += "\"concentricTimeMs\":" + String(rep.concentricTimeMs) + ",";
    json += "\"peakVelocityPctPerSec\":" + String(rep.peakVelocityPctPerSec, 2) + ",";
    json += "\"peakEccentricVelocityPctPerSec\":" + String(rep.peakEccentricVelocityPctPerSec, 2) +
            ",";
    json += "\"invalidFlags\":" + String(rep.invalidFlags) + ",";
    json += "\"offsetMs\":" + String(rep.offsetMs);
    json += "}";
  }
  json += "]";
  json += "}";
  return json;
}
