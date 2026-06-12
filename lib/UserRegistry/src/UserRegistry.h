#pragma once

#include <Arduino.h>

// Modelo local del atleta. Firebase puede ser la fuente de verdad, pero este
// registro permite cache y operacion offline limitada dentro del ESP32.
// Objetivo principal del usuario. Este valor influye en la recomendacion de
// sets, reps, descanso y ajuste de peso segun la maquina activa.
enum class TrainingGoal : uint8_t {
  General = 0,
  Strength,
  Hypertrophy,
  Endurance,
  Test
};

enum class UserGender : uint8_t {
  Unspecified = 0,
  Female,
  Male,
  NonBinary
};

enum class CalibrationConfidence : uint8_t {
  Low = 0,
  Medium,
  High
};

enum class CalibrationAction : uint8_t {
  Keep = 0,
  Increase,
  Decrease,
  RepeatCalibration
};

enum class CalibrationSetClassification : uint8_t {
  Unknown = 0,
  VeryLight,
  Light,
  Moderate,
  HeavyButUsable,
  TooHeavyOrInvalid,
  NoisyInvalid
};

struct CalibrationSetSnapshot {
  uint8_t setIndex = 0;
  float selectedWeightKg = 0.0f;
  uint8_t targetRepCount = 0;
  uint8_t validRepCount = 0;
  uint8_t rejectedRepCount = 0;
  float avgConcentricVelocityPctPerSec = 0.0f;
  float medianConcentricVelocityPctPerSec = 0.0f;
  float avgConcentricDurationMs = 0.0f;
  float avgRomPercent = 0.0f;
  float velocityStdDevPctPerSec = 0.0f;
  float qualityScore = 0.0f;
  CalibrationSetClassification classification = CalibrationSetClassification::Unknown;
  float suggestedNextWeightKg = 0.0f;
  String reasonText;
};

struct MotionTargetsSnapshot {
  uint8_t targetRepsMin = 0;
  uint8_t targetRepsMax = 0;
  uint8_t targetSetsMin = 0;
  uint8_t targetSetsMax = 0;
  uint8_t targetSetsDefault = 0;
  uint16_t restSecondsDefault = 0;
  float riseTimeSecMin = 0.0f;
  float riseTimeSecMax = 0.0f;
  float riseTimeSecDefault = 0.0f;
  float lowerTimeSecMin = 0.0f;
  float lowerTimeSecMax = 0.0f;
  float lowerTimeSecDefault = 0.0f;
  float topPauseSec = 0.0f;
  float bottomPauseSec = 0.0f;
};

// Resultado de calibracion para un tipo de maquina especifico. Se separa por
// machineTypeId para que dos maquinas equivalentes compartan datos del usuario.
struct UserMachineCalibration {
  static constexpr uint8_t kMaxCalibrationSets = 3;
  String machineTypeId;
  bool hasCalibration = false;
  float suggestedWeightKg = 0.0f;
  float userRomPercent = 0.0f;
  float userBottomPct = 0.0f;
  float userTopPct = 0.0f;
  uint32_t updatedAtEpoch = 0;
  String updatedAtIso;

