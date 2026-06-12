#include "UserRegistry.h"

namespace {
// Seeds de demo para arrancar el MVP incluso sin cloud.
struct CalibrationSeed {
  const char* machineTypeId;
  float suggestedWeightKg;
  float userRomPercent;
};

struct UserSeed {
  const char* uid;
  const char* displayName;
  bool hasBasicData;
  float weightKg;
  uint8_t age;
  float heightCm;
  TrainingGoal goal;
  CalibrationSeed calibrations[UserProfile::kMaxMachineCalibrations];
  uint8_t calibrationCount;
};

// Usuarios demo centralizados. Asi es facil cambiar tarjetas de prueba,
// objetivos o calibraciones sin tocar la logica del registro.
constexpr UserSeed kUserSeeds[] = {
    {"D6-FA-A5-05",
     "Usuario1",
     true,
     78.0f,
     22,
     175.0f,
     TrainingGoal::Hypertrophy,
     {{"leg_extension", 22.0f, 91.0f}},
     1},
    {"7E-BA-1E-06",
     "Usuario2",
     true,
     72.0f,
     24,
     172.0f,
     TrainingGoal::Hypertrophy,
     {{"lat_pulldown", 25.0f, 90.0f}},
     1},
};
constexpr size_t kUserSeedCount = sizeof(kUserSeeds) / sizeof(kUserSeeds[0]);
}

void resetUserMachineCalibration(UserMachineCalibration& calibration) {
  calibration.machineTypeId.remove(0);
  calibration.hasCalibration = false;
  calibration.suggestedWeightKg = 0.0f;
  calibration.userRomPercent = 0.0f;
  calibration.userBottomPct = 0.0f;
  calibration.userTopPct = 0.0f;
  calibration.updatedAtEpoch = 0;
  calibration.updatedAtIso.remove(0);
  calibration.schemaVersion = 1;
  calibration.source.remove(0);
  calibration.userUid.remove(0);
  calibration.goalId.remove(0);
  calibration.romBottomRaw = 0.0f;
  calibration.romTopRaw = 0.0f;
  calibration.romRangeRaw = 0.0f;
  calibration.romRangePct = 0.0f;
  calibration.romMeters = 0.0f;
  calibration.romValid = false;
  calibration.machineIncrementKg = 5.0f;
  calibration.machineMinKg = 5.0f;
  calibration.machineMaxKg = 100.0f;
  calibration.startWeightSource.remove(0);
  calibration.suggestedStartWeightKg = 0.0f;
  calibration.actualFirstSetWeightKg = 0.0f;
  calibration.userOverrodeStartWeight = false;
  calibration.calibrationSetCount = 0;
  for (uint8_t i = 0; i < UserMachineCalibration::kMaxCalibrationSets; ++i) {
    CalibrationSetSnapshot& set = calibration.calibrationSets[i];
    set.setIndex = 0;
    set.selectedWeightKg = 0.0f;
    set.targetRepCount = 0;
    set.validRepCount = 0;
    set.rejectedRepCount = 0;
    set.avgConcentricVelocityPctPerSec = 0.0f;
    set.medianConcentricVelocityPctPerSec = 0.0f;
    set.avgConcentricDurationMs = 0.0f;
    set.avgRomPercent = 0.0f;
    set.velocityStdDevPctPerSec = 0.0f;
    set.qualityScore = 0.0f;
    set.classification = CalibrationSetClassification::Unknown;
    set.suggestedNextWeightKg = 0.0f;
    set.reasonText.remove(0);
  }
  calibration.resultRecommendedWeightKg = 0.0f;
  calibration.resultConfidence = CalibrationConfidence::Low;
  calibration.resultAction = CalibrationAction::Keep;
  calibration.resultReasonText.remove(0);
  calibration.estimatedOneRepMaxKg = 0.0f;
  calibration.estimatedOneRepMaxConfidence.remove(0);
  calibration.velocityLoadSlope = 0.0f;
  calibration.velocityLoadIntercept = 0.0f;
  calibration.calibrationModelValid = false;
  calibration.strengthRecommendedKg = 0.0f;
  calibration.hypertrophyRecommendedKg = 0.0f;
  calibration.enduranceRecommendedKg = 0.0f;
  calibration.activeGoalRecommendedKg = 0.0f;
  calibration.activeGoalId.remove(0);
  calibration.nextRecommendedWeightKg = 0.0f;
  calibration.nextRecommendationSource.remove(0);
  calibration.nextRecommendationReason.remove(0);
  calibration.nextRecommendationUpdatedAt = 0;
  calibration.motionTargetsUsed = MotionTargetsSnapshot{};
}

