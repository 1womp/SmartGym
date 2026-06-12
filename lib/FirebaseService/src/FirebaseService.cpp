#include "FirebaseService.h"

#define ENABLE_DATABASE
#define ENABLE_LEGACY_TOKEN
#define ENABLE_ESP_SSLCLIENT

#include <FirebaseClient.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>
#include <esp_wifi.h>
#include <memory>

// Capa REST de Firebase RTDB para el firmware. Aqui se resuelve WiFi, JSON y
// escritura/lectura de nodos sin contaminar AppController con detalles HTTP.
namespace {
constexpr uint32_t kWifiRetryMs = 10000UL;
constexpr uint32_t kWifiConnectTimeoutMs = 16000UL;
constexpr uint32_t kHttpFailureBackoffMs = 3000UL;
constexpr uint32_t kHttpFailureBackoffMaxMs = 45000UL;
constexpr bool kCloudHttpTrace = false;
constexpr int kQueuedFcSslRxBufferBytes = 4096;
constexpr int kQueuedFcSslTxBufferBytes = 1024;
constexpr uint32_t kFcDefaultSyncReadTimeoutSec = 20;
constexpr uint32_t kFcDefaultSyncSendTimeoutSec = 20;
constexpr uint32_t kFcOptionalReadTimeoutSec = 6;

uint32_t internalFree8BitHeap() {
  return heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

uint32_t internalLargestFree8BitBlock() {
  return heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

const char* wifiAuthModeName(wifi_auth_mode_t authMode) {
  switch (authMode) {
    case WIFI_AUTH_OPEN:
      return "OPEN";
    case WIFI_AUTH_WEP:
      return "WEP";
    case WIFI_AUTH_WPA_PSK:
      return "WPA";
    case WIFI_AUTH_WPA2_PSK:
      return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:
      return "WPA/WPA2";
    case WIFI_AUTH_WPA2_ENTERPRISE:
      return "WPA2-ENT";
    case WIFI_AUTH_WPA3_PSK:
      return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK:
      return "WPA2/WPA3";
    default:
      return "UNKNOWN";
  }
}

void setRecommendationFields(const String& json, const char* prefix, GoalRecommendation& recommendation,
                             TrainingGoal goal) {
  recommendation.goal = goal;
  FirebaseService::jsonFloatValue(json, (String(prefix) + "WeightFactor").c_str(),
                                  recommendation.weightFactor);
  uint32_t temp = 0;
  if (FirebaseService::jsonUIntValue(json, (String(prefix) + "RepsMin").c_str(), temp)) {
    recommendation.repsMin = static_cast<uint8_t>(temp);
  }
  if (FirebaseService::jsonUIntValue(json, (String(prefix) + "RepsMax").c_str(), temp)) {
    recommendation.repsMax = static_cast<uint8_t>(temp);
  }
  if (FirebaseService::jsonUIntValue(json, (String(prefix) + "TargetSets").c_str(), temp)) {
    recommendation.targetSets = static_cast<uint8_t>(temp);
  }
  if (FirebaseService::jsonUIntValue(json, (String(prefix) + "RestSeconds").c_str(), temp)) {
    recommendation.restSeconds = static_cast<uint16_t>(temp);
  }
  if (FirebaseService::jsonUIntValue(json, (String(prefix) + "RiseMs").c_str(), temp)) {
    recommendation.riseMs = static_cast<uint16_t>(temp);
  }
  if (FirebaseService::jsonUIntValue(json, (String(prefix) + "LowerMs").c_str(), temp)) {
    recommendation.lowerMs = static_cast<uint16_t>(temp);
  }
  if (FirebaseService::jsonUIntValue(json, (String(prefix) + "TopPauseMs").c_str(), temp)) {
    recommendation.topPauseMs = static_cast<uint16_t>(temp);
  }
  if (FirebaseService::jsonUIntValue(json, (String(prefix) + "BottomPauseMs").c_str(), temp)) {
    recommendation.bottomPauseMs = static_cast<uint16_t>(temp);
  }
}

String buildCalendarDayKey(uint32_t epochSeconds) {
  if (epochSeconds == 0) {
    return "";
  }
  // Estos indices usan hora local para que "hoy" y "esta semana" coincidan con
  // lo que espera ver el usuario en la app, no necesariamente con UTC.
  time_t rawTime = static_cast<time_t>(epochSeconds);
  struct tm timeInfo {};
  localtime_r(&rawTime, &timeInfo);
  char buffer[16];
  if (strftime(buffer, sizeof(buffer), "%Y-%m-%d", &timeInfo) == 0) {
    return "";
  }
  return String(buffer);
}

String buildCalendarWeekKey(uint32_t epochSeconds) {
  if (epochSeconds == 0) {
    return "";
  }
  time_t rawTime = static_cast<time_t>(epochSeconds);
  struct tm timeInfo {};
  localtime_r(&rawTime, &timeInfo);
  char buffer[16];
  if (strftime(buffer, sizeof(buffer), "%G-W%V", &timeInfo) == 0) {
    return "";
  }
  return String(buffer);
}

String escapeJsonValue(const String& input) {
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

String buildSessionTimelineJson(const SessionHistoryRecord& record,
                                const String& dayKey,
                                const String& weekKey) {
  // Tarjeta ligera y ordenable para timelines de dia/semana. Contiene solo lo
  // necesario para listas y cards; el detalle profundo vive bajo sessions/.
  String json;
  json.reserve(1400);
  json += "{";
  json += "\"sessionId\":\"" + escapeJsonValue(record.sessionId) + "\",";
  json += "\"dayKey\":\"" + escapeJsonValue(dayKey) + "\",";
  json += "\"weekKey\":\"" + escapeJsonValue(weekKey) + "\",";
  json += "\"identity\":{";
  json += "\"userUid\":\"" + escapeJsonValue(record.userUid.isEmpty() ? "anonymous" : record.userUid) + "\",";
  json += "\"userDisplayName\":\"" + escapeJsonValue(record.userDisplayName) + "\"";
  json += "},";
  json += "\"ordering\":{";
  json += "\"startedAtEpoch\":" + String(record.startedAtEpoch) + ",";
  json += "\"endedAtEpoch\":" + String(record.endedAtEpoch) + ",";
  json += "\"startedAtIso\":\"" + escapeJsonValue(record.startedAtIso) + "\",";
  json += "\"endedAtIso\":\"" + escapeJsonValue(record.endedAtIso) + "\"";
  json += "},";
  json += "\"machine\":{";
  json += "\"machineId\":\"" + escapeJsonValue(record.machineId) + "\",";
  json += "\"machineTypeId\":\"" + escapeJsonValue(record.machineTypeId) + "\",";
  json += "\"machineDisplayName\":\"" + escapeJsonValue(record.machineDisplayName) + "\",";
  json += "\"exerciseCategory\":\"" + escapeJsonValue(record.exerciseCategory) + "\",";
  json += "\"primaryMuscleGroup\":\"" + escapeJsonValue(record.primaryMuscleGroup) + "\",";
  json += "\"secondaryMuscleGroup\":\"" + escapeJsonValue(record.secondaryMuscleGroup) + "\"";
  json += "},";
  json += "\"plan\":{";
  json += "\"goal\":\"" + escapeJsonValue(record.goal) + "\",";
  json += "\"selectedWeightKg\":" + String(record.selectedWeightKg, 2) + ",";
  json += "\"targetSets\":" + String(record.targetSets) + ",";
  json += "\"targetRepsMin\":" + String(record.targetRepsMin) + ",";
  json += "\"targetRepsMax\":" + String(record.targetRepsMax);
  json += "},";
  json += "\"summary\":{";
  json += "\"setsCompleted\":" + String(record.setsCompleted) + ",";
  json += "\"validReps\":" + String(record.validReps) + ",";
  json += "\"invalidReps\":" + String(record.invalidReps) + ",";
  json += "\"avgRomPercent\":" + String(record.avgRomPercent, 2) + ",";
  json += "\"sessionQualityScore\":" + String(record.sessionQualityScore, 2) + ",";
  json += "\"sessionQualityTier\":\"" + escapeJsonValue(record.sessionQualityTier) + "\",";
  json += "\"durationMs\":" + String(record.durationMs);
  json += "}";
  json += "}";
  return json;
}

String buildDailyMetaJson(const SessionHistoryRecord& record,
                          const String& dayKey,
                          const String& weekKey) {
  // El nodo del dia funciona como contenedor estable. "meta" permite al front
  // detectar rapidamente de quien es ese dia y cual fue la ultima sesion
  // registrada, sin recorrer sessions/ o timeline/.
  String json;
  json.reserve(768);
  json += "{";
  json += "\"dayKey\":\"" + escapeJsonValue(dayKey) + "\",";
  json += "\"weekKey\":\"" + escapeJsonValue(weekKey) + "\",";
  json += "\"userUid\":\"" + escapeJsonValue(record.userUid.isEmpty() ? "anonymous" : record.userUid) + "\",";
  json += "\"userDisplayName\":\"" + escapeJsonValue(record.userDisplayName) + "\",";
  json += "\"lastSessionId\":\"" + escapeJsonValue(record.sessionId) + "\",";
  json += "\"lastStartedAtEpoch\":" + String(record.startedAtEpoch) + ",";
  json += "\"lastEndedAtEpoch\":" + String(record.endedAtEpoch) + ",";
  json += "\"lastStartedAtIso\":\"" + escapeJsonValue(record.startedAtIso) + "\",";
  json += "\"lastEndedAtIso\":\"" + escapeJsonValue(record.endedAtIso) + "\",";
  json += "\"lastMachine\":{";
  json += "\"machineId\":\"" + escapeJsonValue(record.machineId) + "\",";
  json += "\"machineTypeId\":\"" + escapeJsonValue(record.machineTypeId) + "\",";
  json += "\"machineDisplayName\":\"" + escapeJsonValue(record.machineDisplayName) + "\"";
  json += "},";
  json += "\"lastSessionQualityScore\":" + String(record.sessionQualityScore, 2) + ",";
  json += "\"lastSessionQualityTier\":\"" + escapeJsonValue(record.sessionQualityTier) + "\"";
  json += "}";
  return json;
}

String buildTimelineKey(const SessionHistoryRecord& record) {
  return String(record.startedAtEpoch) + "_" + record.sessionId;
}

float computeSessionVolumeLoad(const SessionHistoryRecord& record) {
  return record.selectedWeightKg * static_cast<float>(record.validReps);
}

float computeValidRepRate(const SessionHistoryRecord& record) {
  const uint32_t total = static_cast<uint32_t>(record.validReps) + static_cast<uint32_t>(record.invalidReps);
  if (total == 0) {
    return 0.0f;
  }
  return (static_cast<float>(record.validReps) * 100.0f) / static_cast<float>(total);
}

float computeRomComplianceRate(const SessionHistoryRecord& record) {
  if (record.validReps == 0 || record.userRomPercent <= 0.0f) {
    return 0.0f;
  }
  uint16_t hitCount = 0;
  for (uint16_t i = 0; i < record.repCount; ++i) {
    const RepHistoryRecord& rep = record.reps[i];
    if (rep.valid && rep.romPercent >= record.userRomPercent) {
      hitCount++;
    }
  }
  return (static_cast<float>(hitCount) * 100.0f) / static_cast<float>(record.validReps);
}

float computeIdealRomHitRate(const SessionHistoryRecord& record) {
  if (record.validReps == 0 || record.machineIdealRomPercent <= 0.0f) {
    return 0.0f;
  }
  uint16_t hitCount = 0;
  for (uint16_t i = 0; i < record.repCount; ++i) {
    const RepHistoryRecord& rep = record.reps[i];
    if (rep.valid && rep.romPercent >= record.machineIdealRomPercent) {
      hitCount++;
    }
  }
  return (static_cast<float>(hitCount) * 100.0f) / static_cast<float>(record.validReps);
}

float computeAvgRestSecondsPerSet(const SessionHistoryRecord& record) {
  if (record.setsCompleted == 0) {
    return 0.0f;
  }
  return static_cast<float>(record.totalRestMs) / 1000.0f / static_cast<float>(record.setsCompleted);
}

float computeFatigueRomDrop(const SessionHistoryRecord& record) {
  if (record.setCount == 0) {
    return 0.0f;
  }
  return record.sets[0].avgRomPercent - record.sets[record.setCount - 1].avgRomPercent;
}

float computeFatigueVelocityDrop(const SessionHistoryRecord& record) {
  if (record.setCount == 0) {
    return 0.0f;
  }
  return record.sets[0].avgPeakVelocityPctPerSec - record.sets[record.setCount - 1].avgPeakVelocityPctPerSec;
}

String extractObjectBody(const String& json, const char* objectKey) {
  const String needle = "\"" + String(objectKey) + "\":{";
  const int start = json.indexOf(needle);
  if (start < 0) {
    return "";
  }
  int depth = 1;
  const int bodyStart = start + needle.length();
  for (int i = bodyStart; i < json.length(); ++i) {
    const char c = json.charAt(i);
    if (c == '{') {
      depth++;
    } else if (c == '}') {
      depth--;
      if (depth == 0) {
        return json.substring(bodyStart, i);
      }
    }
  }
  return "";
}

String mergeCounterObjectJson(const String& existingJson,
                              const char* objectKey,
                              const String& incrementKey) {
  String body = extractObjectBody(existingJson, objectKey);
  struct CounterEntry {
    String key;
    uint32_t value = 0;
  };
  CounterEntry entries[8];
  uint8_t count = 0;

  int cursor = 0;
  while (cursor < body.length() && count < 8) {
    const int keyStart = body.indexOf('"', cursor);
    if (keyStart < 0) {
      break;
    }
    const int keyEnd = body.indexOf('"', keyStart + 1);
    if (keyEnd < 0) {
      break;
    }
    const int valueStart = body.indexOf(':', keyEnd);
    if (valueStart < 0) {
      break;
    }
    int valueEnd = valueStart + 1;
    while (valueEnd < body.length()) {
      const char c = body.charAt(valueEnd);
      if ((c < '0' || c > '9')) {
        break;
      }
      valueEnd++;
    }
    entries[count].key = body.substring(keyStart + 1, keyEnd);
    entries[count].value = static_cast<uint32_t>(body.substring(valueStart + 1, valueEnd).toInt());
    count++;
    cursor = valueEnd + 1;
  }

  bool updated = false;
  for (uint8_t i = 0; i < count; ++i) {
    if (entries[i].key == incrementKey) {
      entries[i].value++;
      updated = true;
      break;
    }
  }
  if (!updated && count < 8) {
    entries[count].key = incrementKey;
    entries[count].value = 1;
    count++;
  }

  String merged = "{";
  for (uint8_t i = 0; i < count; ++i) {
    if (i > 0) {
      merged += ",";
    }
    merged += "\"" + escapeJsonValue(entries[i].key) + "\":" + String(entries[i].value);
  }
  merged += "}";
  return merged;
}

String buildAggregateSummaryJson(const String& existingJson,
                                 const SessionHistoryRecord& record,
                                 bool weeklyScope) {
  // daySummary/weekSummary son denormalizaciones para lectura rapida. Se
  // actualizan solo cuando una timelineKey nueva aparece por primera vez.
  uint32_t totalSessions = 0;
  uint32_t totalValidReps = 0;
  uint32_t totalInvalidReps = 0;
  uint32_t totalSetsCompleted = 0;
  uint32_t totalDurationMs = 0;
  uint32_t totalRestMs = 0;
  uint32_t totalFastEccentricWarnings = 0;
  float totalVolumeLoad = 0.0f;
  float avgRomPercent = 0.0f;
  float avgPeakVelocityPctPerSec = 0.0f;

  FirebaseService::jsonUIntValue(existingJson, "totalSessions", totalSessions);
  FirebaseService::jsonUIntValue(existingJson, "totalValidReps", totalValidReps);
  FirebaseService::jsonUIntValue(existingJson, "totalInvalidReps", totalInvalidReps);
  FirebaseService::jsonUIntValue(existingJson, "totalSetsCompleted", totalSetsCompleted);
  FirebaseService::jsonUIntValue(existingJson, "totalDurationMs", totalDurationMs);
  FirebaseService::jsonUIntValue(existingJson, "totalRestMs", totalRestMs);
  FirebaseService::jsonUIntValue(existingJson, "totalFastEccentricWarnings", totalFastEccentricWarnings);
  FirebaseService::jsonFloatValue(existingJson, "totalVolumeLoadKg", totalVolumeLoad);
  FirebaseService::jsonFloatValue(existingJson, "avgRomPercent", avgRomPercent);
  FirebaseService::jsonFloatValue(existingJson, "avgPeakVelocityPctPerSec", avgPeakVelocityPctPerSec);

  const uint32_t previousValidReps = totalValidReps;
  const float previousRomWeighted = avgRomPercent * static_cast<float>(previousValidReps);
  const float previousVelocityWeighted =
      avgPeakVelocityPctPerSec * static_cast<float>(previousValidReps);

  totalSessions += 1;
  totalValidReps += record.validReps;
  totalInvalidReps += record.invalidReps;
  totalSetsCompleted += record.setsCompleted;
  totalDurationMs += record.durationMs;
  totalRestMs += record.totalRestMs;
  totalFastEccentricWarnings += record.fastEccentricWarnings;
  totalVolumeLoad += computeSessionVolumeLoad(record);

  if (totalValidReps > 0) {
    avgRomPercent =
        (previousRomWeighted + (record.avgRomPercent * static_cast<float>(record.validReps))) /
        static_cast<float>(totalValidReps);
    avgPeakVelocityPctPerSec =
        (previousVelocityWeighted +
         (record.avgPeakVelocityPctPerSec * static_cast<float>(record.validReps))) /
        static_cast<float>(totalValidReps);
  }

  String json;
  json.reserve(1400);
  json += "{";
  json += "\"scope\":\"";
  json += weeklyScope ? "week" : "day";
  json += "\",";
  json += "\"lastSessionId\":\"" + escapeJsonValue(record.sessionId) + "\",";
  json += "\"lastStartedAtEpoch\":" + String(record.startedAtEpoch) + ",";
  json += "\"lastStartedAtIso\":\"" + escapeJsonValue(record.startedAtIso) + "\",";
  json += "\"totalSessions\":" + String(totalSessions) + ",";
  json += "\"totalValidReps\":" + String(totalValidReps) + ",";
  json += "\"totalInvalidReps\":" + String(totalInvalidReps) + ",";
  json += "\"totalSetsCompleted\":" + String(totalSetsCompleted) + ",";
  json += "\"totalDurationMs\":" + String(totalDurationMs) + ",";
  json += "\"totalRestMs\":" + String(totalRestMs) + ",";
  json += "\"totalFastEccentricWarnings\":" + String(totalFastEccentricWarnings) + ",";
  json += "\"totalVolumeLoadKg\":" + String(totalVolumeLoad, 2) + ",";
  json += "\"avgRomPercent\":" + String(avgRomPercent, 2) + ",";
  json += "\"avgPeakVelocityPctPerSec\":" + String(avgPeakVelocityPctPerSec, 2) + ",";
  json += "\"machineTypeCounts\":" + mergeCounterObjectJson(existingJson, "machineTypeCounts", record.machineTypeId) + ",";
  json += "\"goalCounts\":" + mergeCounterObjectJson(existingJson, "goalCounts", record.goal);
  json += "}";
  return json;
}
}

struct FirebaseService::QueuedTransportContext {
  QueuedTransportContext() : sslClient(transportClient, true), asyncClient(sslClient) {}

  WiFiClient transportClient;
  ESP_SSLClient2 sslClient;
  AsyncClientClass asyncClient;
  FirebaseApp app;
  RealtimeDatabase database;
  NoAuth noAuth;
  std::unique_ptr<LegacyToken> legacyToken;
  bool initialized = false;
};

FirebaseService::~FirebaseService() {
  if (queuedTransportContext_ != nullptr) {
    delete queuedTransportContext_;
    queuedTransportContext_ = nullptr;
  }
}

bool parseUserProfileFromJson(const String& userUid, const String& json, UserProfile& profile) {
  resetUserProfile(profile);
  profile.rfidUid = userUid;
  FirebaseService::jsonStringValue(json, "displayName", profile.displayName);
  FirebaseService::jsonBoolValue(json, "hasBasicData", profile.hasBasicData);
  FirebaseService::jsonFloatValue(json, "weightKg", profile.weightKg);
  uint32_t age = 0;
  if (FirebaseService::jsonUIntValue(json, "age", age)) {
    profile.age = static_cast<uint8_t>(age);
  }
  FirebaseService::jsonFloatValue(json, "heightCm", profile.heightCm);
  String genderRaw;
  if (FirebaseService::jsonStringValue(json, "gender", genderRaw)) {
    UserRegistry::parseGender(genderRaw, profile.gender);
  }
  String goalRaw;
  if (FirebaseService::jsonStringValue(json, "goal", goalRaw)) {
    UserRegistry::parseGoal(goalRaw, profile.goal);
  }
  FirebaseService::jsonUIntValue(json, "updatedAtEpoch", profile.updatedAtEpoch);
  FirebaseService::jsonStringValue(json, "updatedAtIso", profile.updatedAtIso);
  if (!profile.hasBasicData) {
    const bool hasMeaningfulData = !profile.displayName.isEmpty() ||
                                   profile.weightKg > 0.0f ||
                                   profile.age > 0 ||
                                   profile.heightCm > 0.0f;
    if (hasMeaningfulData) {
      profile.hasBasicData = true;
    }
  }
  return true;
}

void FirebaseService::begin(const FirebaseRuntimeConfig& config, TimeService& timeService) {
  config_ = config;
  timeService_ = &timeService;
  timeService_->begin(config_.timezoneRule);
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  WiFi.setSleep(false);
  esp_wifi_set_ps(WIFI_PS_NONE);
}

void FirebaseService::update() {
  const bool wifiOk = ensureWifiConnected();
  if (queuedTransportContext_ != nullptr && queuedTransportContext_->initialized) {
    queuedTransportContext_->app.loop();
  }
  if (timeService_ != nullptr) {
    timeService_->update(wifiOk);
  }
}

void FirebaseService::setUseFirebaseClientCloudReads(bool enabled) {
  useFirebaseClientCloudReads_ = enabled;
}

void FirebaseService::setUseFirebaseClientDeviceWrites(bool enabled) {
  useFirebaseClientDeviceWrites_ = enabled;
}

void FirebaseService::setUseFirebaseClientProfileWrites(bool enabled) {
  useFirebaseClientProfileWrites_ = enabled;
}

bool FirebaseService::isEnabled() const {
  return hasAnyWifiProfile() && !config_.databaseUrl.isEmpty();
}

bool FirebaseService::isWifiConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

String FirebaseService::getActiveWifiSsid() const {
  return activeWifiSsid_;
}

String FirebaseService::getWifiIpAddress() const {
  return isWifiConnected() ? WiFi.localIP().toString() : "";
}

String FirebaseService::getDeviceId() const {
  String deviceId = WiFi.macAddress();
  deviceId.replace(":", "-");
  return deviceId;
}

uint32_t FirebaseService::getCurrentEpoch() const {
  return timeService_ != nullptr ? timeService_->getEpoch() : 0;
}

String FirebaseService::getCurrentIso() const {
  return timeService_ != nullptr ? timeService_->getIso8601() : "";
}

uint32_t FirebaseService::getMachinePollIntervalMs() const {
  return config_.machinePollIntervalMs;
}

int FirebaseService::getLastHttpStatusCode() const {
  return lastHttpStatusCode_;
}

String FirebaseService::getLastErrorSummary() const {
  return lastErrorSummary_;
}

bool FirebaseService::isInHttpBackoff() const {
  return httpBackoffRemainingMs() > 0;
}

uint32_t FirebaseService::httpBackoffRemainingMs() const {
  const uint32_t nowMs = millis();
  if (httpBackoffUntilMs_ == 0 || nowMs >= httpBackoffUntilMs_) {
    return 0;
  }
  return httpBackoffUntilMs_ - nowMs;
}

bool FirebaseService::ensureWifiConnected() {
  if (!isEnabled()) {
    lastErrorSummary_ = "Cloud deshabilitado";
    return false;
  }

  if (WiFi.status() == WL_CONNECTED) {
    activeWifiSsid_ = WiFi.SSID();
    lastAttemptedWifiSsid_ = activeWifiSsid_;
    wifiAttemptInProgress_ = false;
    return true;
  }

  const uint32_t nowMs = millis();
  if (wifiAttemptInProgress_) {
    const uint32_t elapsedMs = nowMs - wifiAttemptStartedMs_;
    if (elapsedMs < kWifiConnectTimeoutMs) {
      lastErrorSummary_ = "WiFi conectando: " + lastAttemptedWifiSsid_;
      return false;
    }

    Serial.print("[WiFi] timeout ssid=");
    Serial.print(lastAttemptedWifiSsid_);
    Serial.print(" status=");
    Serial.println(static_cast<int>(WiFi.status()));
    WiFi.disconnect(false, false);
    wifiAttemptInProgress_ = false;
    lastWifiAttemptMs_ = nowMs;
    lastErrorSummary_ = "WiFi timeout: " + lastAttemptedWifiSsid_;
    return false;
  }

  if (lastWifiAttemptMs_ != 0 && nowMs - lastWifiAttemptMs_ < kWifiRetryMs) {
    lastErrorSummary_ = "WiFi reconectando";
    return false;
  }

  lastWifiAttemptMs_ = nowMs;
  activeWifiSsid_.clear();
  // Antes de llamar WiFi.begin elegimos a que red intentaremos entrar.
  // Ambas redes configuradas son PSK normales.
  const String targetSsid = chooseAvailableWifiSsid();
  if (targetSsid.isEmpty()) {
    lastErrorSummary_ = "No se detecto una red WiFi configurada";
    return false;
  }

  bool connectStarted = false;
  if (targetSsid == config_.wifiPrimarySsid) {
    connectStarted = connectPrimaryWifi();
  } else if (targetSsid == config_.wifiSecondarySsid) {
    connectStarted = connectSecondaryWifi();
  }

  if (!connectStarted) {
    lastErrorSummary_ = "No se pudo iniciar WiFi para " + targetSsid;
    return false;
  }

  activeWifiSsid_ = targetSsid;
  lastAttemptedWifiSsid_ = targetSsid;
  wifiAttemptInProgress_ = true;
  wifiAttemptStartedMs_ = nowMs;
  lastErrorSummary_ = "Intentando conectar WiFi PSK: " + targetSsid;
  return false;
}

bool FirebaseService::hasAnyWifiProfile() const {
  return !config_.wifiPrimarySsid.isEmpty() || !config_.wifiSecondarySsid.isEmpty();
}

bool FirebaseService::connectPrimaryWifi() {
  if (config_.wifiPrimarySsid.isEmpty()) {
    return false;
  }

  WiFi.mode(WIFI_STA);
  Serial.print("[WiFi] begin PSK ssid=");
  Serial.println(config_.wifiPrimarySsid);
  const bool started = WiFi.begin(config_.wifiPrimarySsid.c_str(), config_.wifiPrimaryPassword.c_str()) != WL_CONNECT_FAILED;
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  return started;
}

bool FirebaseService::connectSecondaryWifi() {
  if (config_.wifiSecondarySsid.isEmpty() || config_.wifiSecondaryPassword.isEmpty()) {
    return false;
  }

  WiFi.mode(WIFI_STA);
  Serial.print("[WiFi] begin PSK ssid=");
  Serial.println(config_.wifiSecondarySsid);
  const bool started = WiFi.begin(config_.wifiSecondarySsid.c_str(), config_.wifiSecondaryPassword.c_str()) !=
                       WL_CONNECT_FAILED;
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  return started;
}

String FirebaseService::chooseAvailableWifiSsid() {
  if (config_.wifiPrimarySsid.isEmpty() && config_.wifiSecondarySsid.isEmpty()) {
    return "";
  }

  bool primaryVisible = false;
  bool secondaryVisible = false;
  int32_t primaryBestRssi = -1000;
  int32_t secondaryBestRssi = -1000;
  int32_t primaryBestChannel = 0;
  int32_t secondaryBestChannel = 0;
  wifi_auth_mode_t primaryBestAuth = WIFI_AUTH_OPEN;
  wifi_auth_mode_t secondaryBestAuth = WIFI_AUTH_OPEN;
  const int networkCount = WiFi.scanNetworks(false, true);
  if (networkCount >= 0) {
    for (int i = 0; i < networkCount; ++i) {
      const String ssid = WiFi.SSID(i);
      const int32_t rssi = WiFi.RSSI(i);
      if (!config_.wifiPrimarySsid.isEmpty() && ssid == config_.wifiPrimarySsid) {
        primaryVisible = true;
        if (rssi > primaryBestRssi) {
          primaryBestRssi = rssi;
          primaryBestChannel = WiFi.channel(i);
          primaryBestAuth = WiFi.encryptionType(i);
        }
      }
      if (!config_.wifiSecondarySsid.isEmpty() && ssid == config_.wifiSecondarySsid) {
        secondaryVisible = true;
        if (rssi > secondaryBestRssi) {
          secondaryBestRssi = rssi;
          secondaryBestChannel = WiFi.channel(i);
          secondaryBestAuth = WiFi.encryptionType(i);
        }
      }
    }
    WiFi.scanDelete();
  }

  Serial.print("[WiFi] scan=");
  Serial.print(networkCount);
  Serial.print(" primary=");
  Serial.print(primaryVisible ? "seen" : "missing");
  Serial.print(" secondary=");
  Serial.print(secondaryVisible ? "seen" : "missing");
  Serial.print(" last=");
  Serial.println(lastAttemptedWifiSsid_.isEmpty() ? "-" : lastAttemptedWifiSsid_);
  if (primaryVisible) {
    Serial.print("[WiFi] primary detail rssi=");
    Serial.print(primaryBestRssi);
    Serial.print(" ch=");
    Serial.print(primaryBestChannel);
    Serial.print(" auth=");
    Serial.println(wifiAuthModeName(primaryBestAuth));
  }
  if (secondaryVisible) {
    Serial.print("[WiFi] secondary detail rssi=");
    Serial.print(secondaryBestRssi);
    Serial.print(" ch=");
    Serial.print(secondaryBestChannel);
    Serial.print(" auth=");
    Serial.println(wifiAuthModeName(secondaryBestAuth));
  }

  if (networkCount < 0) {
    // A failed scan usually means the radio was still recovering from a failed
    // association. Retry the last visible target instead of jumping networks.
    if (!lastAttemptedWifiSsid_.isEmpty()) {
      return lastAttemptedWifiSsid_;
    }
    if (!config_.wifiSecondarySsid.isEmpty()) {
      return config_.wifiSecondarySsid;
    }
    return config_.wifiPrimarySsid;
  }

  if (primaryVisible) {
    if (lastAttemptedWifiSsid_ == config_.wifiPrimarySsid && secondaryVisible) {
      return config_.wifiSecondarySsid;
    }
    return config_.wifiPrimarySsid;
  }
  if (secondaryVisible) {
    return config_.wifiSecondarySsid;
  }

  if (!config_.wifiPrimarySsid.isEmpty()) {
    if (lastAttemptedWifiSsid_ == config_.wifiPrimarySsid && !config_.wifiSecondarySsid.isEmpty()) {
      return config_.wifiSecondarySsid;
    }
    return config_.wifiPrimarySsid;
  }
  return config_.wifiSecondarySsid;
}

String FirebaseService::buildUrl(const String& path) const {
  String url = config_.databaseUrl;
  if (url.endsWith("/")) {
    url.remove(url.length() - 1);
  }

  url += "/";
  url += path;
  url += ".json";
  if (!config_.authToken.isEmpty()) {
    url += "?auth=";
    url += config_.authToken;
  }
  return url;
}

bool FirebaseService::ensureQueuedTransportReady() {
  if (queuedTransportContext_ == nullptr) {
    queuedTransportContext_ = new QueuedTransportContext();
  }
  QueuedTransportContext& ctx = *queuedTransportContext_;
  if (ctx.initialized) {
    return true;
  }

  ctx.sslClient.setInsecure();
  ctx.sslClient.setTimeout(5000);
  ctx.sslClient.setHandshakeTimeout(8000);
  ctx.sslClient.setBufferSizes(kQueuedFcSslRxBufferBytes, kQueuedFcSslTxBufferBytes);
  ctx.asyncClient.setSyncReadTimeout(kFcDefaultSyncReadTimeoutSec);
  ctx.asyncClient.setSyncSendTimeout(kFcDefaultSyncSendTimeoutSec);

  if (config_.authToken.isEmpty()) {
    initializeApp(ctx.asyncClient, ctx.app, getAuth(ctx.noAuth));
  } else {
    ctx.legacyToken = std::make_unique<LegacyToken>(config_.authToken);
    initializeApp(ctx.asyncClient, ctx.app, getAuth(*ctx.legacyToken));
  }
  ctx.app.getApp<RealtimeDatabase>(ctx.database);
  ctx.database.url(config_.databaseUrl);
  ctx.initialized = true;
  return true;
}

bool FirebaseService::requestJsonQueuedTransport(const String& method, const String& path,
                                                 const String& payload, String& response) {
  response = "";
  if (!ensureWifiConnected() || !ensureQueuedTransportReady()) {
    return false;
  }
  if (isInHttpBackoff()) {
    lastHttpStatusCode_ = 0;
    lastErrorSummary_ = "Cloud TLS backoff";
    return false;
  }

  QueuedTransportContext& ctx = *queuedTransportContext_;
  ctx.app.loop();
  ctx.asyncClient.setSyncReadTimeout(kFcDefaultSyncReadTimeoutSec);
  ctx.asyncClient.setSyncSendTimeout(kFcDefaultSyncSendTimeoutSec);
  bool ok = false;
  if (method == "PUT") {
    ok = ctx.database.set<object_t>(ctx.asyncClient, path, object_t(payload));
  } else if (method == "PATCH") {
    ok = ctx.database.update(ctx.asyncClient, path, object_t(payload));
  } else {
    lastHttpStatusCode_ = -2;
    lastErrorSummary_ = "Queued transport method unsupported: " + method;
    return false;
  }

  if (ok) {
    lastHttpStatusCode_ = 200;
    lastErrorSummary_ = "";
    lastHttpFailureMs_ = 0;
    consecutiveHttpFailures_ = 0;
    httpBackoffUntilMs_ = 0;
    return true;
  }

  const int errorCode = ctx.asyncClient.lastError().code();
  const String errorText = ctx.asyncClient.lastError().message();
  lastHttpStatusCode_ = (errorCode == 0) ? -1 : errorCode;
  lastErrorSummary_ = method + " " + path + " -> " + errorText + " code=" + String(errorCode);
  if (lastHttpStatusCode_ < 0 || lastHttpStatusCode_ == 429 || lastHttpStatusCode_ >= 500) {
    lastHttpFailureMs_ = millis();
    consecutiveHttpFailures_ = static_cast<uint8_t>(min<uint16_t>(consecutiveHttpFailures_ + 1U, 12U));
    const uint32_t multiplier = (1UL << min<uint8_t>(consecutiveHttpFailures_, 4));
    const uint32_t backoffMs = min<uint32_t>(kHttpFailureBackoffMaxMs, kHttpFailureBackoffMs * multiplier);
    httpBackoffUntilMs_ = lastHttpFailureMs_ + backoffMs;
  }
  return false;
}

bool FirebaseService::requestJson(const String& method, const String& path, const String& payload,
                                  String& response) {
  const uint32_t legacyStartedAt = millis();
  Serial.printf("[CloudLegacy] begin kind=request method=%s path=%s payloadBytes=%u heap=%lu\n",
                method.c_str(),
                path.c_str(),
                static_cast<unsigned>(payload.length()),
                static_cast<unsigned long>(ESP.getFreeHeap()));
  static uint32_t sLastReqTraceMs = 0;
  const uint32_t traceNowMs = millis();
  const bool traceThisRequest = kCloudHttpTrace && (traceNowMs - sLastReqTraceMs) >= 1500UL;
  if (traceThisRequest) {
    sLastReqTraceMs = traceNowMs;
    Serial.printf("[CLOUD][REQ] method=%s path=%s payloadBytes=%u heap=%u lastHttp=%d\n",
                  method.c_str(),
                  path.c_str(),
                  static_cast<unsigned>(payload.length()),
                  static_cast<unsigned>(ESP.getFreeHeap()),
                  lastHttpStatusCode_);
  }
  if (!ensureWifiConnected()) {
    Serial.printf("[CloudLegacy] end kind=request ok=0 method=%s path=%s durationMs=%lu http=%d error=wifi_not_connected heap=%lu\n",
                  method.c_str(),
                  path.c_str(),
                  static_cast<unsigned long>(millis() - legacyStartedAt),
                  lastHttpStatusCode_,
                  static_cast<unsigned long>(ESP.getFreeHeap()));
    return false;
  }

  const uint32_t nowMs = millis();
  if (httpBackoffUntilMs_ != 0 && nowMs < httpBackoffUntilMs_) {
    lastHttpStatusCode_ = 0;
    lastErrorSummary_ = "Cloud TLS backoff";
    Serial.printf("[CloudLegacy] end kind=request ok=0 method=%s path=%s durationMs=%lu http=%d error=%s heap=%lu\n",
                  method.c_str(),
                  path.c_str(),
                  static_cast<unsigned long>(millis() - legacyStartedAt),
                  lastHttpStatusCode_,
                  lastErrorSummary_.c_str(),
                  static_cast<unsigned long>(ESP.getFreeHeap()));
    return false;
  }

  // TLS handshakes on ESP32-S3 are bursty in heap use. If we are already low,
  // skip this cycle and back off instead of thrashing sockets.
  const size_t payloadBytes = payload.length();
  const bool writeRequest = method == "PUT" || method == "PATCH";
  const bool smallWrite = writeRequest && payloadBytes <= 1500U;
  const bool mediumWrite = writeRequest && payloadBytes <= 2500U;
  const bool readRequest = method == "GET";
  uint32_t minHeapForTls = 70000UL;
  if (readRequest || smallWrite) {
    minHeapForTls = 52000UL;
  } else if (mediumWrite) {
    minHeapForTls = 60000UL;
  }
  const uint32_t freeHeap = ESP.getFreeHeap();
  if (freeHeap < minHeapForTls) {
    lastHttpStatusCode_ = 0;
    lastErrorSummary_ = String("Cloud TLS skipped freeHeap=") + String(freeHeap) +
                        " threshold=" + String(minHeapForTls) +
                        " method=" + method +
                        " payloadBytes=" + String(payloadBytes);
    lastHttpFailureMs_ = nowMs;
    const uint32_t multiplier = (1UL << min<uint8_t>(max<uint8_t>(consecutiveHttpFailures_, 1), 4));
    const uint32_t backoffMs = min<uint32_t>(kHttpFailureBackoffMaxMs, kHttpFailureBackoffMs * multiplier);
    httpBackoffUntilMs_ = nowMs + backoffMs;
    Serial.printf("[CloudLegacy] end kind=request ok=0 method=%s path=%s durationMs=%lu http=%d error=%s heap=%lu\n",
                  method.c_str(),
                  path.c_str(),
                  static_cast<unsigned long>(millis() - legacyStartedAt),
                  lastHttpStatusCode_,
                  lastErrorSummary_.c_str(),
                  static_cast<unsigned long>(ESP.getFreeHeap()));
    return false;
  }

  HTTPClient http;
  const String url = buildUrl(path);
  WiFiClientSecure secureClient;
  int statusCode = -1;
  lastHttpStatusCode_ = 0;
  response = "";
  // Reuse=false avoids stale socket reuse after intermittent TLS failures.
  http.setReuse(false);
  http.setConnectTimeout(2200);
  http.setTimeout(3200);
  if (url.startsWith("https://")) {
    // El cliente TLS debe vivir hasta terminar el GET/PUT. Si se crea dentro
    // del bloque y se destruye antes de http.GET()/PUT(), la conexion HTTPS
    // queda apuntando a memoria invalida y aparecen fallos intermitentes.
    secureClient.setInsecure();
    if (!http.begin(secureClient, url)) {
      lastErrorSummary_ = "No se pudo abrir HTTPS a " + path;
      lastHttpFailureMs_ = millis();
      consecutiveHttpFailures_ = static_cast<uint8_t>(min<uint16_t>(consecutiveHttpFailures_ + 1U, 12U));
      const uint32_t multiplier = (1UL << min<uint8_t>(consecutiveHttpFailures_, 4));
      const uint32_t backoffMs = min<uint32_t>(kHttpFailureBackoffMaxMs, kHttpFailureBackoffMs * multiplier);
      httpBackoffUntilMs_ = lastHttpFailureMs_ + backoffMs;
      Serial.printf("[CloudLegacy] end kind=request ok=0 method=%s path=%s durationMs=%lu http=%d error=%s heap=%lu\n",
                    method.c_str(),
                    path.c_str(),
                    static_cast<unsigned long>(millis() - legacyStartedAt),
                    lastHttpStatusCode_,
                    lastErrorSummary_.c_str(),
                    static_cast<unsigned long>(ESP.getFreeHeap()));
      return false;
    }
  } else {
    if (!http.begin(url)) {
      lastErrorSummary_ = "No se pudo abrir HTTP a " + path;
      lastHttpFailureMs_ = millis();
      consecutiveHttpFailures_ = static_cast<uint8_t>(min<uint16_t>(consecutiveHttpFailures_ + 1U, 12U));
      const uint32_t multiplier = (1UL << min<uint8_t>(consecutiveHttpFailures_, 4));
      const uint32_t backoffMs = min<uint32_t>(kHttpFailureBackoffMaxMs, kHttpFailureBackoffMs * multiplier);
      httpBackoffUntilMs_ = lastHttpFailureMs_ + backoffMs;
      Serial.printf("[CloudLegacy] end kind=request ok=0 method=%s path=%s durationMs=%lu http=%d error=%s heap=%lu\n",
                    method.c_str(),
                    path.c_str(),
                    static_cast<unsigned long>(millis() - legacyStartedAt),
                    lastHttpStatusCode_,
                    lastErrorSummary_.c_str(),
                    static_cast<unsigned long>(ESP.getFreeHeap()));
      return false;
    }
  }

  http.addHeader("Content-Type", "application/json");
  if (method == "GET") {
    statusCode = http.GET();
  } else if (method == "PUT") {
    statusCode = http.PUT(payload);
  } else if (method == "PATCH") {
    statusCode = http.sendRequest("PATCH", payload);
  } else {
    http.end();
    lastErrorSummary_ = "Metodo no soportado: " + method;
    Serial.printf("[CloudLegacy] end kind=request ok=0 method=%s path=%s durationMs=%lu http=%d error=%s heap=%lu\n",
                  method.c_str(),
                  path.c_str(),
                  static_cast<unsigned long>(millis() - legacyStartedAt),
                  lastHttpStatusCode_,
                  lastErrorSummary_.c_str(),
                  static_cast<unsigned long>(ESP.getFreeHeap()));
    return false;
  }

  lastHttpStatusCode_ = statusCode;
  if (statusCode >= 200 && statusCode < 300) {
    // GET callers need the RTDB JSON body for profile/config/calibration
    // reconciliation and idempotency checks. PUT/PATCH responses can echo
    // large payloads, so skip them to keep heap pressure low.
    response = method == "GET" ? http.getString() : "";
    http.end();
    lastErrorSummary_ = "";
    lastHttpFailureMs_ = 0;
    consecutiveHttpFailures_ = 0;
    httpBackoffUntilMs_ = 0;
    if (traceThisRequest) {
      Serial.printf("[CLOUD][RES] method=%s path=%s status=%d heap=%u\n",
                    method.c_str(),
                    path.c_str(),
                    statusCode,
                    static_cast<unsigned>(ESP.getFreeHeap()));
    }
    Serial.printf("[CloudLegacy] end kind=request ok=1 method=%s path=%s durationMs=%lu http=%d error=%s heap=%lu\n",
                  method.c_str(),
                  path.c_str(),
                  static_cast<unsigned long>(millis() - legacyStartedAt),
                  lastHttpStatusCode_,
                  lastErrorSummary_.c_str(),
                  static_cast<unsigned long>(ESP.getFreeHeap()));
    return true;
  }
  response = http.getString();
  http.end();

  // Guardamos un resumen corto del error para que comandos como "cloud" o
  // mensajes de cola offline puedan mostrar rapidamente que fallo.
  lastErrorSummary_ = method + " " + path + " -> HTTP " + String(statusCode);
  const String responseSnippet = buildResponseSnippet(response);
  if (!responseSnippet.isEmpty()) {
    lastErrorSummary_ += " | " + responseSnippet;
  }
  if (statusCode < 0 || statusCode == 429 || statusCode >= 500) {
    lastHttpFailureMs_ = millis();
    consecutiveHttpFailures_ = static_cast<uint8_t>(min<uint16_t>(consecutiveHttpFailures_ + 1U, 12U));
    const uint32_t multiplier = (1UL << min<uint8_t>(consecutiveHttpFailures_, 4));
    const uint32_t backoffMs = min<uint32_t>(kHttpFailureBackoffMaxMs, kHttpFailureBackoffMs * multiplier);
    httpBackoffUntilMs_ = lastHttpFailureMs_ + backoffMs;
  }
  if (traceThisRequest) {
    Serial.printf("[CLOUD][RES] method=%s path=%s status=%d heap=%u\n",
                  method.c_str(),
                  path.c_str(),
                  statusCode,
                  static_cast<unsigned>(ESP.getFreeHeap()));
  }
  Serial.printf("[CloudLegacy] end kind=request ok=0 method=%s path=%s durationMs=%lu http=%d error=%s heap=%lu\n",
                method.c_str(),
                path.c_str(),
                static_cast<unsigned long>(millis() - legacyStartedAt),
                lastHttpStatusCode_,
                lastErrorSummary_.c_str(),
                static_cast<unsigned long>(ESP.getFreeHeap()));
  return false;
}

bool FirebaseService::getJsonOldReadLogged(const char* kind, const String& path, String& response) {
  const uint32_t startedAt = millis();
  Serial.printf("[CloudRead] old begin kind=%s path=%s\n",
                kind != nullptr ? kind : "unknown",
                path.c_str());
  const bool ok = getJson(path, response);
  const uint32_t durationMs = millis() - startedAt;
  Serial.printf("[CloudRead] old end kind=%s ok=%d http=%d durationMs=%lu error=%s\n",
                kind != nullptr ? kind : "unknown",
                ok ? 1 : 0,
                lastHttpStatusCode_,
                static_cast<unsigned long>(durationMs),
                lastErrorSummary_.c_str());
  return ok;
}

bool FirebaseService::getJsonFirebaseClientLogged(const char* kind, const String& path, String& response) {
  const uint32_t startedAt = millis();
  const bool optionalRead = kind != nullptr && String(kind) == "path_json";
  Serial.printf("[CloudRead][fc] begin kind=%s path=%s heap=%lu internalHeap=%lu largestBlock=%lu\n",
                kind != nullptr ? kind : "unknown",
                path.c_str(),
                static_cast<unsigned long>(ESP.getFreeHeap()),
                static_cast<unsigned long>(internalFree8BitHeap()),
                static_cast<unsigned long>(internalLargestFree8BitBlock()));
  const bool ok = optionalRead
                      ? getJsonFirebaseClientWithTimeout(path,
                                                         response,
                                                         kFcOptionalReadTimeoutSec,
                                                         kFcOptionalReadTimeoutSec)
                      : getJsonFirebaseClient(path, response);
  const uint32_t durationMs = millis() - startedAt;
  if (optionalRead && !ok && durationMs >= (kFcOptionalReadTimeoutSec * 1000UL)) {
    Serial.printf("[CloudRead] optional timeout kind=%s durationMs=%lu path=%s\n",
                  kind != nullptr ? kind : "unknown",
                  static_cast<unsigned long>(durationMs),
                  path.c_str());
  }
  Serial.printf("[CloudRead][fc] end kind=%s ok=%d durationMs=%lu error=%s payloadBytes=%u heap=%lu internalHeap=%lu largestBlock=%lu\n",
                kind != nullptr ? kind : "unknown",
                ok ? 1 : 0,
                static_cast<unsigned long>(durationMs),
                lastErrorSummary_.c_str(),
                static_cast<unsigned>(response.length()),
                static_cast<unsigned long>(ESP.getFreeHeap()),
                static_cast<unsigned long>(internalFree8BitHeap()),
                static_cast<unsigned long>(internalLargestFree8BitBlock()));
  return ok;
}

bool FirebaseService::getJsonFirebaseClientWithTimeout(const String& path,
                                                       String& response,
                                                       uint32_t readTimeoutSec,
                                                       uint32_t sendTimeoutSec) {
  response = "";
  if (!ensureWifiConnected() || !ensureQueuedTransportReady()) {
    return false;
  }
  if (isInHttpBackoff()) {
    lastHttpStatusCode_ = 0;
    lastErrorSummary_ = "Cloud TLS backoff";
    return false;
  }

  QueuedTransportContext& ctx = *queuedTransportContext_;
  ctx.asyncClient.setSyncReadTimeout(readTimeoutSec);
  ctx.asyncClient.setSyncSendTimeout(sendTimeoutSec);
  ctx.app.loop();
  const String value = ctx.database.get<String>(ctx.asyncClient, path);
  ctx.asyncClient.setSyncReadTimeout(kFcDefaultSyncReadTimeoutSec);
  ctx.asyncClient.setSyncSendTimeout(kFcDefaultSyncSendTimeoutSec);
  const int errorCode = ctx.asyncClient.lastError().code();
  if (errorCode == 0) {
    response = value;
    response.trim();
    lastHttpStatusCode_ = 200;
    lastErrorSummary_ = "";
    lastHttpFailureMs_ = 0;
    consecutiveHttpFailures_ = 0;
    httpBackoffUntilMs_ = 0;
    return true;
  }

  lastHttpStatusCode_ = errorCode > 0 ? errorCode : -1;
  lastErrorSummary_ = String("GET ") + path + " -> " + ctx.asyncClient.lastError().message() +
                      " code=" + String(errorCode);
  if (lastHttpStatusCode_ < 0 || lastHttpStatusCode_ == 429 || lastHttpStatusCode_ >= 500) {
    lastHttpFailureMs_ = millis();
    consecutiveHttpFailures_ = static_cast<uint8_t>(min<uint16_t>(consecutiveHttpFailures_ + 1U, 12U));
    const uint32_t multiplier = (1UL << min<uint8_t>(consecutiveHttpFailures_, 4));
    const uint32_t backoffMs = min<uint32_t>(kHttpFailureBackoffMaxMs, kHttpFailureBackoffMs * multiplier);
    httpBackoffUntilMs_ = lastHttpFailureMs_ + backoffMs;
  }
  return false;
}

bool FirebaseService::getJsonFirebaseClient(const String& path, String& response) {
  return getJsonFirebaseClientWithTimeout(path,
                                          response,
                                          kFcDefaultSyncReadTimeoutSec,
                                          kFcDefaultSyncSendTimeoutSec);
}

bool FirebaseService::getJson(const String& path, String& response) {
  return requestJson("GET", path, "", response);
}

bool FirebaseService::putJson(const String& path, const String& payload) {
  String response;
  return requestJson("PUT", path, payload, response);
}

bool FirebaseService::patchJson(const String& path, const String& payload) {
  String response;
  return requestJson("PATCH", path, payload, response);
}

bool FirebaseService::putPathJson(const String& path, const String& payload) {
  // PUT se usa porque queremos reescribir documentos canonicos completos en vez
  // de depender de muchos PATCH pequeños con estructuras parcialmente vivas.
  return putJson(path, payload);
}

bool FirebaseService::patchPathJson(const String& path, const String& payload) {
  return patchJson(path, payload);
}

bool FirebaseService::putPathJsonQueued(const String& path, const String& payload) {
  String response;
  return requestJsonQueuedTransport("PUT", path, payload, response);
}

bool FirebaseService::patchPathJsonQueued(const String& path, const String& payload) {
  String response;
  return requestJsonQueuedTransport("PATCH", path, payload, response);
}

bool FirebaseService::pathExists(const String& path) {
  // El timeline del dia funciona como marcador de "esta sesion ya fue contada".
  // Si el path existe, podemos reescribir detalle sin volver a inflar resumenes.
  String response;
  if (!getJson(path, response)) {
    return false;
  }
  response.trim();
  return !response.isEmpty() && response != "null";
}

String FirebaseService::buildDailyMetaJson(const SessionHistoryRecord& record,
                                           const String& dayKey,
                                           const String& weekKey) const {
  return ::buildDailyMetaJson(record, dayKey, weekKey);
}

String FirebaseService::buildSessionTimelineJson(const SessionHistoryRecord& record,
                                                 const String& dayKey,
                                                 const String& weekKey) const {
  return ::buildSessionTimelineJson(record, dayKey, weekKey);
}

String FirebaseService::buildAggregateSummaryJson(const String& existingJson,
                                                  const SessionHistoryRecord& record,
                                                  bool weeklyScope) const {
  return ::buildAggregateSummaryJson(existingJson, record, weeklyScope);
}

bool FirebaseService::pushSession(const SessionHistoryRecord& record) {
  SessionRecorder temp;
  temp.loadRecord(record);
  const uint32_t anchorEpoch = record.startedAtEpoch != 0 ? record.startedAtEpoch : record.endedAtEpoch;
  const String dayKey = buildCalendarDayKey(anchorEpoch);
  const String weekKey = buildCalendarWeekKey(anchorEpoch);
  const String userKey = record.userUid.isEmpty() ? "anonymous" : record.userUid;
  const String safeDayKey = dayKey.isEmpty() ? "undated" : dayKey;
  const String safeWeekKey = weekKey.isEmpty() ? "unscheduled" : weekKey;
  const String timelineJson = buildSessionTimelineJson(record, safeDayKey, safeWeekKey);
  const String dailyMetaJson = buildDailyMetaJson(record, safeDayKey, safeWeekKey);
  const String weekBasePath = "athleteWeeklySessions/" + userKey + "/" + safeWeekKey;
  const String dayBasePath = weekBasePath + "/days/" + safeDayKey;
  const String sessionPath = dayBasePath + "/sessions/" + record.sessionId;
  const String timelinePath = dayBasePath + "/timeline/" + buildTimelineKey(record);
  const String daySummaryPath = dayBasePath + "/daySummary";
  const String weekSummaryPath = weekBasePath + "/weekSummary";
  const bool timelineAlreadyExisted = pathExists(timelinePath);

  // Toda sesion vive dentro de su semana y cada semana contiene dias. Asi la
  // app puede abrir una semana y navegar sus dias sin saltar entre arboles.
  if (!putJson(sessionPath, temp.toAthleteAnalysisJson())) {
    return false;
  }
  if (!putJson(sessionPath + "/rawSession", temp.toJson())) {
    return false;
  }
  if (!putJson(sessionPath + "/setDetails", temp.toSetDetailsJson())) {
    return false;
  }
  bool seenSet[SessionHistoryRecord::kMaxSets + 1] = {false};
  for (uint8_t setNumber = 1; setNumber <= record.setCount && setNumber <= SessionHistoryRecord::kMaxSets; ++setNumber) {
    seenSet[setNumber] = true;
  }
  for (uint16_t i = 0; i < record.repCount; ++i) {
    const uint8_t setNum = record.reps[i].setNumber;
    if (setNum >= 1 && setNum <= SessionHistoryRecord::kMaxSets) {
      seenSet[setNum] = true;
    }
  }
  for (uint8_t setNumber = 1; setNumber <= SessionHistoryRecord::kMaxSets; ++setNumber) {
    if (!seenSet[setNumber]) {
      continue;
    }
    if (!putJson(sessionPath + "/repSets/set" + String(setNumber),
                 temp.toRepSetJson(setNumber))) {
      return false;
    }
  }
  if (!putJson(dayBasePath + "/meta", dailyMetaJson)) {
    return false;
  }
  if (!putJson(timelinePath, timelineJson)) {
    return false;
  }

  // timeline es nuestra huella idempotente: si ya existía, esta sesión ya fue
  // contada en los resúmenes y solo estamos sobrescribiendo detalle por
  // reintento o resync.
  if (!timelineAlreadyExisted) {
    String existingDaySummary;
    if (!getJson(daySummaryPath, existingDaySummary) && lastHttpStatusCode_ != 404) {
      existingDaySummary = "";
    }
    if (!putJson(daySummaryPath, buildAggregateSummaryJson(existingDaySummary, record, false))) {
      return false;
    }

    String existingWeekSummary;
    if (!getJson(weekSummaryPath, existingWeekSummary) && lastHttpStatusCode_ != 404) {
      existingWeekSummary = "";
    }
    if (!putJson(weekSummaryPath, buildAggregateSummaryJson(existingWeekSummary, record, true))) {
      return false;
    }
  }
  return true;
}

String FirebaseService::buildUserProfileJson(const UserProfile& profile) const {
  const uint32_t nowEpoch = getCurrentEpoch();
  const String nowIso = getCurrentIso();
  const String hasBasicDataJson = profile.hasBasicData ? "true" : "false";
  const String ageJson = String(static_cast<unsigned int>(profile.age));
  String json;
  json.reserve(384);
  json += "{";
  json += "\"rfidUid\":\"" + escapeJson(profile.rfidUid) + "\",";
  json += "\"displayName\":\"" + escapeJson(profile.displayName) + "\",";
  json += "\"hasBasicData\":" + hasBasicDataJson + ",";
  json += "\"weightKg\":" + String(profile.weightKg, 2) + ",";
  json += "\"age\":" + ageJson + ",";
  json += "\"heightCm\":" + String(profile.heightCm, 2) + ",";
  json += "\"gender\":\"" + escapeJson(UserRegistry::genderToString(profile.gender)) + "\",";
  json += "\"goal\":\"" + escapeJson(UserRegistry::goalToString(profile.goal)) + "\",";
  json += "\"updatedAtEpoch\":" + String(nowEpoch) + ",";
  json += "\"updatedAtIso\":\"" + escapeJson(nowIso) + "\"";
  json += "}";
  return json;
}

bool FirebaseService::pushUserProfile(const UserProfile& profile) {
  const String path = "usersByRfid/" + profile.rfidUid;
  const String payload = buildUserProfileJson(profile);
  if (useFirebaseClientProfileWrites_) {
    const uint32_t startedAt = millis();
    Serial.printf("[CloudWrite][fc] begin kind=profile_write path=%s payloadBytes=%u heap=%lu internalHeap=%lu largestBlock=%lu\n",
                  path.c_str(),
                  static_cast<unsigned>(payload.length()),
                  static_cast<unsigned long>(ESP.getFreeHeap()),
                  static_cast<unsigned long>(internalFree8BitHeap()),
                  static_cast<unsigned long>(internalLargestFree8BitBlock()));
    const bool ok = putPathJsonQueued(path, payload);
    const uint32_t durationMs = millis() - startedAt;
    Serial.printf("[CloudWrite][fc] end kind=profile_write ok=%d durationMs=%lu error=%s heap=%lu internalHeap=%lu largestBlock=%lu\n",
                  ok ? 1 : 0,
                  static_cast<unsigned long>(durationMs),
                  lastErrorSummary_.c_str(),
                  static_cast<unsigned long>(ESP.getFreeHeap()),
                  static_cast<unsigned long>(internalFree8BitHeap()),
                  static_cast<unsigned long>(internalLargestFree8BitBlock()));
    return ok;
  }
  Serial.printf("[CloudLegacy] begin kind=profile_write method=PUT path=%s payloadBytes=%u\n",
                path.c_str(),
                static_cast<unsigned>(payload.length()));
  const bool ok = putJson(path, payload);
  Serial.printf("[CloudLegacy] end kind=profile_write ok=%d method=PUT path=%s http=%d error=%s\n",
                ok ? 1 : 0,
                path.c_str(),
                lastHttpStatusCode_,
                lastErrorSummary_.c_str());
  return ok;
}

String FirebaseService::buildCalibrationJson(const UserMachineCalibration& calibration) const {
  const uint32_t nowEpoch = getCurrentEpoch();
  const String nowIso = getCurrentIso();
  String json;
  json.reserve(2200);
  json += "{";
  json += "\"schemaVersion\":" + String(calibration.schemaVersion > 0 ? calibration.schemaVersion : 1) + ",";
  json += "\"source\":\"" +
          escapeJson(calibration.source.isEmpty() ? String("unified_velocity_calibration_v1")
                                                  : calibration.source) +
          "\",";
  json += "\"userUid\":\"" + escapeJson(calibration.userUid) + "\",";
  json += "\"machineTypeId\":\"" + escapeJson(calibration.machineTypeId) + "\",";
  json += "\"goalId\":\"" + escapeJson(calibration.goalId) + "\",";
  json += "\"hasCalibration\":" + String(calibration.hasCalibration ? "true" : "false") + ",";
  json += "\"suggestedWeightKg\":" + String(calibration.suggestedWeightKg, 2) + ",";
  json += "\"userRomPercent\":" + String(calibration.userRomPercent, 2) + ",";
  json += "\"userBottomPct\":" + String(calibration.userBottomPct, 2) + ",";
  json += "\"userTopPct\":" + String(calibration.userTopPct, 2) + ",";
  json += "\"romBottomRaw\":" + String(calibration.romBottomRaw, 3) + ",";
  json += "\"romTopRaw\":" + String(calibration.romTopRaw, 3) + ",";
  json += "\"romRangeRaw\":" + String(calibration.romRangeRaw, 3) + ",";
  json += "\"romRangePct\":" + String(calibration.romRangePct, 3) + ",";
  json += "\"romMeters\":" + String(calibration.romMeters, 4) + ",";
  json += "\"romValid\":" + String(calibration.romValid ? "true" : "false") + ",";
  json += "\"machineIncrementKg\":" + String(calibration.machineIncrementKg, 2) + ",";
  json += "\"machineMinKg\":" + String(calibration.machineMinKg, 2) + ",";
  json += "\"machineMaxKg\":" + String(calibration.machineMaxKg, 2) + ",";
  json += "\"startWeightSource\":\"" + escapeJson(calibration.startWeightSource) + "\",";
  json += "\"suggestedStartWeightKg\":" + String(calibration.suggestedStartWeightKg, 2) + ",";
  json += "\"actualFirstSetWeightKg\":" + String(calibration.actualFirstSetWeightKg, 2) + ",";
  json += "\"userOverrodeStartWeight\":" + String(calibration.userOverrodeStartWeight ? "true" : "false") + ",";
  json += "\"resultRecommendedWeightKg\":" + String(calibration.resultRecommendedWeightKg, 2) + ",";
  json += "\"resultConfidence\":\"" +
          escapeJson(UserRegistry::calibrationConfidenceToString(calibration.resultConfidence)) + "\",";
  json += "\"resultAction\":\"" +
          escapeJson(UserRegistry::calibrationActionToString(calibration.resultAction)) + "\",";
  json += "\"resultReasonText\":\"" + escapeJson(calibration.resultReasonText) + "\",";
  json += "\"estimatedOneRepMaxKg\":" + String(calibration.estimatedOneRepMaxKg, 2) + ",";
  json += "\"estimatedOneRepMaxConfidence\":\"" + escapeJson(calibration.estimatedOneRepMaxConfidence) + "\",";
  json += "\"velocityLoadSlope\":" + String(calibration.velocityLoadSlope, 6) + ",";
  json += "\"velocityLoadIntercept\":" + String(calibration.velocityLoadIntercept, 3) + ",";
  json += "\"calibrationModelValid\":" + String(calibration.calibrationModelValid ? "true" : "false") + ",";
  json += "\"strengthRecommendedKg\":" + String(calibration.strengthRecommendedKg, 2) + ",";
  json += "\"hypertrophyRecommendedKg\":" + String(calibration.hypertrophyRecommendedKg, 2) + ",";
  json += "\"enduranceRecommendedKg\":" + String(calibration.enduranceRecommendedKg, 2) + ",";
  json += "\"activeGoalRecommendedKg\":" + String(calibration.activeGoalRecommendedKg, 2) + ",";
  json += "\"activeGoalId\":\"" + escapeJson(calibration.activeGoalId) + "\",";
  json += "\"nextRecommendedWeightKg\":" + String(calibration.nextRecommendedWeightKg, 2) + ",";
  json += "\"nextRecommendationSource\":\"" + escapeJson(calibration.nextRecommendationSource) + "\",";
  json += "\"nextRecommendationReason\":\"" + escapeJson(calibration.nextRecommendationReason) + "\",";
  json += "\"nextRecommendationUpdatedAt\":" + String(calibration.nextRecommendationUpdatedAt) + ",";
  json += "\"motionTargetsUsed\":{";
  json += "\"targetRepsMin\":" + String(calibration.motionTargetsUsed.targetRepsMin) + ",";
  json += "\"targetRepsMax\":" + String(calibration.motionTargetsUsed.targetRepsMax) + ",";
  json += "\"targetSetsMin\":" + String(calibration.motionTargetsUsed.targetSetsMin) + ",";
  json += "\"targetSetsMax\":" + String(calibration.motionTargetsUsed.targetSetsMax) + ",";
  json += "\"targetSetsDefault\":" + String(calibration.motionTargetsUsed.targetSetsDefault) + ",";
  json += "\"restSecondsDefault\":" + String(calibration.motionTargetsUsed.restSecondsDefault) + ",";
  json += "\"riseTimeSecMin\":" + String(calibration.motionTargetsUsed.riseTimeSecMin, 3) + ",";
  json += "\"riseTimeSecMax\":" + String(calibration.motionTargetsUsed.riseTimeSecMax, 3) + ",";
  json += "\"riseTimeSecDefault\":" + String(calibration.motionTargetsUsed.riseTimeSecDefault, 3) + ",";
  json += "\"lowerTimeSecMin\":" + String(calibration.motionTargetsUsed.lowerTimeSecMin, 3) + ",";
  json += "\"lowerTimeSecMax\":" + String(calibration.motionTargetsUsed.lowerTimeSecMax, 3) + ",";
  json += "\"lowerTimeSecDefault\":" + String(calibration.motionTargetsUsed.lowerTimeSecDefault, 3) + ",";
  json += "\"topPauseSec\":" + String(calibration.motionTargetsUsed.topPauseSec, 3) + ",";
  json += "\"bottomPauseSec\":" + String(calibration.motionTargetsUsed.bottomPauseSec, 3);
  json += "},";
  json += "\"calibrationSets\":[";
  for (uint8_t i = 0; i < calibration.calibrationSetCount &&
                      i < UserMachineCalibration::kMaxCalibrationSets;
       ++i) {
    if (i > 0) {
      json += ",";
    }
    const CalibrationSetSnapshot& set = calibration.calibrationSets[i];
    json += "{";
    json += "\"setIndex\":" + String(set.setIndex) + ",";
    json += "\"selectedWeightKg\":" + String(set.selectedWeightKg, 2) + ",";
    json += "\"targetRepCount\":" + String(set.targetRepCount) + ",";
    json += "\"validRepCount\":" + String(set.validRepCount) + ",";
    json += "\"rejectedRepCount\":" + String(set.rejectedRepCount) + ",";
    json += "\"avgConcentricVelocityPctPerSec\":" + String(set.avgConcentricVelocityPctPerSec, 3) + ",";
    json += "\"medianConcentricVelocityPctPerSec\":" + String(set.medianConcentricVelocityPctPerSec, 3) + ",";
    json += "\"avgConcentricDurationMs\":" + String(set.avgConcentricDurationMs, 2) + ",";
    json += "\"avgRomPercent\":" + String(set.avgRomPercent, 2) + ",";
    json += "\"velocityStdDevPctPerSec\":" + String(set.velocityStdDevPctPerSec, 3) + ",";
    json += "\"qualityScore\":" + String(set.qualityScore, 2) + ",";
    json += "\"classification\":\"" +
            escapeJson(UserRegistry::calibrationSetClassificationToString(set.classification)) + "\",";
    json += "\"suggestedNextWeightKg\":" + String(set.suggestedNextWeightKg, 2) + ",";
    json += "\"reasonText\":\"" + escapeJson(set.reasonText) + "\"";
    json += "}";
  }
  json += "],";
  json += "\"updatedAtEpoch\":" + String(nowEpoch) + ",";
  json += "\"updatedAtIso\":\"" + escapeJson(nowIso) + "\"";
  json += "}";
  return json;
}

bool FirebaseService::pushCalibration(const String& userUid, const UserMachineCalibration& calibration) {
  const String json = buildCalibrationJson(calibration);
  const String rootPath = "calibrations/" + userUid + "/" + calibration.machineTypeId;
  const String historyPath = "calibrationHistory/" + userUid + "/" + calibration.machineTypeId + "/" +
                             String(getCurrentEpoch());
  const bool useFirebaseClientWrites =
      useFirebaseClientCloudReads_ || useFirebaseClientDeviceWrites_ || useFirebaseClientProfileWrites_;
  const bool rootOk = useFirebaseClientWrites ? putPathJsonQueued(rootPath, json) : putJson(rootPath, json);
  if (!rootOk) {
    return false;
  }
  return useFirebaseClientWrites ? putPathJsonQueued(historyPath, json) : putJson(historyPath, json);
}

String FirebaseService::buildMachineConfigJson(const MachineCloudConfig& cloudConfig) const {
  String json;
  json.reserve(1536);
  json += "{";
  json += "\"machineId\":\"" + escapeJson(cloudConfig.machineId) + "\",";
  json += "\"exerciseCategory\":\"" + escapeJson(cloudConfig.exerciseCategory) + "\",";
  json += "\"primaryMuscleGroup\":\"" + escapeJson(cloudConfig.primaryMuscleGroup) + "\",";
  json += "\"secondaryMuscleGroup\":\"" + escapeJson(cloudConfig.secondaryMuscleGroup) + "\",";
  json += "\"version\":" + String(cloudConfig.version) + ",";
  json += "\"updatedAtEpoch\":" + String(cloudConfig.updatedAtEpoch) + ",";
  json += "\"strokeLengthMm\":" + String(cloudConfig.strokeLengthMm, 2) + ",";
  json += "\"idealRomPercent\":" + String(cloudConfig.idealRomPercent, 2) + ",";
  json += "\"defaultCalibrationWeightKg\":" + String(cloudConfig.defaultCalibrationWeightKg, 2) + ",";
  json += "\"targetRepsPerSet\":" + String(cloudConfig.targetRepsPerSet) + ",";
  json += "\"strengthWeightFactor\":" + String(cloudConfig.recommendations[0].weightFactor, 2) + ",";
  json += "\"strengthRepsMin\":" + String(cloudConfig.recommendations[0].repsMin) + ",";
  json += "\"strengthRepsMax\":" + String(cloudConfig.recommendations[0].repsMax) + ",";
  json += "\"strengthTargetSets\":" + String(cloudConfig.recommendations[0].targetSets) + ",";
  json += "\"strengthRestSeconds\":" + String(cloudConfig.recommendations[0].restSeconds) + ",";
  json += "\"strengthRiseMs\":" + String(cloudConfig.recommendations[0].riseMs) + ",";
  json += "\"strengthLowerMs\":" + String(cloudConfig.recommendations[0].lowerMs) + ",";
  json += "\"strengthTopPauseMs\":" + String(cloudConfig.recommendations[0].topPauseMs) + ",";
  json += "\"strengthBottomPauseMs\":" + String(cloudConfig.recommendations[0].bottomPauseMs) + ",";
  json += "\"hypertrophyWeightFactor\":" + String(cloudConfig.recommendations[1].weightFactor, 2) + ",";
  json += "\"hypertrophyRepsMin\":" + String(cloudConfig.recommendations[1].repsMin) + ",";
  json += "\"hypertrophyRepsMax\":" + String(cloudConfig.recommendations[1].repsMax) + ",";
  json += "\"hypertrophyTargetSets\":" + String(cloudConfig.recommendations[1].targetSets) + ",";
  json += "\"hypertrophyRestSeconds\":" + String(cloudConfig.recommendations[1].restSeconds) + ",";
  json += "\"hypertrophyRiseMs\":" + String(cloudConfig.recommendations[1].riseMs) + ",";
  json += "\"hypertrophyLowerMs\":" + String(cloudConfig.recommendations[1].lowerMs) + ",";
  json += "\"hypertrophyTopPauseMs\":" + String(cloudConfig.recommendations[1].topPauseMs) + ",";
  json += "\"hypertrophyBottomPauseMs\":" + String(cloudConfig.recommendations[1].bottomPauseMs) + ",";
  json += "\"enduranceWeightFactor\":" + String(cloudConfig.recommendations[2].weightFactor, 2) + ",";
  json += "\"enduranceRepsMin\":" + String(cloudConfig.recommendations[2].repsMin) + ",";
  json += "\"enduranceRepsMax\":" + String(cloudConfig.recommendations[2].repsMax) + ",";
  json += "\"enduranceTargetSets\":" + String(cloudConfig.recommendations[2].targetSets) + ",";
  json += "\"enduranceRestSeconds\":" + String(cloudConfig.recommendations[2].restSeconds) + ",";
  json += "\"enduranceRiseMs\":" + String(cloudConfig.recommendations[2].riseMs) + ",";
  json += "\"enduranceLowerMs\":" + String(cloudConfig.recommendations[2].lowerMs) + ",";
  json += "\"enduranceTopPauseMs\":" + String(cloudConfig.recommendations[2].topPauseMs) + ",";
  json += "\"enduranceBottomPauseMs\":" + String(cloudConfig.recommendations[2].bottomPauseMs) + ",";
  json += "\"generalWeightFactor\":" + String(cloudConfig.recommendations[3].weightFactor, 2) + ",";
  json += "\"generalRepsMin\":" + String(cloudConfig.recommendations[3].repsMin) + ",";
  json += "\"generalRepsMax\":" + String(cloudConfig.recommendations[3].repsMax) + ",";
  json += "\"generalTargetSets\":" + String(cloudConfig.recommendations[3].targetSets) + ",";
  json += "\"generalRestSeconds\":" + String(cloudConfig.recommendations[3].restSeconds) + ",";
  json += "\"generalRiseMs\":" + String(cloudConfig.recommendations[3].riseMs) + ",";
  json += "\"generalLowerMs\":" + String(cloudConfig.recommendations[3].lowerMs) + ",";
  json += "\"generalTopPauseMs\":" + String(cloudConfig.recommendations[3].topPauseMs) + ",";
  json += "\"generalBottomPauseMs\":" + String(cloudConfig.recommendations[3].bottomPauseMs) + ",";
  json += "\"testWeightFactor\":" + String(cloudConfig.recommendations[4].weightFactor, 2) + ",";
  json += "\"testRepsMin\":" + String(cloudConfig.recommendations[4].repsMin) + ",";
  json += "\"testRepsMax\":" + String(cloudConfig.recommendations[4].repsMax) + ",";
  json += "\"testTargetSets\":" + String(cloudConfig.recommendations[4].targetSets) + ",";
  json += "\"testRestSeconds\":" + String(cloudConfig.recommendations[4].restSeconds) + ",";
  json += "\"testRiseMs\":" + String(cloudConfig.recommendations[4].riseMs) + ",";
  json += "\"testLowerMs\":" + String(cloudConfig.recommendations[4].lowerMs) + ",";
  json += "\"testTopPauseMs\":" + String(cloudConfig.recommendations[4].topPauseMs) + ",";
  json += "\"testBottomPauseMs\":" + String(cloudConfig.recommendations[4].bottomPauseMs);
  json += "}";
  return json;
}

bool FirebaseService::pushMachineConfig(const MachineCloudConfig& cloudConfig) {
  return !cloudConfig.machineId.isEmpty() &&
         putJson("machineConfigs/" + cloudConfig.machineId, buildMachineConfigJson(cloudConfig));
}

bool FirebaseService::pushDeviceHeartbeat(const String& machineId, const String& machineTypeId,
                                          const String& machineDisplayName, const String& appState,
                                          const String& activeUserUid, bool encoderCalibrationValid,
                                          uint32_t encoderZeroRaw, uint32_t encoderFullRaw,
                                          float encoderReferenceDistanceMm,
                                          bool encoderInvertDirection) {
  const uint32_t nowEpoch = getCurrentEpoch();
  const String nowIso = getCurrentIso();
  String deviceId = WiFi.macAddress();
  deviceId.replace(":", "-");

  String json;
  json.reserve(512);
  json += "{";
  json += "\"deviceId\":\"" + escapeJson(deviceId) + "\",";
  json += "\"macAddress\":\"" + escapeJson(WiFi.macAddress()) + "\",";
  json += "\"ipAddress\":\"" + escapeJson(WiFi.localIP().toString()) + "\",";
  json += "\"machineId\":\"" + escapeJson(machineId) + "\",";
  json += "\"machineTypeId\":\"" + escapeJson(machineTypeId) + "\",";
  json += "\"machineDisplayName\":\"" + escapeJson(machineDisplayName) + "\",";
  json += "\"appState\":\"" + escapeJson(appState) + "\",";
  json += "\"activeUserUid\":\"" + escapeJson(activeUserUid) + "\",";
  json += "\"encoderCalibrationValid\":" + String(encoderCalibrationValid ? "true" : "false") + ",";
  json += "\"encoderZeroRaw\":" + String(encoderZeroRaw) + ",";
  json += "\"encoderFullRaw\":" + String(encoderFullRaw) + ",";
  json += "\"encoderReferenceDistanceMm\":" + String(encoderReferenceDistanceMm, 2) + ",";
  json += "\"encoderInvertDirection\":" + String(encoderInvertDirection ? "true" : "false") + ",";
  json += "\"wifiConnected\":" + String(isWifiConnected() ? "true" : "false") + ",";
  json += "\"updatedAtEpoch\":" + String(nowEpoch) + ",";
  json += "\"updatedAtIso\":\"" + escapeJson(nowIso) + "\"";
  json += "}";
  const String path = "devices/" + deviceId;
  if (useFirebaseClientDeviceWrites_) {
    const uint32_t startedAt = millis();
    Serial.printf("[CloudWrite][fc] begin kind=device_heartbeat path=%s payloadBytes=%u heap=%lu internalHeap=%lu largestBlock=%lu\n",
                  path.c_str(),
                  static_cast<unsigned>(json.length()),
                  static_cast<unsigned long>(ESP.getFreeHeap()),
                  static_cast<unsigned long>(internalFree8BitHeap()),
                  static_cast<unsigned long>(internalLargestFree8BitBlock()));
    const bool ok = putPathJsonQueued(path, json);
    const uint32_t durationMs = millis() - startedAt;
    Serial.printf("[CloudWrite][fc] end kind=device_heartbeat ok=%d durationMs=%lu error=%s heap=%lu internalHeap=%lu largestBlock=%lu\n",
                  ok ? 1 : 0,
                  static_cast<unsigned long>(durationMs),
                  lastErrorSummary_.c_str(),
                  static_cast<unsigned long>(ESP.getFreeHeap()),
                  static_cast<unsigned long>(internalFree8BitHeap()),
                  static_cast<unsigned long>(internalLargestFree8BitBlock()));
    return ok;
  }

  Serial.printf("[CloudLegacy] begin kind=device_heartbeat method=PUT path=%s payloadBytes=%u\n",
                path.c_str(),
                static_cast<unsigned>(json.length()));
  const bool ok = putJson(path, json);
  Serial.printf("[CloudLegacy] end kind=device_heartbeat ok=%d method=PUT path=%s http=%d error=%s\n",
                ok ? 1 : 0,
                path.c_str(),
                lastHttpStatusCode_,
                lastErrorSummary_.c_str());
  return ok;
}

bool FirebaseService::fetchDeviceCalibration(const String& deviceId, String& json) {
  if (deviceId.isEmpty()) {
    return false;
  }
  const String path = "devices/" + deviceId;
  const bool ok = useFirebaseClientCloudReads_
                      ? getJsonFirebaseClientLogged("boot_restore", path, json)
                      : getJsonOldReadLogged("boot_restore", path, json);
  if (!ok || json == "null") {
    return false;
  }
  return true;
}

bool FirebaseService::fetchUserProfile(const String& userUid, UserProfile& profile) {
  String json;
  const String path = "usersByRfid/" + userUid;
  const bool ok = useFirebaseClientCloudReads_
                      ? getJsonFirebaseClientLogged("profile", path, json)
                      : getJsonOldReadLogged("profile", path, json);
  if (!ok || json == "null") {
    return false;
  }
  return parseUserProfileFromJson(userUid, json, profile);
}

bool FirebaseService::fetchUserProfileFirebaseClient(const String& userUid, UserProfile& profile) {
  String json;
  if (!getJsonFirebaseClientLogged("profile", "usersByRfid/" + userUid, json) || json == "null") {
    return false;
  }
  return parseUserProfileFromJson(userUid, json, profile);
}

bool FirebaseService::fetchCalibration(const String& userUid, const String& machineTypeId,
                                       UserMachineCalibration& calibration) {
  String json;
  const String path = "calibrations/" + userUid + "/" + machineTypeId;
  const bool ok = useFirebaseClientCloudReads_
                      ? getJsonFirebaseClientLogged("calibration", path, json)
                      : getJsonOldReadLogged("calibration", path, json);
  if (!ok || json == "null") {
    return false;
  }

  resetUserMachineCalibration(calibration);
  calibration.machineTypeId = machineTypeId;
  calibration.userUid = userUid;
  calibration.source = "unified_velocity_calibration_v1";
  uint32_t schemaVersion = 0;
  if (jsonUIntValue(json, "schemaVersion", schemaVersion)) {
    calibration.schemaVersion = static_cast<uint16_t>(schemaVersion);
  }
  jsonStringValue(json, "source", calibration.source);
  jsonStringValue(json, "goalId", calibration.goalId);
  jsonBoolValue(json, "hasCalibration", calibration.hasCalibration);
  jsonFloatValue(json, "suggestedWeightKg", calibration.suggestedWeightKg);
  jsonFloatValue(json, "userRomPercent", calibration.userRomPercent);
  jsonFloatValue(json, "userBottomPct", calibration.userBottomPct);
  jsonFloatValue(json, "userTopPct", calibration.userTopPct);
  jsonFloatValue(json, "romBottomRaw", calibration.romBottomRaw);
  jsonFloatValue(json, "romTopRaw", calibration.romTopRaw);
  jsonFloatValue(json, "romRangeRaw", calibration.romRangeRaw);
  jsonFloatValue(json, "romRangePct", calibration.romRangePct);
  jsonFloatValue(json, "romMeters", calibration.romMeters);
  jsonBoolValue(json, "romValid", calibration.romValid);
  jsonFloatValue(json, "machineIncrementKg", calibration.machineIncrementKg);
  jsonFloatValue(json, "machineMinKg", calibration.machineMinKg);
  jsonFloatValue(json, "machineMaxKg", calibration.machineMaxKg);
  jsonStringValue(json, "startWeightSource", calibration.startWeightSource);
  jsonFloatValue(json, "suggestedStartWeightKg", calibration.suggestedStartWeightKg);
  jsonFloatValue(json, "actualFirstSetWeightKg", calibration.actualFirstSetWeightKg);
  jsonBoolValue(json, "userOverrodeStartWeight", calibration.userOverrodeStartWeight);
  jsonFloatValue(json, "resultRecommendedWeightKg", calibration.resultRecommendedWeightKg);
  String confidence;
  if (jsonStringValue(json, "resultConfidence", confidence)) {
    calibration.resultConfidence = UserRegistry::calibrationConfidenceFromString(confidence);
  }
  String action;
  if (jsonStringValue(json, "resultAction", action)) {
    calibration.resultAction = UserRegistry::calibrationActionFromString(action);
  }
  jsonStringValue(json, "resultReasonText", calibration.resultReasonText);
  jsonFloatValue(json, "estimatedOneRepMaxKg", calibration.estimatedOneRepMaxKg);
  jsonStringValue(json, "estimatedOneRepMaxConfidence", calibration.estimatedOneRepMaxConfidence);
  jsonFloatValue(json, "velocityLoadSlope", calibration.velocityLoadSlope);
  jsonFloatValue(json, "velocityLoadIntercept", calibration.velocityLoadIntercept);
  jsonBoolValue(json, "calibrationModelValid", calibration.calibrationModelValid);
  jsonFloatValue(json, "strengthRecommendedKg", calibration.strengthRecommendedKg);
  jsonFloatValue(json, "hypertrophyRecommendedKg", calibration.hypertrophyRecommendedKg);
  jsonFloatValue(json, "enduranceRecommendedKg", calibration.enduranceRecommendedKg);
  jsonFloatValue(json, "activeGoalRecommendedKg", calibration.activeGoalRecommendedKg);
  jsonStringValue(json, "activeGoalId", calibration.activeGoalId);
  jsonFloatValue(json, "nextRecommendedWeightKg", calibration.nextRecommendedWeightKg);
  jsonStringValue(json, "nextRecommendationSource", calibration.nextRecommendationSource);
  jsonStringValue(json, "nextRecommendationReason", calibration.nextRecommendationReason);
  jsonUIntValue(json, "nextRecommendationUpdatedAt", calibration.nextRecommendationUpdatedAt);
  if (calibration.userRomPercent <= 0.0f && calibration.userTopPct > calibration.userBottomPct) {
    calibration.userRomPercent = calibration.userTopPct - calibration.userBottomPct;
  }
  if (calibration.userTopPct <= calibration.userBottomPct) {
    calibration.userBottomPct = 0.0f;
    calibration.userTopPct = calibration.userRomPercent;
  }
  if (calibration.nextRecommendedWeightKg <= 0.0f) {
    calibration.nextRecommendedWeightKg = calibration.suggestedWeightKg;
  }
  if (calibration.resultRecommendedWeightKg <= 0.0f) {
    calibration.resultRecommendedWeightKg = calibration.suggestedWeightKg;
  }
  jsonUIntValue(json, "updatedAtEpoch", calibration.updatedAtEpoch);
  jsonStringValue(json, "updatedAtIso", calibration.updatedAtIso);
  if (calibration.nextRecommendationUpdatedAt == 0 &&
      calibration.nextRecommendationSource.equalsIgnoreCase("session_summary")) {
    calibration.nextRecommendationUpdatedAt = calibration.updatedAtEpoch;
  }
  return calibration.hasCalibration;
}

bool FirebaseService::fetchMachineConfig(const String& machineId, MachineCloudConfig& cloudConfig) {
  String json;
  const String path = "machineConfigs/" + machineId;
  const bool ok = useFirebaseClientCloudReads_
                      ? getJsonFirebaseClientLogged("machine_config", path, json)
                      : getJsonOldReadLogged("machine_config", path, json);
  if (!ok || json == "null") {
    return false;
  }

  // El esquema de machineConfigs es plano a proposito. En ESP32 es mucho mas
  // robusto parsear unas cuantas llaves conocidas que depender de un parser
  // JSON grande solo para un polling de configuracion.
  cloudConfig = MachineCloudConfig{};
  cloudConfig.machineId = machineId;
  cloudConfig.valid = true;
  jsonStringValue(json, "exerciseCategory", cloudConfig.exerciseCategory);
  jsonStringValue(json, "primaryMuscleGroup", cloudConfig.primaryMuscleGroup);
  jsonStringValue(json, "secondaryMuscleGroup", cloudConfig.secondaryMuscleGroup);
  jsonUIntValue(json, "version", cloudConfig.version);
  jsonUIntValue(json, "updatedAtEpoch", cloudConfig.updatedAtEpoch);
  jsonFloatValue(json, "strokeLengthMm", cloudConfig.strokeLengthMm);
  jsonFloatValue(json, "idealRomPercent", cloudConfig.idealRomPercent);
  jsonFloatValue(json, "defaultCalibrationWeightKg", cloudConfig.defaultCalibrationWeightKg);
  uint32_t temp = 0;
  if (jsonUIntValue(json, "targetRepsPerSet", temp)) {
    cloudConfig.targetRepsPerSet = static_cast<uint8_t>(temp);
  }

  setRecommendationFields(json, "strength", cloudConfig.recommendations[0], TrainingGoal::Strength);
  setRecommendationFields(json, "hypertrophy", cloudConfig.recommendations[1], TrainingGoal::Hypertrophy);
  setRecommendationFields(json, "endurance", cloudConfig.recommendations[2], TrainingGoal::Endurance);
  setRecommendationFields(json, "general", cloudConfig.recommendations[3], TrainingGoal::General);
  setRecommendationFields(json, "test", cloudConfig.recommendations[4], TrainingGoal::Test);
  return true;
}

bool FirebaseService::fetchPathJson(const String& path, String& response) {
  if (useFirebaseClientCloudReads_) {
    return getJsonFirebaseClientLogged("path_json", path, response);
  }
  return getJsonOldReadLogged("path_json", path, response);
}

String FirebaseService::escapeJson(const String& input) {
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

String FirebaseService::buildResponseSnippet(const String& response) {
  String snippet = response;
  snippet.trim();
  snippet.replace('\n', ' ');
  snippet.replace('\r', ' ');
  if (snippet.length() > 96) {
    snippet = snippet.substring(0, 96) + "...";
  }
  return snippet;
}

bool FirebaseService::jsonStringValue(const String& json, const char* key, String& outValue) {
  const String needle = "\"" + String(key) + "\":\"";
  const int start = json.indexOf(needle);
  if (start < 0) {
    return false;
  }
  const int valueStart = start + needle.length();
  const int valueEnd = json.indexOf('"', valueStart);
  if (valueEnd < 0) {
    return false;
  }
  outValue = json.substring(valueStart, valueEnd);
  return true;
}

bool FirebaseService::jsonFloatValue(const String& json, const char* key, float& outValue) {
  const String needle = "\"" + String(key) + "\":";
  const int start = json.indexOf(needle);
  if (start < 0) {
    return false;
  }
  const int valueStart = start + needle.length();
  int valueEnd = valueStart;
  while (valueEnd < json.length()) {
    const char c = json.charAt(valueEnd);
    if ((c < '0' || c > '9') && c != '.' && c != '-') {
      break;
    }
    valueEnd++;
  }
  outValue = json.substring(valueStart, valueEnd).toFloat();
  return true;
}

bool FirebaseService::jsonUIntValue(const String& json, const char* key, uint32_t& outValue) {
  const String needle = "\"" + String(key) + "\":";
  const int start = json.indexOf(needle);
  if (start < 0) {
    return false;
  }
  const int valueStart = start + needle.length();
  int valueEnd = valueStart;
  while (valueEnd < json.length()) {
    const char c = json.charAt(valueEnd);
    if (c < '0' || c > '9') {
      break;
    }
    valueEnd++;
  }
  outValue = static_cast<uint32_t>(json.substring(valueStart, valueEnd).toInt());
  return true;
}

bool FirebaseService::jsonBoolValue(const String& json, const char* key, bool& outValue) {
  const String needle = "\"" + String(key) + "\":";
  const int start = json.indexOf(needle);
  if (start < 0) {
    return false;
  }
  const int valueStart = start + needle.length();
  if (json.startsWith("true", valueStart)) {
    outValue = true;
    return true;
  }
  if (json.startsWith("false", valueStart)) {
    outValue = false;
    return true;
  }
  return false;
}