  uint16_t schemaVersion = 1;
  String source;
  String userUid;
  String goalId;
  float romBottomRaw = 0.0f;
  float romTopRaw = 0.0f;
  float romRangeRaw = 0.0f;
  float romRangePct = 0.0f;
  float romMeters = 0.0f;
  bool romValid = false;
  float machineIncrementKg = 5.0f;
  float machineMinKg = 5.0f;
  float machineMaxKg = 100.0f;
  String startWeightSource;
  float suggestedStartWeightKg = 0.0f;
  float actualFirstSetWeightKg = 0.0f;
  bool userOverrodeStartWeight = false;
  CalibrationSetSnapshot calibrationSets[kMaxCalibrationSets];
  uint8_t calibrationSetCount = 0;
  float resultRecommendedWeightKg = 0.0f;
  CalibrationConfidence resultConfidence = CalibrationConfidence::Low;
  CalibrationAction resultAction = CalibrationAction::Keep;
  String resultReasonText;
  float estimatedOneRepMaxKg = 0.0f;
  String estimatedOneRepMaxConfidence;
  float velocityLoadSlope = 0.0f;
  float velocityLoadIntercept = 0.0f;
  bool calibrationModelValid = false;
  float strengthRecommendedKg = 0.0f;
  float hypertrophyRecommendedKg = 0.0f;
  float enduranceRecommendedKg = 0.0f;
  float activeGoalRecommendedKg = 0.0f;
  String activeGoalId;
  float nextRecommendedWeightKg = 0.0f;
  String nextRecommendationSource;
  String nextRecommendationReason;
  uint32_t nextRecommendationUpdatedAt = 0;
  MotionTargetsSnapshot motionTargetsUsed;
};

// Perfil local del usuario identificado por RFID.
// En el MVP se guarda en RAM; despues se puede persistir en NVS o Firebase.
struct UserProfile {
  static constexpr uint8_t kMaxMachineCalibrations = 8;

  String rfidUid;
  String displayName;
  bool hasBasicData = false;
  float weightKg = 0.0f;
  uint8_t age = 0;
  float heightCm = 0.0f;
  UserGender gender = UserGender::Unspecified;
  TrainingGoal goal = TrainingGoal::Hypertrophy;
  uint32_t updatedAtEpoch = 0;
  String updatedAtIso;
  UserMachineCalibration machineCalibrations[kMaxMachineCalibrations];
  uint8_t machineCalibrationCount = 0;
};

void resetUserMachineCalibration(UserMachineCalibration& calibration);
void resetUserProfile(UserProfile& profile);

// Registro local de usuarios. AppController lo usa para resolver una tarjeta
// RFID a un perfil, cargar datos basicos y buscar calibraciones previas.
class UserRegistry {
 public:
  static constexpr uint8_t kMaxProfiles = 8;

  // Carga usuarios demo iniciales.
  void begin();
  void clear();
  // Busca un usuario existente por el UID de su tarjeta.
  UserProfile* findByUid(const String& uid);
  bool upsertProfile(const UserProfile& profile);
  uint8_t getProfileCount() const;
  const UserProfile* getProfileAt(uint8_t index) const;
  // Crea o actualiza datos basicos del usuario identificado.
  UserProfile* upsertBasicData(const String& uid, const String& displayName, float weightKg,
                               uint8_t age, float heightCm,
                               TrainingGoal goal = TrainingGoal::Hypertrophy);
  // Busca la calibracion correspondiente al tipo de maquina actual.
  UserMachineCalibration* findCalibration(UserProfile& profile, const String& machineTypeId);
  // Crea o actualiza una calibracion para un tipo de maquina.
  UserMachineCalibration* upsertCalibration(UserProfile& profile, const String& machineTypeId,
                                            float suggestedWeightKg, float userRomPercent,
                                            float userBottomPct = 0.0f,
                                            float userTopPct = 0.0f);
  // Helpers para imprimir/parsear el objetivo desde la consola serial.
  static const char* goalToString(TrainingGoal goal);
  static bool parseGoal(const String& rawGoal, TrainingGoal& goal);
  static const char* genderToString(UserGender gender);
  static bool parseGender(const String& rawGender, UserGender& gender);
  static const char* calibrationConfidenceToString(CalibrationConfidence confidence);
  static CalibrationConfidence calibrationConfidenceFromString(const String& value);
  static const char* calibrationActionToString(CalibrationAction action);
  static CalibrationAction calibrationActionFromString(const String& value);
  static const char* calibrationSetClassificationToString(CalibrationSetClassification classification);
  static CalibrationSetClassification calibrationSetClassificationFromString(const String& value);

 private:
  UserProfile profiles_[kMaxProfiles];
  uint8_t profileCount_ = 0;

  UserProfile* appendProfile();
};