void resetUserProfile(UserProfile& profile) {
  profile.rfidUid.remove(0);
  profile.displayName.remove(0);
  profile.hasBasicData = false;
  profile.weightKg = 0.0f;
  profile.age = 0;
  profile.heightCm = 0.0f;
  profile.gender = UserGender::Unspecified;
  profile.goal = TrainingGoal::Hypertrophy;
  profile.updatedAtEpoch = 0;
  profile.updatedAtIso.remove(0);
  profile.machineCalibrationCount = 0;
  for (uint8_t i = 0; i < UserProfile::kMaxMachineCalibrations; ++i) {
    resetUserMachineCalibration(profile.machineCalibrations[i]);
  }
}

void UserRegistry::begin() {
  // Para el MVP arrancamos con un registro local en RAM y dos tarjetas demo.
  // Mas adelante esto se puede reemplazar por NVS o Firebase sin cambiar el
  // resto del flujo de AppController.
  profileCount_ = 0;

  for (size_t i = 0; i < kUserSeedCount; ++i) {
    const UserSeed& seed = kUserSeeds[i];

    UserProfile* profile = appendProfile();
    if (profile == nullptr) {
      return;
    }

    profile->rfidUid = seed.uid;
    profile->displayName = seed.displayName;
    profile->goal = seed.goal;

    if (seed.hasBasicData) {
      profile->hasBasicData = true;
      profile->weightKg = seed.weightKg;
      profile->age = seed.age;
      profile->heightCm = seed.heightCm;
    }

    for (uint8_t j = 0; j < seed.calibrationCount; ++j) {
      upsertCalibration(*profile,
                        seed.calibrations[j].machineTypeId,
                        seed.calibrations[j].suggestedWeightKg,
                        seed.calibrations[j].userRomPercent);
    }
  }
}

void UserRegistry::clear() {
  profileCount_ = 0;
}

UserProfile* UserRegistry::findByUid(const String& uid) {
  for (uint8_t i = 0; i < profileCount_; ++i) {
    if (profiles_[i].rfidUid.equalsIgnoreCase(uid)) {
      return &profiles_[i];
    }
  }

  return nullptr;
}

bool UserRegistry::upsertProfile(const UserProfile& profile) {
  UserProfile* slot = findByUid(profile.rfidUid);
  if (slot == nullptr) {
    slot = appendProfile();
  }

  if (slot == nullptr) {
    return false;
  }

  *slot = profile;
  return true;
}

uint8_t UserRegistry::getProfileCount() const {
  return profileCount_;
}

const UserProfile* UserRegistry::getProfileAt(uint8_t index) const {
  if (index >= profileCount_) {
    return nullptr;
  }

  return &profiles_[index];
}

UserProfile* UserRegistry::upsertBasicData(const String& uid, const String& displayName,
                                           float weightKg, uint8_t age, float heightCm,
                                           TrainingGoal goal) {
  // Si el UID ya existe, actualizamos el perfil. Si no existe, lo creamos.
  UserProfile* profile = findByUid(uid);
  if (profile == nullptr) {
    profile = appendProfile();
  }

  if (profile == nullptr) {
    return nullptr;
  }

  profile->rfidUid = uid;
  profile->displayName = displayName;
  profile->hasBasicData = true;
  profile->weightKg = weightKg;
  profile->age = age;
  profile->heightCm = heightCm;
  profile->gender = UserGender::Unspecified;
  profile->goal = goal;
  profile->updatedAtEpoch = 0;
  profile->updatedAtIso = "";
  // Cuando el usuario cambia sus datos basicos reiniciamos las calibraciones
  // cargadas en RAM para evitar mezclar recomendaciones viejas con un perfil
  // actualizado.
  profile->machineCalibrationCount = 0;
  return profile;
}

UserMachineCalibration* UserRegistry::findCalibration(UserProfile& profile,
                                                      const String& machineTypeId) {
  for (uint8_t i = 0; i < profile.machineCalibrationCount; ++i) {
    if (profile.machineCalibrations[i].machineTypeId.equalsIgnoreCase(machineTypeId)) {
      return &profile.machineCalibrations[i];
    }
  }

  return nullptr;
}

