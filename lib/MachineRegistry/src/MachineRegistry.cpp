#include "MachineRegistry.h"
#include <cstring>

namespace {

struct GoalTemplate {
  const char* goalId;
  uint8_t targetRepsMin;
  uint8_t targetRepsMax;
  uint8_t targetSetsMin;
  uint8_t targetSetsMax;
  uint8_t targetSetsDefault;
  uint16_t restSecondsDefault;
  float riseTimeSecMin;
  float riseTimeSecMax;
  float riseTimeSecDefault;
  float lowerTimeSecMin;
  float lowerTimeSecMax;
  float lowerTimeSecDefault;
};

struct MachineSeed {
  const char* machineId;
  const char* machineTypeId;
  const char* displayName;
  const char* machineCategory;
  const char* muscleGroup;
  const char* subMuscle;
  float strokeLengthMm;
  float idealRomPercent;
  float machineIncrementKg;
  float machineMinKg;
  float machineMaxKg;
  float defaultSafeCalibrationKg;
};

constexpr GoalTemplate kHypertrophyTemplate = {
    "hypertrophy", 8, 12, 3, 4, 3, 90, 1.0f, 1.6f, 1.2f, 2.0f, 3.0f, 2.5f};
constexpr GoalTemplate kStrengthTemplate = {
    "strength", 4, 6, 3, 5, 3, 180, 0.8f, 1.8f, 1.2f, 2.0f, 3.5f, 2.8f};
constexpr GoalTemplate kEnduranceTemplate = {
    "endurance", 12, 15, 2, 3, 2, 45, 0.7f, 1.2f, 0.9f, 1.0f, 2.0f, 1.4f};
constexpr GoalTemplate kTestTemplate = {
    "test", 3, 5, 1, 2, 1, 30, 0.8f, 1.6f, 1.2f, 1.2f, 2.5f, 1.8f};

constexpr MachineSeed kMachineSeeds[] = {
    {"incline_press_1", "incline_press", "Incline Press", "compound_press", "chest", "upper chest", 430.0f, 90.0f, 5.0f, 5.0f, 100.0f, 20.0f},
    {"flat_bench_press_1", "flat_bench_press", "Flat Bench Press", "compound_press", "chest", "middle chest", 430.0f, 90.0f, 5.0f, 5.0f, 100.0f, 20.0f},
    {"cable_fly_1", "cable_fly", "Cable Fly", "isolation_squeeze", "chest", "lower chest", 500.0f, 88.0f, 5.0f, 5.0f, 100.0f, 20.0f},
    {"incline_cable_curl_1", "incline_cable_curl", "Incline Cable Curl", "isolation_squeeze", "biceps", "long head", 380.0f, 88.0f, 5.0f, 5.0f, 100.0f, 20.0f},
    {"preacher_curl_1", "preacher_curl", "Preacher Curl", "isolation_squeeze", "biceps", "short head", 350.0f, 88.0f, 5.0f, 5.0f, 100.0f, 20.0f},
    {"triceps_pushdown_1", "triceps_pushdown", "Triceps Pushdown", "isolation_squeeze", "triceps", "lateral head", 360.0f, 90.0f, 5.0f, 5.0f, 100.0f, 20.0f},
    {"overhead_triceps_extension_1", "overhead_triceps_extension", "Overhead Triceps Extension", "isolation_squeeze", "triceps", "long head", 400.0f, 90.0f, 5.0f, 5.0f, 100.0f, 20.0f},
    {"lat_pulldown_1", "lat_pulldown", "Lat Pulldown", "pulling", "back", "lats", 650.0f, 88.0f, 5.0f, 5.0f, 100.0f, 20.0f},
    {"seated_cable_row_1", "seated_cable_row", "Seated Cable Row", "pulling", "back", "trapezius / mid back", 620.0f, 88.0f, 5.0f, 5.0f, 100.0f, 20.0f},
    {"shoulder_press_1", "shoulder_press", "Shoulder Press", "compound_press", "shoulder", "anterior deltoid", 420.0f, 88.0f, 5.0f, 5.0f, 100.0f, 20.0f},
    {"cable_lateral_raise_1", "cable_lateral_raise", "Cable Lateral Raise", "isolation_squeeze", "shoulder", "lateral deltoid", 350.0f, 85.0f, 5.0f, 5.0f, 100.0f, 20.0f},
    {"face_pull_1", "face_pull", "Face Pull", "pulling", "shoulder", "posterior deltoid", 380.0f, 85.0f, 5.0f, 5.0f, 100.0f, 20.0f},
    {"leg_press_1", "leg_press", "Leg Press", "compound_press", "legs", "glutes / quadriceps", 700.0f, 90.0f, 5.0f, 5.0f, 100.0f, 20.0f},
    {"calf_raise_1", "calf_raise", "Calf Raise", "calf_raise", "legs", "calves", 300.0f, 92.0f, 5.0f, 5.0f, 100.0f, 20.0f},
    {"leg_extension_1", "leg_extension", "Leg Extension", "isolation_squeeze", "legs", "quadriceps", 420.0f, 90.0f, 5.0f, 5.0f, 100.0f, 20.0f},
    {"seated_leg_curl_1", "seated_leg_curl", "Seated Leg Curl", "isolation_squeeze", "legs", "hamstrings", 420.0f, 90.0f, 5.0f, 5.0f, 100.0f, 20.0f},
    {"hip_adductor_1", "hip_adductor", "Hip Adductor", "isolation_squeeze", "legs", "adductors", 350.0f, 85.0f, 5.0f, 5.0f, 100.0f, 20.0f},
    {"hip_abductor_1", "hip_abductor", "Hip Abductor", "isolation_squeeze", "legs", "abductors", 350.0f, 85.0f, 5.0f, 5.0f, 100.0f, 20.0f},
};

MachineProfile kMachines[sizeof(kMachineSeeds) / sizeof(kMachineSeeds[0])];
constexpr size_t kMachineCount = sizeof(kMachines) / sizeof(kMachines[0]);

bool equalsLiteral(const char* value, const char* expected) {
  return value != nullptr && expected != nullptr && strcmp(value, expected) == 0;
}

uint16_t restOverride(const char* machineTypeId, const char* goalId, uint16_t fallback) {
  const bool isHypertrophy = equalsLiteral(goalId, "hypertrophy");
  const bool isStrength = equalsLiteral(goalId, "strength");
  if (equalsLiteral(machineTypeId, "incline_press")) return isHypertrophy ? 120 : (isStrength ? 180 : 60);
  if (equalsLiteral(machineTypeId, "flat_bench_press")) return isHypertrophy ? 150 : (isStrength ? 210 : 90);
  if (equalsLiteral(machineTypeId, "cable_fly")) return isHypertrophy ? 90 : (isStrength ? 120 : 40);
  if (equalsLiteral(machineTypeId, "incline_cable_curl")) return isHypertrophy ? 90 : (isStrength ? 150 : 45);
  if (equalsLiteral(machineTypeId, "preacher_curl")) return isHypertrophy ? 90 : (isStrength ? 150 : 45);
  if (equalsLiteral(machineTypeId, "triceps_pushdown")) return isHypertrophy ? 90 : (isStrength ? 150 : 45);
  if (equalsLiteral(machineTypeId, "overhead_triceps_extension")) return isHypertrophy ? 60 : (isStrength ? 150 : 45);
  if (equalsLiteral(machineTypeId, "lat_pulldown")) return isHypertrophy ? 120 : (isStrength ? 180 : 60);
  if (equalsLiteral(machineTypeId, "seated_cable_row")) return isHypertrophy ? 120 : (isStrength ? 180 : 60);
  if (equalsLiteral(machineTypeId, "shoulder_press")) return isHypertrophy ? 120 : (isStrength ? 180 : 60);
  if (equalsLiteral(machineTypeId, "cable_lateral_raise")) return isHypertrophy ? 60 : (isStrength ? 150 : 45);
  if (equalsLiteral(machineTypeId, "face_pull")) return isHypertrophy ? 60 : (isStrength ? 120 : 40);
  if (equalsLiteral(machineTypeId, "leg_press")) return isHypertrophy ? 90 : (isStrength ? 120 : 40);
  if (equalsLiteral(machineTypeId, "calf_raise")) return isHypertrophy ? 60 : (isStrength ? 180 : 60);
  if (equalsLiteral(machineTypeId, "leg_extension")) return isHypertrophy ? 90 : (isStrength ? 120 : 40);
  if (equalsLiteral(machineTypeId, "seated_leg_curl")) return isHypertrophy ? 60 : (isStrength ? 120 : 40);
  if (equalsLiteral(machineTypeId, "hip_adductor")) return isHypertrophy ? 60 : (isStrength ? 120 : 40);
  if (equalsLiteral(machineTypeId, "hip_abductor")) return isHypertrophy ? 60 : (isStrength ? 120 : 40);
  return fallback;
}

float topPauseForCategory(const char* category, const char* goalId) {
  const bool isHypertrophy = equalsLiteral(goalId, "hypertrophy");
  const bool isStrength = equalsLiteral(goalId, "strength");
  if (equalsLiteral(category, "compound_press")) return 0.0f;
  if (equalsLiteral(category, "pulling")) return isHypertrophy ? 0.3f : (isStrength ? 0.2f : 0.1f);
  if (equalsLiteral(category, "isolation_squeeze")) return isHypertrophy ? 0.4f : (isStrength ? 0.3f : 0.1f);
  if (equalsLiteral(category, "calf_raise")) return isHypertrophy ? 0.5f : (isStrength ? 0.4f : 0.2f);
  return 0.0f;
}

float bottomPauseForCategory(const char* machineTypeId, const char* category, const char* goalId) {
  const bool isHypertrophy = equalsLiteral(goalId, "hypertrophy");
  const bool isStrength = equalsLiteral(goalId, "strength");
  if (equalsLiteral(machineTypeId, "leg_press") ||
      equalsLiteral(machineTypeId, "leg_extension") ||
      equalsLiteral(machineTypeId, "seated_leg_curl")) {
    return isHypertrophy ? 0.5f : (isStrength ? 1.0f : 0.15f);
  }
  if (equalsLiteral(category, "compound_press") || equalsLiteral(category, "pulling")) {
    return isHypertrophy ? 0.4f : (isStrength ? 1.0f : 0.15f);
  }
  if (equalsLiteral(category, "isolation_squeeze")) {
    return isHypertrophy ? 0.2f : (isStrength ? 0.3f : 0.0f);
  }
  if (equalsLiteral(category, "calf_raise")) {
    return isHypertrophy ? 0.3f : (isStrength ? 0.5f : 0.1f);
  }
  return 0.2f;
}

MotionTargetConfig buildMotionTarget(const MachineSeed& seed, const GoalTemplate& tpl) {
  MotionTargetConfig cfg;
  cfg.goalId = tpl.goalId;
  cfg.debugOnly = equalsLiteral(tpl.goalId, "test");
  cfg.targetRepsMin = tpl.targetRepsMin;
  cfg.targetRepsMax = tpl.targetRepsMax;
  cfg.targetSetsMin = tpl.targetSetsMin;
  cfg.targetSetsMax = tpl.targetSetsMax;
  cfg.targetSetsDefault = tpl.targetSetsDefault;
  cfg.restSecondsDefault = restOverride(seed.machineTypeId, tpl.goalId, tpl.restSecondsDefault);
  cfg.riseTimeSecMin = tpl.riseTimeSecMin;
  cfg.riseTimeSecMax = tpl.riseTimeSecMax;
  cfg.riseTimeSecDefault = tpl.riseTimeSecDefault;
  cfg.lowerTimeSecMin = tpl.lowerTimeSecMin;
  cfg.lowerTimeSecMax = tpl.lowerTimeSecMax;
  cfg.lowerTimeSecDefault = tpl.lowerTimeSecDefault;
  if (equalsLiteral(tpl.goalId, "test")) {
    cfg.topPauseSec = topPauseForCategory(seed.machineCategory, "hypertrophy");
    cfg.bottomPauseSec = 0.2f;
  } else {
    cfg.topPauseSec = topPauseForCategory(seed.machineCategory, tpl.goalId);
    cfg.bottomPauseSec = bottomPauseForCategory(seed.machineTypeId, seed.machineCategory, tpl.goalId);
  }
  return cfg;
}

GoalRecommendation toRecommendation(TrainingGoal goal, const MotionTargetConfig& motion, float weightFactor) {
  return GoalRecommendation(goal,
                            weightFactor,
                            motion.targetRepsMin,
                            motion.targetRepsMax,
                            motion.targetSetsDefault,
                            motion.restSecondsDefault,
                            static_cast<uint16_t>(motion.riseTimeSecDefault * 1000.0f),
                            static_cast<uint16_t>(motion.lowerTimeSecDefault * 1000.0f),
                            static_cast<uint16_t>(motion.topPauseSec * 1000.0f),
                            static_cast<uint16_t>(motion.bottomPauseSec * 1000.0f));
}

void initRecommendations() {
  static bool initialized = false;
  if (initialized) {
    return;
  }
  initialized = true;

  for (size_t i = 0; i < kMachineCount; ++i) {
    const MachineSeed& seed = kMachineSeeds[i];
    kMachines[i] = MachineProfile{};
    kMachines[i].machineId = seed.machineId;
    kMachines[i].machineTypeId = seed.machineTypeId;
    kMachines[i].displayName = seed.displayName;
    kMachines[i].machineCategory = seed.machineCategory;
    kMachines[i].exerciseCategory = seed.machineCategory;
    kMachines[i].primaryMuscleGroup = seed.muscleGroup;
    kMachines[i].secondaryMuscleGroup = seed.subMuscle;
    kMachines[i].strokeLengthMm = seed.strokeLengthMm;
    kMachines[i].idealRomPercent = seed.idealRomPercent;
    kMachines[i].targetRepsPerSet = static_cast<uint8_t>(kHypertrophyTemplate.targetRepsMax);
    kMachines[i].machineIncrementKg = seed.machineIncrementKg;
    kMachines[i].machineMinKg = seed.machineMinKg;
    kMachines[i].machineMaxKg = seed.machineMaxKg;
    kMachines[i].defaultSafeCalibrationKg = seed.defaultSafeCalibrationKg;
    kMachines[i].defaultCalibrationWeightKg = seed.defaultSafeCalibrationKg;
    kMachines[i].motionTargets[0] = buildMotionTarget(seed, kHypertrophyTemplate);
    kMachines[i].motionTargets[1] = buildMotionTarget(seed, kStrengthTemplate);
    kMachines[i].motionTargets[2] = buildMotionTarget(seed, kEnduranceTemplate);
    kMachines[i].motionTargets[3] = buildMotionTarget(seed, kTestTemplate);

    kMachines[i].recommendations[0] = toRecommendation(TrainingGoal::Strength, kMachines[i].motionTargets[1], 1.0f);
    kMachines[i].recommendations[1] = toRecommendation(TrainingGoal::Hypertrophy, kMachines[i].motionTargets[0], 1.0f);
    kMachines[i].recommendations[2] = toRecommendation(TrainingGoal::Endurance, kMachines[i].motionTargets[2], 0.95f);
    kMachines[i].recommendations[3] = toRecommendation(TrainingGoal::General, kMachines[i].motionTargets[0], 1.0f);
    kMachines[i].recommendations[4] = toRecommendation(TrainingGoal::Test, kMachines[i].motionTargets[3], 1.0f);
  }
}

}  // namespace

