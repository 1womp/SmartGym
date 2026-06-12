#pragma once

#include <Arduino.h>
#include "UserRegistry.h"

// Catalogo local de maquinas compatibles con este firmware. Permite que el
// dispositivo arranque y recomiende planes aun si Firebase no esta disponible.
// Recomendacion derivada del objetivo del usuario para una maquina concreta.
// Se aplica despues de calibracion para convertir un peso base en un plan de
// sets, reps y descanso.
struct GoalRecommendation {
  TrainingGoal goal = TrainingGoal::Hypertrophy;
  float weightFactor = 1.0f;
  uint8_t repsMin = 8;
  uint8_t repsMax = 10;
  uint8_t targetSets = 3;
  uint16_t restSeconds = 60;
  // Motion-guide timing (ms) used to build the ideal graph shape.
  uint16_t riseMs = 1100;
  uint16_t lowerMs = 1200;
  uint16_t topPauseMs = 200;
  uint16_t bottomPauseMs = 180;

  constexpr GoalRecommendation() = default;
  constexpr GoalRecommendation(TrainingGoal trainingGoal, float factor, uint8_t minReps,
                               uint8_t maxReps, uint8_t sets, uint16_t rest,
                               uint16_t rise = 1100, uint16_t lower = 1200,
                               uint16_t topPause = 200, uint16_t bottomPause = 180)
      : goal(trainingGoal),
        weightFactor(factor),
        repsMin(minReps),
        repsMax(maxReps),
        targetSets(sets),
        restSeconds(rest),
        riseMs(rise),
        lowerMs(lower),
        topPauseMs(topPause),
        bottomPauseMs(bottomPause) {}
};

struct MotionTargetConfig {
  const char* goalId = "hypertrophy";
  bool debugOnly = false;
  uint8_t targetRepsMin = 8;
  uint8_t targetRepsMax = 12;
  uint8_t targetSetsMin = 3;
  uint8_t targetSetsMax = 4;
  uint8_t targetSetsDefault = 3;
  uint16_t restSecondsDefault = 90;
  float riseTimeSecMin = 1.0f;
  float riseTimeSecMax = 1.6f;
  float riseTimeSecDefault = 1.2f;
  float lowerTimeSecMin = 2.0f;
  float lowerTimeSecMax = 3.0f;
  float lowerTimeSecDefault = 2.5f;
  float topPauseSec = 0.0f;
  float bottomPauseSec = 0.2f;
};

// Vista mutable de la configuracion remota que puede llegar desde Firebase.
// Usamos el mismo shape base de la maquina local para que AppController solo
// tenga que re-aplicar el perfil cuando cambien estos valores.
struct MachineCloudConfig {
  String machineId;
  String exerciseCategory;
  String primaryMuscleGroup;
  String secondaryMuscleGroup;
  uint32_t version = 0;
  uint32_t updatedAtEpoch = 0;
  float strokeLengthMm = 0.0f;
  float idealRomPercent = 0.0f;
  float defaultCalibrationWeightKg = 0.0f;
  uint8_t targetRepsPerSet = 0;
  bool encoderCalibrationValid = false;
  uint32_t encoderZeroRaw = 0;
  uint32_t encoderFullRaw = 0;
  float encoderReferenceDistanceMm = 1000.0f;
  bool encoderInvertDirection = false;
  GoalRecommendation recommendations[5];
  bool valid = false;
};

// Perfil de una maquina instalada en el gimnasio.
// machineId identifica el equipo fisico; machineTypeId agrupa maquinas
// equivalentes que comparten calibracion y reglas.
struct MachineProfile {
  const char* machineId = "";
  const char* machineTypeId = "";
  const char* displayName = "";
  const char* machineCategory = "";
  const char* exerciseCategory = "";
  const char* primaryMuscleGroup = "";
  const char* secondaryMuscleGroup = "";
  float strokeLengthMm = 500.0f;
  float idealRomPercent = 90.0f;
  float defaultCalibrationWeightKg = 20.0f;
  float machineIncrementKg = 5.0f;
  float machineMinKg = 5.0f;
  float machineMaxKg = 100.0f;
  float defaultSafeCalibrationKg = 20.0f;
  uint8_t targetRepsPerSet = 8;
  bool encoderCalibrationValid = false;
  uint32_t encoderZeroRaw = 0;
  uint32_t encoderFullRaw = 0;
  float encoderReferenceDistanceMm = 1000.0f;
  bool encoderInvertDirection = false;
  GoalRecommendation recommendations[5];
  uint32_t cloudVersion = 0;
  uint32_t cloudUpdatedAtEpoch = 0;
  MotionTargetConfig motionTargets[4];

  MachineProfile() = default;
  MachineProfile(const char* id, const char* typeId, const char* name, const char* category,
                 const char* primaryMuscle, const char* secondaryMuscle, float strokeMm,
                 float idealRom, float defaultWeightKg, uint8_t repsPerSet)
      : machineId(id),
        machineTypeId(typeId),
        displayName(name),
        machineCategory(category),
        exerciseCategory(category),
        primaryMuscleGroup(primaryMuscle),
        secondaryMuscleGroup(secondaryMuscle),
        strokeLengthMm(strokeMm),
        idealRomPercent(idealRom),
        defaultCalibrationWeightKg(defaultWeightKg),
        targetRepsPerSet(repsPerSet) {}
};

// Registro local de maquinas disponibles para el prototipo.
class MachineRegistry {
 public:
  // Devuelve la maquina usada cuando el dispositivo no tiene una configuracion
  // previa en memoria.
  const MachineProfile* getDefault() const;
  // Busca un equipo fisico por su machineId.
  const MachineProfile* findById(const String& machineId) const;
  // Devuelve la recomendacion especifica de esa maquina para el objetivo dado.
  const GoalRecommendation* findGoalRecommendation(const MachineProfile& machineProfile,
                                                   TrainingGoal goal) const;
  bool getMotionTargetsForMachineGoal(const String& machineTypeId,
                                      const String& goalId,
                                      MotionTargetConfig& outConfig) const;
  static const char* trainingGoalToGoalId(TrainingGoal goal);
  // Aplica una configuracion remota traida desde Firebase al perfil local ya
  // cargado en RAM. Esto permite que la maquina reaccione a cambios cloud sin
  // recompilar firmware.
  bool applyCloudConfig(const MachineCloudConfig& cloudConfig);
};