UserMachineCalibration* UserRegistry::upsertCalibration(UserProfile& profile,
                                                        const String& machineTypeId,
                                                        float suggestedWeightKg,
                                                        float userRomPercent,
                                                        float userBottomPct,
                                                        float userTopPct) {
  // La calibracion vive por tipo de maquina. Ejemplo: dos chest press
  // comparten la misma recomendacion, pero una squat requeriria otra.
  UserMachineCalibration* calibration = findCalibration(profile, machineTypeId);
  if (calibration == nullptr) {
    if (profile.machineCalibrationCount >= UserProfile::kMaxMachineCalibrations) {
      return nullptr;
    }

    calibration = &profile.machineCalibrations[profile.machineCalibrationCount];
    resetUserMachineCalibration(*calibration);
    profile.machineCalibrationCount++;
  }

  calibration->machineTypeId = machineTypeId;
  calibration->hasCalibration = true;
  calibration->suggestedWeightKg = suggestedWeightKg;
  calibration->userRomPercent = userRomPercent;
  calibration->userBottomPct = constrain(userBottomPct, 0.0f, 95.0f);
  const float fallbackTop = calibration->userBottomPct + max(5.0f, userRomPercent);
  calibration->userTopPct = constrain(userTopPct > calibration->userBottomPct
                                          ? userTopPct
                                          : fallbackTop,
                                      calibration->userBottomPct + 5.0f,
                                      100.0f);
  calibration->updatedAtEpoch = 0;
  calibration->updatedAtIso = "";
  calibration->schemaVersion = 1;
  calibration->source = "legacy_calibration";
  calibration->goalId = "hypertrophy";
  calibration->romBottomRaw = calibration->userBottomPct;
  calibration->romTopRaw = calibration->userTopPct;
  calibration->romRangeRaw = calibration->userTopPct - calibration->userBottomPct;
  calibration->romRangePct = calibration->userRomPercent;
  calibration->romValid = calibration->userRomPercent > 0.0f;
  calibration->resultRecommendedWeightKg = calibration->suggestedWeightKg;
  calibration->nextRecommendedWeightKg = calibration->suggestedWeightKg;
  calibration->nextRecommendationSource = "calibration";
  calibration->nextRecommendationReason = "Saved calibration recommendation.";
  calibration->nextRecommendationUpdatedAt = 0;
  return calibration;
}

UserProfile* UserRegistry::appendProfile() {
  if (profileCount_ >= kMaxProfiles) {
    return nullptr;
  }

  // El registro usa almacenamiento fijo para no fragmentar memoria en el ESP32
  // mientras siguen creciendo las responsabilidades del firmware.
  UserProfile* profile = &profiles_[profileCount_];
  resetUserProfile(*profile);
  profileCount_++;
  return profile;
}

