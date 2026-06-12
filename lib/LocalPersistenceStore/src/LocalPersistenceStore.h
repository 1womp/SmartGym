#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include "SessionRecorder.h"
#include "UserRegistry.h"

struct PendingUploadRecord {
  String path;
  String payload;
  String method = "PUT";
  uint8_t attempts = 0;
  uint32_t enqueuedAtMs = 0;
};

struct WeeklyTrainingSignal {
  bool hasData = false;
  uint8_t sessions = 0;
  uint8_t recentSessionsUsed = 0;
  float avgSelectedWeightKg = 0.0f;
  float lastSelectedWeightKg = 0.0f;
  float avgSetCompletionRatio = 0.0f;   // setsCompleted / targetSets
  float avgRepCompletionRatio = 0.0f;   // validReps / (targetSets * targetRepsMax)
  float avgPeakVelocityPctPerSec = 0.0f;
  float avgRepQualityScore = 0.0f;      // 0..100 derived from session quality
  float poorFormSessionRatio = 0.0f;    // fraction of sessions considered poor form
};

// Persistencia local en NVS para sobrevivir reinicios. Guarda perfiles de
// usuario y una cola circular de writes pequenas para futura subida a
// Firebase. No intenta ser base de datos historica: en despliegue real la
// fuente de verdad es RTDB y NVS solo actua como cache + buffer corto.
class LocalPersistenceStore {
 public:
  bool begin() const;
  bool loadUsers(UserRegistry& registry) const;
  bool saveUsers(const UserRegistry& registry) const;
  bool appendSession(const SessionHistoryRecord& record) const;
  // enqueueUpload solo acepta payloads chicos. Si algo no cabe en NVS, se
  // omite en vez de corromper la cola; por eso el firmware prioriza summary,
  // timeline y documentos raiz sobre blobs profundos por rep.
  bool enqueueUpload(const String& path, const String& payload) const;
  bool enqueueUpload(const String& path, const String& payload, const String& method) const;
  bool repairUploadQueue(size_t maxPayloadBytes, bool dropOptional) const;
  bool peekUpload(PendingUploadRecord& record) const;
  bool dropOldestUpload() const;
  bool bumpOldestUploadAttempt() const;
  uint8_t getPendingUploadCount() const;
  bool buildWeeklyTrainingSignal(const String& userUid,
                                 const String& machineId,
                                 uint32_t nowEpoch,
                                 WeeklyTrainingSignal& signal) const;

 private:
  static constexpr const char* kUsersNamespace = "sg_users";
  static constexpr const char* kSessionsNamespace = "sg_sess";
  static constexpr const char* kUploadsNamespace = "sg_upq";
  static constexpr const char* kUserCountKey = "count";
  static constexpr const char* kSessionHeadKey = "head";
  static constexpr const char* kSessionCountKey = "count";
  static constexpr const char* kUploadHeadKey = "head";
  static constexpr const char* kUploadTailKey = "tail";
  static constexpr const char* kUploadCountKey = "count";
  static constexpr uint8_t kMaxStoredSessions = 4;
  static constexpr uint8_t kMaxPendingUploads = 16;
  static constexpr size_t kMaxSessionJsonLength = 2048;
  static constexpr size_t kMaxUploadPayloadLength = 4096;

  static String profileKey(uint8_t index);
  static String sessionKey(uint8_t index);
  static String uploadPathKey(uint8_t index);
  static String uploadPayloadKey(uint8_t index);
  static String uploadMetaKey(uint8_t index);
  static String serializeProfile(const UserProfile& profile);
  static String serializeSessionSummary(const SessionHistoryRecord& record);
  static bool deserializeProfile(const String& raw, UserProfile& profile);
};