const MachineProfile* MachineRegistry::getDefault() const {
  initRecommendations();
  return &kMachines[0];
}

const MachineProfile* MachineRegistry::findById(const String& machineId) const {
  initRecommendations();
  for (size_t i = 0; i < kMachineCount; ++i) {
    if (machineId.equalsIgnoreCase(kMachines[i].machineId)) {
      return &kMachines[i];
    }
  }
  return nullptr;
}

const GoalRecommendation* MachineRegistry::findGoalRecommendation(
    const MachineProfile& machineProfile, TrainingGoal goal) const {
  for (const GoalRecommendation& recommendation : machineProfile.recommendations) {
    if (recommendation.goal == goal) {
      return &recommendation;
    }
  }
  return nullptr;
}

bool MachineRegistry::getMotionTargetsForMachineGoal(const String& machineTypeId,
                                                      const String& goalId,
                                                      MotionTargetConfig& outConfig) const {
  initRecommendations();
  for (size_t i = 0; i < kMachineCount; ++i) {
    if (!machineTypeId.equalsIgnoreCase(kMachines[i].machineTypeId)) {
      continue;
    }
    for (const MotionTargetConfig& cfg : kMachines[i].motionTargets) {
      if (goalId.equalsIgnoreCase(cfg.goalId != nullptr ? cfg.goalId : "")) {
        outConfig = cfg;
        return true;
      }
    }
    outConfig = kMachines[i].motionTargets[0];
    return true;
  }
  return false;
}