const char* UserRegistry::goalToString(TrainingGoal goal) {
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

bool UserRegistry::parseGoal(const String& rawGoal, TrainingGoal& goal) {
  // Aceptamos alias en espanol e ingles para que la consola del MVP sea mas
  // tolerante durante pruebas.
  if (rawGoal.equalsIgnoreCase("strength") || rawGoal.equalsIgnoreCase("fuerza")) {
    goal = TrainingGoal::Strength;
    return true;
  }

  if (rawGoal.equalsIgnoreCase("hypertrophy") || rawGoal.equalsIgnoreCase("hipertrofia") ||
      rawGoal.equalsIgnoreCase("volume") || rawGoal.equalsIgnoreCase("volumen")) {
    goal = TrainingGoal::Hypertrophy;
    return true;
  }

  if (rawGoal.equalsIgnoreCase("endurance") || rawGoal.equalsIgnoreCase("condicion") ||
      rawGoal.equalsIgnoreCase("resistencia")) {
    goal = TrainingGoal::Endurance;
    return true;
  }

  if (rawGoal.equalsIgnoreCase("test") || rawGoal.equalsIgnoreCase("prueba") ||
      rawGoal.equalsIgnoreCase("quick") || rawGoal.equalsIgnoreCase("rapido")) {
    goal = TrainingGoal::Test;
    return true;
  }

  if (rawGoal.equalsIgnoreCase("general")) {
    goal = TrainingGoal::General;
    return true;
  }

  return false;
}

const char* UserRegistry::genderToString(UserGender gender) {
  switch (gender) {
    case UserGender::Female:
      return "female";
    case UserGender::Male:
      return "male";
    case UserGender::NonBinary:
      return "non_binary";
    case UserGender::Unspecified:
    default:
      return "unspecified";
  }
}

bool UserRegistry::parseGender(const String& rawGender, UserGender& gender) {
  if (rawGender.equalsIgnoreCase("female") || rawGender.equalsIgnoreCase("femenino")) {
    gender = UserGender::Female;
    return true;
  }
  if (rawGender.equalsIgnoreCase("male") || rawGender.equalsIgnoreCase("masculino")) {
    gender = UserGender::Male;
    return true;
  }
  if (rawGender.equalsIgnoreCase("non_binary") || rawGender.equalsIgnoreCase("nonbinary")) {
    gender = UserGender::NonBinary;
    return true;
  }
  if (rawGender.equalsIgnoreCase("unspecified") || rawGender.equalsIgnoreCase("otro")) {
    gender = UserGender::Unspecified;
    return true;
  }
  return false;
}

const char* UserRegistry::calibrationConfidenceToString(CalibrationConfidence confidence) {
  switch (confidence) {
    case CalibrationConfidence::High:
      return "high";
    case CalibrationConfidence::Medium:
      return "medium";
    case CalibrationConfidence::Low:
    default:
      return "low";
  }
}

CalibrationConfidence UserRegistry::calibrationConfidenceFromString(const String& value) {
  if (value.equalsIgnoreCase("high")) {
    return CalibrationConfidence::High;
  }
  if (value.equalsIgnoreCase("medium")) {
    return CalibrationConfidence::Medium;
  }
  return CalibrationConfidence::Low;
}

const char* UserRegistry::calibrationActionToString(CalibrationAction action) {
  switch (action) {
    case CalibrationAction::Increase:
      return "increase";
    case CalibrationAction::Decrease:
      return "decrease";
    case CalibrationAction::RepeatCalibration:
      return "repeat_calibration";
    case CalibrationAction::Keep:
    default:
      return "keep";
  }
}

CalibrationAction UserRegistry::calibrationActionFromString(const String& value) {
  if (value.equalsIgnoreCase("increase")) {
    return CalibrationAction::Increase;
  }
  if (value.equalsIgnoreCase("decrease")) {
    return CalibrationAction::Decrease;
  }
  if (value.equalsIgnoreCase("repeat_calibration")) {
    return CalibrationAction::RepeatCalibration;
  }
  return CalibrationAction::Keep;
}

const char* UserRegistry::calibrationSetClassificationToString(
    CalibrationSetClassification classification) {
  switch (classification) {
    case CalibrationSetClassification::VeryLight:
      return "very_light";
    case CalibrationSetClassification::Light:
      return "light";
    case CalibrationSetClassification::Moderate:
      return "moderate";
    case CalibrationSetClassification::HeavyButUsable:
      return "heavy_but_usable";
    case CalibrationSetClassification::TooHeavyOrInvalid:
      return "too_heavy_or_invalid";
    case CalibrationSetClassification::NoisyInvalid:
      return "noisy_invalid";
    case CalibrationSetClassification::Unknown:
    default:
      return "unknown";
  }
}

CalibrationSetClassification UserRegistry::calibrationSetClassificationFromString(
    const String& value) {
  if (value.equalsIgnoreCase("very_light")) {
    return CalibrationSetClassification::VeryLight;
  }
  if (value.equalsIgnoreCase("light")) {
    return CalibrationSetClassification::Light;
  }
  if (value.equalsIgnoreCase("moderate")) {
    return CalibrationSetClassification::Moderate;
  }
  if (value.equalsIgnoreCase("heavy_but_usable")) {
    return CalibrationSetClassification::HeavyButUsable;
  }
  if (value.equalsIgnoreCase("too_heavy_or_invalid")) {
    return CalibrationSetClassification::TooHeavyOrInvalid;
  }
  if (value.equalsIgnoreCase("noisy_invalid")) {
    return CalibrationSetClassification::NoisyInvalid;
  }
  return CalibrationSetClassification::Unknown;
}
