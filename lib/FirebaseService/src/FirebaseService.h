#pragma once

#include <Arduino.h>
#include "MachineRegistry.h"
#include "SessionRecorder.h"
#include "TimeService.h"
#include "UserRegistry.h"

// Configuracion inyectada desde AppController para no acoplar FirebaseService
// a pines, UI ni detalles del resto del firmware.
struct FirebaseRuntimeConfig {
  String wifiPrimarySsid;
  String wifiPrimaryPassword;
  String wifiSecondarySsid;
  String wifiSecondaryPassword;
  String databaseUrl;
  String authToken;
  String timezoneRule = "UTC0";
  uint32_t machinePollIntervalMs = 300000UL;
};

// Capa REST ligera para Firebase Realtime Database. Elegimos RTDB porque en
// ESP32 es mucho mas manejable hacer GET/PUT/PATCH con JSON y polling simple
// que intentar listeners complejos tipo servidor.
// Nodos esperados en Firebase:
// - usersByRfid/{uid}
// - calibrations/{uid}/{machineTypeId}
// - machineConfigs/{machineId}
// - athleteWeeklySessions/{uid}/{weekKey}/days/{dayKey}/...
class FirebaseService {
 public:
  ~FirebaseService();
  void begin(const FirebaseRuntimeConfig& config, TimeService& timeService);
  void update();
  bool isEnabled() const;
  bool isWifiConnected() const;
  String getActiveWifiSsid() const;
  String getWifiIpAddress() const;
  String getDeviceId() const;
  uint32_t getCurrentEpoch() const;
  String getCurrentIso() const;
  uint32_t getMachinePollIntervalMs() const;
  int getLastHttpStatusCode() const;
  String getLastErrorSummary() const;
  String buildUserProfileJson(const UserProfile& profile) const;
  String buildCalibrationJson(const UserMachineCalibration& calibration) const;
  String buildMachineConfigJson(const MachineCloudConfig& cloudConfig) const;
  String buildDailyMetaJson(const SessionHistoryRecord& record,
                            const String& dayKey,
                            const String& weekKey) const;
  String buildSessionTimelineJson(const SessionHistoryRecord& record,
                                  const String& dayKey,
                                  const String& weekKey) const;
  String buildAggregateSummaryJson(const String& existingJson,
                                   const SessionHistoryRecord& record,
                                   bool weeklyScope) const;
  bool putPathJson(const String& path, const String& payload);
  bool patchPathJson(const String& path, const String& payload);
  bool putPathJsonQueued(const String& path, const String& payload);
  bool patchPathJsonQueued(const String& path, const String& payload);
  bool pushMachineConfig(const MachineCloudConfig& cloudConfig);
  bool isInHttpBackoff() const;
  uint32_t httpBackoffRemainingMs() const;
  void setUseFirebaseClientCloudReads(bool enabled);
  void setUseFirebaseClientDeviceWrites(bool enabled);
  void setUseFirebaseClientProfileWrites(bool enabled);

  // pushSession escribe una sesion canonica en RTDB. El detalle rico se guarda
  // bajo sessions/{sessionId}; timeline actua como "huella" idempotente para
  // evitar que resyncs o retries vuelvan a inflar daySummary/weekSummary.
  bool pushSession(const SessionHistoryRecord& record);
  bool pushUserProfile(const UserProfile& profile);
  bool pushCalibration(const String& userUid, const UserMachineCalibration& calibration);
  bool pushDeviceHeartbeat(const String& machineId, const String& machineTypeId,
                           const String& machineDisplayName, const String& appState,
                           const String& activeUserUid, bool encoderCalibrationValid,
                           uint32_t encoderZeroRaw, uint32_t encoderFullRaw,
                           float encoderReferenceDistanceMm, bool encoderInvertDirection);
  bool fetchDeviceCalibration(const String& deviceId, String& json);
  bool fetchUserProfile(const String& userUid, UserProfile& profile);
  bool fetchUserProfileFirebaseClient(const String& userUid, UserProfile& profile);
  bool fetchCalibration(const String& userUid, const String& machineTypeId,
                        UserMachineCalibration& calibration);
  bool fetchMachineConfig(const String& machineId, MachineCloudConfig& cloudConfig);
  bool fetchPathJson(const String& path, String& response);
  static bool jsonStringValue(const String& json, const char* key, String& outValue);
  static bool jsonFloatValue(const String& json, const char* key, float& outValue);
  static bool jsonUIntValue(const String& json, const char* key, uint32_t& outValue);
  static bool jsonBoolValue(const String& json, const char* key, bool& outValue);

 private:
  FirebaseRuntimeConfig config_;
  TimeService* timeService_ = nullptr;
  uint32_t lastWifiAttemptMs_ = 0;
  int lastHttpStatusCode_ = 0;
  String lastErrorSummary_;
  String activeWifiSsid_;
  String lastAttemptedWifiSsid_;
  bool wifiAttemptInProgress_ = false;
  uint32_t wifiAttemptStartedMs_ = 0;
  uint32_t lastHttpFailureMs_ = 0;
  uint8_t consecutiveHttpFailures_ = 0;
  uint32_t httpBackoffUntilMs_ = 0;
  bool useFirebaseClientCloudReads_ = false;
  bool useFirebaseClientDeviceWrites_ = false;
  bool useFirebaseClientProfileWrites_ = false;
  struct QueuedTransportContext;
  QueuedTransportContext* queuedTransportContext_ = nullptr;

  bool ensureWifiConnected();
  bool ensureQueuedTransportReady();
  bool hasAnyWifiProfile() const;
  bool connectPrimaryWifi();
  bool connectSecondaryWifi();
  String chooseAvailableWifiSsid();
  String buildUrl(const String& path) const;
  bool requestJson(const String& method, const String& path, const String& payload, String& response);
  bool requestJsonQueuedTransport(const String& method,
                                  const String& path,
                                  const String& payload,
                                  String& response);
  bool getJsonOldReadLogged(const char* kind, const String& path, String& response);
  bool getJsonFirebaseClientLogged(const char* kind, const String& path, String& response);
  bool getJsonFirebaseClientWithTimeout(const String& path,
                                        String& response,
                                        uint32_t readTimeoutSec,
                                        uint32_t sendTimeoutSec);
  bool getJsonFirebaseClient(const String& path, String& response);
  bool getJson(const String& path, String& response);
  bool putJson(const String& path, const String& payload);
  bool patchJson(const String& path, const String& payload);
  bool pathExists(const String& path);
  static String escapeJson(const String& input);
  static String buildResponseSnippet(const String& response);
};