const char* MachineRegistry::trainingGoalToGoalId(TrainingGoal goal) {
  switch (goal) {
    case TrainingGoal::Strength:
      return "strength";
    case TrainingGoal::Endurance:
      return "endurance";
    case TrainingGoal::Hypertrophy:
      return "hypertrophy";
    case TrainingGoal::Test:
      return "test";
    case TrainingGoal::General:
    default:
      return "hypertrophy";
  }
}

bool MachineRegistry::applyCloudConfig(const MachineCloudConfig& cloudConfig) {
  initRecommendations();
  if (!cloudConfig.valid) {
    return false;
  }

  for (size_t i = 0; i < kMachineCount; ++i) {
    if (!cloudConfig.machineId.equalsIgnoreCase(kMachines[i].machineId)) {
      continue;
    }

    kMachines[i].strokeLengthMm = cloudConfig.strokeLengthMm;
    kMachines[i].idealRomPercent = cloudConfig.idealRomPercent;
    kMachines[i].defaultCalibrationWeightKg = cloudConfig.defaultCalibrationWeightKg;
    kMachines[i].targetRepsPerSet = cloudConfig.targetRepsPerSet;
    kMachines[i].cloudVersion = cloudConfig.version;
    kMachines[i].cloudUpdatedAtEpoch = cloudConfig.updatedAtEpoch;
    for (size_t j = 0; j < 5; ++j) {
      kMachines[i].recommendations[j] = cloudConfig.recommendations[j];
    }
    return true;
  }

  return false;
}
