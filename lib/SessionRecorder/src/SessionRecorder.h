#pragma once

#include <Arduino.h>
#include "MachineRegistry.h"
#include "RepDetector.h"

// Snapshot de una rep individual listo para analytics y para reconstruir la
// sesion mas tarde en Firebase o en la app.
struct RepHistoryRecord {
  uint8_t setNumber = 0;
  uint8_t repNumberInSet = 0;
  bool valid = false;
  bool warningFastEccentric = false;
  float selectedWeightKg = 0.0f;
  float romPercent = 0.0f;
  uint32_t durationMs = 0;
  uint32_t concentricTimeMs = 0;
  float peakVelocityPctPerSec = 0.0f;
  float peakEccentricVelocityPctPerSec = 0.0f;
  uint8_t invalidFlags = 0;
  uint32_t offsetMs = 0;
};

struct SetHistoryRecord {
  uint8_t setNumber = 0;
  uint8_t targetRepsMin = 0;
  uint8_t targetRepsMax = 0;
  uint8_t targetRepsUsed = 0;
  uint8_t validReps = 0;
  uint8_t invalidReps = 0;
  uint8_t fastEccentricWarnings = 0;
  float avgRomPercent = 0.0f;
  float avgConcentricTimeMs = 0.0f;
  float avgPeakVelocityPctPerSec = 0.0f;
  float avgPeakEccentricVelocityPctPerSec = 0.0f;
  uint32_t startOffsetMs = 0;
  uint32_t endOffsetMs = 0;
  uint16_t plannedRestSeconds = 0;
  uint16_t actualRestSeconds = 0;
};

// Resumen rico de sesion pensado para persistencia local y futura subida a
// Firebase. Incluye resumen global, detalle por set y detalle por rep.
struct SessionHistoryRecord {
  static constexpr uint8_t kMaxSets = 8;
  static constexpr uint8_t kMaxReps = 80;

  String sessionId;
  String userUid;
  String userDisplayName;
  String machineId;
  String machineTypeId;
  String machineDisplayName;
  String exerciseCategory;
  String primaryMuscleGroup;
  String secondaryMuscleGroup;
  String goal;
  bool anonymous = false;
  bool calibrationBased = false;
  float selectedWeightKg = 0.0f;
  float suggestedWeightKg = 0.0f;
  float userRomPercent = 0.0f;
  float userRomBottomPct = 0.0f;
  float userRomTopPct = 0.0f;
  float machineIdealRomPercent = 0.0f;
  uint8_t targetSets = 0;
  uint8_t targetRepsMin = 0;
  uint8_t targetRepsMax = 0;
  uint16_t plannedRestSeconds = 0;
  uint8_t setsCompleted = 0;
  uint16_t validReps = 0;
  uint16_t invalidReps = 0;
  uint16_t fastEccentricWarnings = 0;
  float avgRomPercent = 0.0f;
  float bestRomPercent = 0.0f;
  float avgConcentricTimeMs = 0.0f;
  float avgPeakVelocityPctPerSec = 0.0f;
  float avgPeakEccentricVelocityPctPerSec = 0.0f;
  float sessionQualityScore = 0.0f;  // 0..100 aggregated session quality
  String sessionQualityTier;         // excellent/good/ok/bad
  uint32_t startedAtEpoch = 0;
  uint32_t endedAtEpoch = 0;
  String startedAtIso;
  String endedAtIso;
  uint32_t startMs = 0;
  uint32_t endMs = 0;
  uint32_t durationMs = 0;
  uint32_t totalRestMs = 0;
  SetHistoryRecord sets[kMaxSets];
  uint8_t setCount = 0;
  RepHistoryRecord reps[kMaxReps];
  uint16_t repCount = 0;
};

class SessionRecorder {
 public:
  // start inicializa una sesion canonica; desde ese momento solo se agregan
  // eventos (reps, descansos, cierre de sets) hasta finish().
  void reset();
  void start(const String& sessionId,
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
             const String& startedAtIso);
  void recordRep(const RepMetrics& rep, uint8_t setNumber, uint8_t repNumberInSet, float selectedWeightKg,
                 uint32_t nowMs);
  void completeSet(uint8_t setNumber, uint8_t targetRepsUsed, uint16_t plannedRestSeconds,
                   uint32_t nowMs);
  void beginRest(uint8_t setNumber, uint32_t nowMs);
  void endRest(uint32_t nowMs);
  void finish(uint32_t endMs, uint32_t endedAtEpoch, const String& endedAtIso);
  bool isActive() const;
  const SessionHistoryRecord& getRecord() const;
  void loadRecord(const SessionHistoryRecord& record);
  String toJson() const;
  // JSON principal para Firebase con estructura mas amigable para frontend.
  String toAthleteAnalysisJson() const;
  // JSON derivado por set para tablas y comparativas set-a-set.
  String toSetDetailsJson() const;
  // JSON ligero con todas las reps de un set concreto.
  String toRepSetJson(uint8_t setNumber) const;

 private:
  struct SetAccumulator {
    float romSum = 0.0f;
    float concentricSum = 0.0f;
    float peakVelSum = 0.0f;
    float peakEccSum = 0.0f;
  };

  bool active_ = false;
  SessionHistoryRecord record_;
  SetAccumulator setAccumulators_[SessionHistoryRecord::kMaxSets];
  float totalRomSum_ = 0.0f;
  float totalConcentricSum_ = 0.0f;
  float totalPeakVelocitySum_ = 0.0f;
  float totalPeakEccentricSum_ = 0.0f;
  bool restActive_ = false;
  uint8_t restSetNumber_ = 0;
  uint32_t restStartMs_ = 0;

  SetHistoryRecord* getOrCreateSet(uint8_t setNumber);
  static String escapeJson(const String& input);
};
