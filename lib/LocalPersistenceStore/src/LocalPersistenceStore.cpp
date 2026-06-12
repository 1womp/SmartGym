#include "LocalPersistenceStore.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Implementacion compacta de cache/cola en NVS. Esta capa intenta fallar de
// forma conservadora cuando el espacio no alcanza, en vez de forzar writes
// grandes y terminar con registros corruptos o incompletos.
namespace {
SemaphoreHandle_t uploadQueueMutex() {
  static SemaphoreHandle_t mutex = nullptr;
  static portMUX_TYPE createMux = portMUX_INITIALIZER_UNLOCKED;
  if (mutex == nullptr) {
    taskENTER_CRITICAL(&createMux);
    if (mutex == nullptr) {
      mutex = xSemaphoreCreateMutex();
    }
    taskEXIT_CRITICAL(&createMux);
  }
  return mutex;
}

class UploadQueueLock {
 public:
  UploadQueueLock() {
    mutex_ = uploadQueueMutex();
    if (mutex_ != nullptr) {
      locked_ = xSemaphoreTake(mutex_, pdMS_TO_TICKS(100)) == pdTRUE;
    }
  }
  ~UploadQueueLock() {
    if (mutex_ != nullptr && locked_) {
      xSemaphoreGive(mutex_);
    }
  }
  bool locked() const {
    return locked_;
  }

 private:
  SemaphoreHandle_t mutex_ = nullptr;
  bool locked_ = false;
};

TrainingGoal goalFromInt(int raw) {
  switch (raw) {
    case 1:
      return TrainingGoal::Strength;
    case 2:
      return TrainingGoal::Hypertrophy;
    case 3:
      return TrainingGoal::Endurance;
    case 4:
      return TrainingGoal::Test;
    case 0:
    default:
      return TrainingGoal::General;
  }
}

bool jsonReadString(const String& json, const char* key, String& out) {
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
  out = json.substring(valueStart, valueEnd);
  return true;
}

bool jsonReadFloat(const String& json, const char* key, float& out) {
  const String needle = "\"" + String(key) + "\":";
  const int start = json.indexOf(needle);
  if (start < 0) {
    return false;
  }
  const int valueStart = start + needle.length();
  int valueEnd = valueStart;
  while (valueEnd < static_cast<int>(json.length())) {
    const char c = json.charAt(valueEnd);
    if ((c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+') {
      valueEnd++;
      continue;
    }
    break;
  }
  if (valueEnd <= valueStart) {
    return false;
  }
  out = json.substring(valueStart, valueEnd).toFloat();
  return true;
}

bool jsonReadUInt(const String& json, const char* key, uint32_t& out) {
  float value = 0.0f;
  if (!jsonReadFloat(json, key, value)) {
    return false;
  }
  if (value < 0.0f) {
    value = 0.0f;
  }
  out = static_cast<uint32_t>(value);
  return true;
}
}

bool LocalPersistenceStore::begin() const {
  // Abrimos una vez cada namespace en modo escritura para que NVS cree las
  // entradas si aun no existen. Asi evitamos warnings NOT_FOUND cuando luego
  // consultamos contadores en modo solo lectura durante el arranque.
  Preferences preferences;
  if (!preferences.begin(kUsersNamespace, false)) {
    return false;
  }
  preferences.end();

  if (!preferences.begin(kSessionsNamespace, false)) {
    return false;
  }
  preferences.end();

  if (!preferences.begin(kUploadsNamespace, false)) {
    return false;
  }
  preferences.end();
  return true;
}

bool LocalPersistenceStore::loadUsers(UserRegistry& registry) const {
  Preferences preferences;
  if (!preferences.begin(kUsersNamespace, true)) {
    return false;
  }

  const uint8_t count = preferences.getUChar(kUserCountKey, 0);
  if (count == 0) {
    preferences.end();
    return false;
  }

  registry.clear();
  bool loadedAny = false;
  for (uint8_t i = 0; i < count; ++i) {
    UserProfile profile;
    const String key = profileKey(i);
    if (!preferences.isKey(key.c_str())) {
      continue;
    }
    const String raw = preferences.getString(key.c_str(), "");
    if (deserializeProfile(raw, profile)) {
      registry.upsertProfile(profile);
      loadedAny = true;
    }
  }

  preferences.end();
  return loadedAny;
}

bool LocalPersistenceStore::saveUsers(const UserRegistry& registry) const {
  Preferences preferences;
  if (!preferences.begin(kUsersNamespace, false)) {
    return false;
  }

  preferences.clear();
  const uint8_t count = registry.getProfileCount();
  preferences.putUChar(kUserCountKey, count);
  for (uint8_t i = 0; i < count; ++i) {
    const UserProfile* profile = registry.getProfileAt(i);
    if (profile == nullptr) {
      continue;
    }
    preferences.putString(profileKey(i).c_str(), serializeProfile(*profile));
  }

  preferences.end();
  return true;
}

bool LocalPersistenceStore::appendSession(const SessionHistoryRecord& record) const {
  Preferences preferences;
  if (!preferences.begin(kSessionsNamespace, false)) {
    return false;
  }

  const uint8_t head = preferences.getUChar(kSessionHeadKey, 0);
  const uint8_t count = preferences.getUChar(kSessionCountKey, 0);
  const uint8_t slot = head % kMaxStoredSessions;

  SessionRecorder temp;
  temp.loadRecord(record);
  String json = temp.toJson();
  // La sesion rica completa se usa para Firebase. En NVS guardamos la version
  // completa solo si cabe con margen; si no, persistimos un resumen compacto
  // para evitar crashes o corrupciones por cadenas gigantes.
  if (json.isEmpty() || json.length() > kMaxSessionJsonLength) {
    json = serializeSessionSummary(record);
  }

  const bool ok = !json.isEmpty() &&
                  preferences.putString(sessionKey(slot).c_str(), json) > 0;
  preferences.putUChar(kSessionHeadKey, (slot + 1) % kMaxStoredSessions);
  preferences.putUChar(kSessionCountKey, min<uint8_t>(count + 1, kMaxStoredSessions));
  preferences.end();
  return ok;
}

bool LocalPersistenceStore::enqueueUpload(const String& path, const String& payload) const {
  return enqueueUpload(path, payload, "PUT");
}

bool LocalPersistenceStore::enqueueUpload(const String& path,
                                          const String& payload,
                                          const String& method) const {
  UploadQueueLock queueLock;
  if (!queueLock.locked()) {
    Serial.printf("[Sync] enqueue failed stage=queue_lock pathLen=%u payloadBytes=%u\n",
                  static_cast<unsigned>(path.length()),
                  static_cast<unsigned>(payload.length()));
    return false;
  }
  if (path.isEmpty() || payload.isEmpty()) {
    Serial.printf("[Sync] enqueue failed stage=input_empty pathLen=%u payloadBytes=%u\n",
                  static_cast<unsigned>(path.length()),
                  static_cast<unsigned>(payload.length()));
    return false;
  }
  const String normalizedMethod = method.equalsIgnoreCase("PATCH") ? "PATCH" : "PUT";
  if (!method.equalsIgnoreCase("PATCH") && !method.equalsIgnoreCase("PUT")) {
    Serial.printf("[Sync] enqueue failed stage=method_invalid method=%s pathLen=%u payloadBytes=%u\n",
                  method.c_str(),
                  static_cast<unsigned>(path.length()),
                  static_cast<unsigned>(payload.length()));
    return false;
  }
  if (payload.length() > kMaxUploadPayloadLength) {
    // La cola offline vive en NVS y aun tiene limites practicos de tamano por
    // entrada. Evitamos writes gigantes para no corromper la cola.
    Serial.printf("[Sync] enqueue failed stage=payload_too_large pathLen=%u payloadBytes=%u limit=%u\n",
                  static_cast<unsigned>(path.length()),
                  static_cast<unsigned>(payload.length()),
                  static_cast<unsigned>(kMaxUploadPayloadLength));
    return false;
  }

  // La cola offline es circular: cuando se llena, el siguiente write pisa al
  // mas viejo. Eso evita crecimiento sin limite en NVS.
  Preferences preferences;
  if (!preferences.begin(kUploadsNamespace, false)) {
    Serial.printf("[Sync] enqueue failed stage=nvs_begin pathLen=%u payloadBytes=%u\n",
                  static_cast<unsigned>(path.length()),
                  static_cast<unsigned>(payload.length()));
    return false;
  }

  uint8_t head = preferences.getUChar(kUploadHeadKey, 0);
  uint8_t tail = preferences.getUChar(kUploadTailKey, 0);
  uint8_t count = preferences.getUChar(kUploadCountKey, 0);
  const uint8_t beforeCount = count;
  const uint8_t slot = head % kMaxPendingUploads;

  const bool wrotePath = preferences.putString(uploadPathKey(slot).c_str(), path) > 0;
  const bool wrotePayload = preferences.putString(uploadPayloadKey(slot).c_str(), payload) > 0;
  const String meta = String(0) + "|" + String(millis()) + "|" + normalizedMethod;
  const bool wroteMeta = preferences.putString(uploadMetaKey(slot).c_str(), meta) > 0;
  if (!wrotePath || !wrotePayload || !wroteMeta) {
    const char* stage = !wrotePath ? "path_write" : (!wrotePayload ? "payload_write" : "meta_write");
    Serial.printf(
        "[Sync] enqueue failed stage=%s pathLen=%u payloadBytes=%u queueBefore=%u head=%u tail=%u count=%u\n",
        stage,
        static_cast<unsigned>(path.length()),
        static_cast<unsigned>(payload.length()),
        static_cast<unsigned>(beforeCount),
        static_cast<unsigned>(head),
        static_cast<unsigned>(tail),
        static_cast<unsigned>(count));
    if (preferences.isKey(uploadPathKey(slot).c_str())) {
      preferences.remove(uploadPathKey(slot).c_str());
    }
    if (preferences.isKey(uploadPayloadKey(slot).c_str())) {
      preferences.remove(uploadPayloadKey(slot).c_str());
    }
    if (preferences.isKey(uploadMetaKey(slot).c_str())) {
      preferences.remove(uploadMetaKey(slot).c_str());
    }
    preferences.end();
    return false;
  }

  head = (slot + 1) % kMaxPendingUploads;
  if (count >= kMaxPendingUploads) {
    Serial.printf("[SYNC_QUEUE] slot reused key=%s\n", uploadPayloadKey(slot).c_str());
    tail = (tail + 1) % kMaxPendingUploads;
    count = kMaxPendingUploads;
  } else {
    count++;
  }

  const bool wroteHead = preferences.putUChar(kUploadHeadKey, head) == sizeof(uint8_t);
  const bool wroteTail = preferences.putUChar(kUploadTailKey, tail) == sizeof(uint8_t);
  const bool wroteCount = preferences.putUChar(kUploadCountKey, count) == sizeof(uint8_t);
  if (!wroteHead || !wroteTail || !wroteCount) {
    const char* stage = !wroteHead ? "head_write" : (!wroteTail ? "tail_write" : "count_write");
    Serial.printf(
        "[Sync] enqueue failed stage=%s pathLen=%u payloadBytes=%u queueBefore=%u head=%u tail=%u count=%u\n",
        stage,
        static_cast<unsigned>(path.length()),
        static_cast<unsigned>(payload.length()),
        static_cast<unsigned>(beforeCount),
        static_cast<unsigned>(head),
        static_cast<unsigned>(tail),
        static_cast<unsigned>(count));
    preferences.end();
    return false;
  }
  preferences.end();
  Serial.printf("[Sync] enqueue ok method=%s pathLen=%u payloadBytes=%u queueBefore=%u queueAfter=%u\n",
                normalizedMethod.c_str(),
                static_cast<unsigned>(path.length()),
                static_cast<unsigned>(payload.length()),
                static_cast<unsigned>(beforeCount),
                static_cast<unsigned>(count));
  return true;
}

bool LocalPersistenceStore::repairUploadQueue(size_t maxPayloadBytes, bool dropOptional) const {
  UploadQueueLock queueLock;
  if (!queueLock.locked()) {
    return false;
  }
  Preferences preferences;
  if (!preferences.begin(kUploadsNamespace, false)) {
    return false;
  }

  PendingUploadRecord kept[kMaxPendingUploads];
  uint8_t keptCount = 0;
  const uint8_t count = preferences.getUChar(kUploadCountKey, 0);
  const uint8_t tail = preferences.getUChar(kUploadTailKey, 0) % kMaxPendingUploads;

  for (uint8_t i = 0; i < count && i < kMaxPendingUploads; ++i) {
    const uint8_t slot = static_cast<uint8_t>((tail + i) % kMaxPendingUploads);
    const bool hasPathKey = preferences.isKey(uploadPathKey(slot).c_str());
    const bool hasPayloadKey = preferences.isKey(uploadPayloadKey(slot).c_str());
    const bool hasMetaKey = preferences.isKey(uploadMetaKey(slot).c_str());
    if (!hasPathKey || !hasPayloadKey) {
      continue;
    }
    PendingUploadRecord record;
    record.path = preferences.getString(uploadPathKey(slot).c_str(), "");
    record.payload = preferences.getString(uploadPayloadKey(slot).c_str(), "");
    const String meta = hasMetaKey ? preferences.getString(uploadMetaKey(slot).c_str(), "") : "";
    const int sep1 = meta.indexOf('|');
    const int sep2 = sep1 > 0 ? meta.indexOf('|', sep1 + 1) : -1;
    if (sep1 > 0) {
      record.attempts = static_cast<uint8_t>(meta.substring(0, sep1).toInt());
      if (sep2 > sep1) {
        record.enqueuedAtMs = static_cast<uint32_t>(meta.substring(sep1 + 1, sep2).toInt());
        const String methodPart = meta.substring(sep2 + 1);
        record.method = methodPart.equalsIgnoreCase("PATCH") ? "PATCH" : "PUT";
      } else {
        record.enqueuedAtMs = static_cast<uint32_t>(meta.substring(sep1 + 1).toInt());
        record.method = "PUT";
      }
    }

    const bool optional = record.path.endsWith("/uploadManifest") ||
                          record.path.indexOf("/repSets/") >= 0 ||
                          record.path.endsWith("/rawSession");
    const bool usable = !record.path.isEmpty() &&
                        !record.payload.isEmpty() &&
                        record.payload.length() <= maxPayloadBytes &&
                        !(dropOptional && optional);
    if (usable && keptCount < kMaxPendingUploads) {
      kept[keptCount++] = record;
    }
  }

  for (uint8_t slot = 0; slot < kMaxPendingUploads; ++slot) {
    if (preferences.isKey(uploadPathKey(slot).c_str())) {
      preferences.remove(uploadPathKey(slot).c_str());
    }
    if (preferences.isKey(uploadPayloadKey(slot).c_str())) {
      preferences.remove(uploadPayloadKey(slot).c_str());
    }
    if (preferences.isKey(uploadMetaKey(slot).c_str())) {
      preferences.remove(uploadMetaKey(slot).c_str());
    }
  }
  preferences.putUChar(kUploadHeadKey, 0);
  preferences.putUChar(kUploadTailKey, 0);
  preferences.putUChar(kUploadCountKey, 0);

  uint8_t written = 0;
  for (uint8_t i = 0; i < keptCount; ++i) {
    const uint8_t slot = written % kMaxPendingUploads;
    const String method = kept[i].method.equalsIgnoreCase("PATCH") ? "PATCH" : "PUT";
    const String meta = String(kept[i].attempts) + "|" + String(kept[i].enqueuedAtMs) + "|" + method;
    const bool wrotePath = preferences.putString(uploadPathKey(slot).c_str(), kept[i].path) > 0;
    const bool wrotePayload = preferences.putString(uploadPayloadKey(slot).c_str(), kept[i].payload) > 0;
    const bool wroteMeta = preferences.putString(uploadMetaKey(slot).c_str(), meta) > 0;
    if (wrotePath && wrotePayload && wroteMeta) {
      written++;
    } else {
      if (preferences.isKey(uploadPathKey(slot).c_str())) {
        preferences.remove(uploadPathKey(slot).c_str());
      }
      if (preferences.isKey(uploadPayloadKey(slot).c_str())) {
        preferences.remove(uploadPayloadKey(slot).c_str());
      }
      if (preferences.isKey(uploadMetaKey(slot).c_str())) {
        preferences.remove(uploadMetaKey(slot).c_str());
      }
    }
  }

  preferences.putUChar(kUploadHeadKey, written % kMaxPendingUploads);
  preferences.putUChar(kUploadTailKey, 0);
  preferences.putUChar(kUploadCountKey, written);
  preferences.end();
  return true;
}

bool LocalPersistenceStore::peekUpload(PendingUploadRecord& record) const {
  UploadQueueLock queueLock;
  if (!queueLock.locked()) {
    return false;
  }
  Preferences preferences;
  if (!preferences.begin(kUploadsNamespace, false)) {
    return false;
  }

  uint8_t count = preferences.getUChar(kUploadCountKey, 0);
  if (count == 0) {
    preferences.end();
    return false;
  }

  uint8_t tail = preferences.getUChar(kUploadTailKey, 0) % kMaxPendingUploads;
  uint8_t scanned = 0;
  bool healed = false;
  while (count > 0 && scanned < kMaxPendingUploads) {
    const String pathKey = uploadPathKey(tail);
    const String payloadKey = uploadPayloadKey(tail);
    const String metaKey = uploadMetaKey(tail);
    const bool hasPath = preferences.isKey(pathKey.c_str());
    const bool hasPayload = preferences.isKey(payloadKey.c_str());

    if (hasPath && hasPayload) {
      record.path = preferences.getString(pathKey.c_str(), "");
      record.payload = preferences.getString(payloadKey.c_str(), "");
      const String meta = preferences.isKey(metaKey.c_str())
                              ? preferences.getString(metaKey.c_str(), "")
                              : "";
      const int sep1 = meta.indexOf('|');
      const int sep2 = sep1 > 0 ? meta.indexOf('|', sep1 + 1) : -1;
      if (sep1 > 0) {
        record.attempts = static_cast<uint8_t>(meta.substring(0, sep1).toInt());
        if (sep2 > sep1) {
          record.enqueuedAtMs = static_cast<uint32_t>(meta.substring(sep1 + 1, sep2).toInt());
          const String methodPart = meta.substring(sep2 + 1);
          record.method = methodPart.equalsIgnoreCase("PATCH") ? "PATCH" : "PUT";
        } else {
          record.enqueuedAtMs = static_cast<uint32_t>(meta.substring(sep1 + 1).toInt());
          record.method = "PUT";
        }
      } else {
        record.attempts = 0;
        record.enqueuedAtMs = 0;
        record.method = "PUT";
      }
      if (!record.path.isEmpty() && !record.payload.isEmpty()) {
        if (healed) {
          preferences.putUChar(kUploadTailKey, tail);
          preferences.putUChar(kUploadCountKey, count);
          if (count == 0) {
            preferences.putUChar(kUploadHeadKey, tail);
          }
        }
        preferences.end();
        return true;
      }
    }

    // Self-heal: slot metadata says "used" but payload/path is missing.
    preferences.remove(pathKey.c_str());
    preferences.remove(payloadKey.c_str());
    preferences.remove(metaKey.c_str());
    healed = true;
    tail = (tail + 1) % kMaxPendingUploads;
    count--;
    scanned++;
  }
  preferences.putUChar(kUploadTailKey, tail);
  preferences.putUChar(kUploadCountKey, count);
  if (count == 0) {
    preferences.putUChar(kUploadHeadKey, tail);
  }
  preferences.end();
  return false;
}

bool LocalPersistenceStore::dropOldestUpload() const {
  UploadQueueLock queueLock;
  if (!queueLock.locked()) {
    return false;
  }
  Preferences preferences;
  if (!preferences.begin(kUploadsNamespace, false)) {
    return false;
  }

  uint8_t count = preferences.getUChar(kUploadCountKey, 0);
  if (count == 0) {
    preferences.end();
    return false;
  }

  const uint8_t tail = preferences.getUChar(kUploadTailKey, 0) % kMaxPendingUploads;
  const String clearedKey = uploadPayloadKey(tail);
  preferences.remove(uploadPathKey(tail).c_str());
  preferences.remove(uploadPayloadKey(tail).c_str());
  preferences.remove(uploadMetaKey(tail).c_str());
  preferences.putUChar(kUploadTailKey, (tail + 1) % kMaxPendingUploads);
  preferences.putUChar(kUploadCountKey, count - 1);
  preferences.end();
  Serial.printf("[SYNC_QUEUE] item cleared key=%s\n", clearedKey.c_str());
  return true;
}

bool LocalPersistenceStore::bumpOldestUploadAttempt() const {
  UploadQueueLock queueLock;
  if (!queueLock.locked()) {
    return false;
  }
  Preferences preferences;
  if (!preferences.begin(kUploadsNamespace, false)) {
    return false;
  }
  const uint8_t count = preferences.getUChar(kUploadCountKey, 0);
  if (count == 0) {
    preferences.end();
    return false;
  }
  const uint8_t tail = preferences.getUChar(kUploadTailKey, 0) % kMaxPendingUploads;
  const String metaKey = uploadMetaKey(tail);
  const String meta = preferences.isKey(metaKey.c_str())
                          ? preferences.getString(metaKey.c_str(), "")
                          : "";
  uint8_t attempts = 0;
  uint32_t enqueuedAtMs = millis();
  String method = "PUT";
  const int sep1 = meta.indexOf('|');
  const int sep2 = sep1 > 0 ? meta.indexOf('|', sep1 + 1) : -1;
  if (sep1 > 0) {
    attempts = static_cast<uint8_t>(meta.substring(0, sep1).toInt());
    if (sep2 > sep1) {
      enqueuedAtMs = static_cast<uint32_t>(meta.substring(sep1 + 1, sep2).toInt());
      const String methodPart = meta.substring(sep2 + 1);
      method = methodPart.equalsIgnoreCase("PATCH") ? "PATCH" : "PUT";
    } else {
      enqueuedAtMs = static_cast<uint32_t>(meta.substring(sep1 + 1).toInt());
    }
  }
  attempts = static_cast<uint8_t>(attempts + 1);
  const String nextMeta = String(attempts) + "|" + String(enqueuedAtMs) + "|" + method;
  const bool ok = preferences.putString(metaKey.c_str(), nextMeta) > 0;
  preferences.end();
  return ok;
}

String LocalPersistenceStore::uploadMetaKey(uint8_t index) {
  return String("meta_") + String(index);
}

uint8_t LocalPersistenceStore::getPendingUploadCount() const {
  UploadQueueLock queueLock;
  if (!queueLock.locked()) {
    return 0;
  }
  Preferences preferences;
  if (!preferences.begin(kUploadsNamespace, true)) {
    return 0;
  }
  const uint8_t count = preferences.getUChar(kUploadCountKey, 0);
  preferences.end();
  return count;
}

bool LocalPersistenceStore::buildWeeklyTrainingSignal(const String& userUid,
                                                      const String& machineId,
                                                      uint32_t nowEpoch,
                                                      WeeklyTrainingSignal& signal) const {
  signal = WeeklyTrainingSignal{};
  if (userUid.isEmpty() || machineId.isEmpty() || nowEpoch == 0) {
    return false;
  }

  Preferences preferences;
  if (!preferences.begin(kSessionsNamespace, true)) {
    return false;
  }

  const uint8_t count = preferences.getUChar(kSessionCountKey, 0);
  if (count == 0) {
    preferences.end();
    return false;
  }

  // Use a wider local lookback and then choose the most recent 2-3 sessions
  // so load decisions are stable and based on current performance.
  constexpr uint32_t kLookbackSec = 21UL * 24UL * 60UL * 60UL;
  const uint32_t minEpoch = nowEpoch > kLookbackSec ? (nowEpoch - kLookbackSec) : 0;

  struct Candidate {
    bool used = false;
    uint32_t endedAtEpoch = 0;
    float selectedWeightKg = 0.0f;
    float setRatio = 0.0f;
    float repRatio = 0.0f;
    float avgPeakVelocity = 0.0f;
    float qualityScore = 0.0f;
    bool poorForm = false;
  };
  Candidate top[3];
  uint8_t topCount = 0;

  uint32_t bestEpochSeen = 0;
  float lastWeightSeen = 0.0f;

  for (uint8_t i = 0; i < kMaxStoredSessions; ++i) {
    const String key = sessionKey(i);
    if (!preferences.isKey(key.c_str())) {
      continue;
    }
    const String raw = preferences.getString(key.c_str(), "");
    if (raw.isEmpty()) {
      continue;
    }

    String recordUserUid;
    String recordMachineId;
    if (!jsonReadString(raw, "userUid", recordUserUid) ||
        !jsonReadString(raw, "machineId", recordMachineId)) {
      continue;
    }
    if (!recordUserUid.equalsIgnoreCase(userUid) ||
        !recordMachineId.equalsIgnoreCase(machineId)) {
      continue;
    }

    uint32_t endedAtEpoch = 0;
    if (!jsonReadUInt(raw, "endedAtEpoch", endedAtEpoch) || endedAtEpoch < minEpoch) {
      continue;
    }

    float selectedWeightKg = 0.0f;
    float avgPeakVelocity = 0.0f;
    float setsCompleted = 0.0f;
    float targetSets = 0.0f;
    float validReps = 0.0f;
    float targetRepsMax = 0.0f;
    jsonReadFloat(raw, "selectedWeightKg", selectedWeightKg);
    jsonReadFloat(raw, "avgPeakVelocityPctPerSec", avgPeakVelocity);
    jsonReadFloat(raw, "setsCompleted", setsCompleted);
    jsonReadFloat(raw, "targetSets", targetSets);
    jsonReadFloat(raw, "validReps", validReps);
    jsonReadFloat(raw, "targetRepsMax", targetRepsMax);

    const float safeTargetSets = targetSets > 0.0f ? targetSets : 1.0f;
    const float plannedReps = safeTargetSets * (targetRepsMax > 0.0f ? targetRepsMax : 1.0f);
    const float setRatio = constrain(setsCompleted / safeTargetSets, 0.0f, 1.2f);
    const float repRatio = constrain(validReps / plannedReps, 0.0f, 1.2f);

    float avgRomPercent = 0.0f;
    float invalidReps = 0.0f;
    jsonReadFloat(raw, "avgRomPercent", avgRomPercent);
    jsonReadFloat(raw, "invalidReps", invalidReps);
    const float totalReps = max(1.0f, validReps + invalidReps);
    const float validRatio = constrain(validReps / totalReps, 0.0f, 1.0f);

    // Session quality score 0..100 (simple, explainable, robust).
    const float romScore = constrain(avgRomPercent / 95.0f, 0.0f, 1.0f) * 100.0f;
    const float repScore = constrain(repRatio / 1.0f, 0.0f, 1.0f) * 100.0f;
    const float validScore = validRatio * 100.0f;
    const float velScore = constrain(avgPeakVelocity / 120.0f, 0.0f, 1.0f) * 100.0f;
    const float qualityScore = (romScore * 0.35f) + (repScore * 0.25f) +
                               (validScore * 0.25f) + (velScore * 0.15f);
    const bool poorForm = (qualityScore < 62.0f) || (validRatio < 0.70f) || (avgRomPercent < 62.0f);

    signal.sessions++;
    if (endedAtEpoch >= bestEpochSeen) {
      bestEpochSeen = endedAtEpoch;
      lastWeightSeen = selectedWeightKg;
    }

    // Insert by recency into fixed-size top[3].
    int insertAt = -1;
    for (int j = 0; j < 3; ++j) {
      if (!top[j].used || endedAtEpoch > top[j].endedAtEpoch) {
        insertAt = j;
        break;
      }
    }
    if (insertAt >= 0) {
      for (int j = 2; j > insertAt; --j) {
        top[j] = top[j - 1];
      }
      top[insertAt].used = true;
      top[insertAt].endedAtEpoch = endedAtEpoch;
      top[insertAt].selectedWeightKg = selectedWeightKg;
      top[insertAt].setRatio = setRatio;
      top[insertAt].repRatio = repRatio;
      top[insertAt].avgPeakVelocity = avgPeakVelocity;
      top[insertAt].qualityScore = qualityScore;
      top[insertAt].poorForm = poorForm;
      if (topCount < 3) {
        topCount++;
      }
    }
  }

  preferences.end();
  if (signal.sessions == 0) {
    return false;
  }

  if (topCount == 0) {
    return false;
  }

  float sumWeight = 0.0f;
  float sumSetRatio = 0.0f;
  float sumRepRatio = 0.0f;
  float sumPeakVel = 0.0f;
  float sumQuality = 0.0f;
  uint8_t poorFormCount = 0;
  for (uint8_t i = 0; i < topCount; ++i) {
    sumWeight += top[i].selectedWeightKg;
    sumSetRatio += top[i].setRatio;
    sumRepRatio += top[i].repRatio;
    sumPeakVel += top[i].avgPeakVelocity;
    sumQuality += top[i].qualityScore;
    if (top[i].poorForm) {
      poorFormCount++;
    }
  }

  const float sessionsF = static_cast<float>(topCount);
  signal.hasData = true;
  signal.recentSessionsUsed = topCount;
  signal.avgSelectedWeightKg = sumWeight / sessionsF;
  signal.lastSelectedWeightKg = lastWeightSeen > 0.0f ? lastWeightSeen : signal.avgSelectedWeightKg;
  signal.avgSetCompletionRatio = sumSetRatio / sessionsF;
  signal.avgRepCompletionRatio = sumRepRatio / sessionsF;
  signal.avgPeakVelocityPctPerSec = sumPeakVel / sessionsF;
  signal.avgRepQualityScore = sumQuality / sessionsF;
  signal.poorFormSessionRatio = static_cast<float>(poorFormCount) / sessionsF;
  return true;
}

String LocalPersistenceStore::profileKey(uint8_t index) {
  return "u" + String(index);
}

String LocalPersistenceStore::sessionKey(uint8_t index) {
  return "s" + String(index);
}

String LocalPersistenceStore::uploadPathKey(uint8_t index) {
  return "up" + String(index);
}

String LocalPersistenceStore::uploadPayloadKey(uint8_t index) {
  return "ud" + String(index);
}

String LocalPersistenceStore::serializeProfile(const UserProfile& profile) {
  // Elegimos un formato compacto separado por delimitadores porque perfiles y
  // calibraciones deben ocupar poco espacio y persistir rapido en NVS.
  String raw;
  raw.reserve(256);
  raw += profile.rfidUid + "|";
  raw += profile.displayName + "|";
  raw += String(profile.hasBasicData ? 1 : 0) + "|";
  raw += String(profile.weightKg, 2) + "|";
  raw += String(profile.age) + "|";
  raw += String(profile.heightCm, 2) + "|";
  raw += String(static_cast<int>(profile.gender)) + "|";
  raw += String(static_cast<int>(profile.goal)) + "|";
  raw += String(profile.updatedAtEpoch) + "|";
  raw += String(profile.machineCalibrationCount) + "|";

  for (uint8_t i = 0; i < profile.machineCalibrationCount; ++i) {
    if (i > 0) {
      raw += "^";
    }
    const UserMachineCalibration& calibration = profile.machineCalibrations[i];
    raw += calibration.machineTypeId + ",";
    raw += String(calibration.hasCalibration ? 1 : 0) + ",";
    raw += String(calibration.suggestedWeightKg, 2) + ",";
    raw += String(calibration.userRomPercent, 2) + ",";
    raw += String(calibration.userBottomPct, 2) + ",";
    raw += String(calibration.userTopPct, 2) + ",";
    raw += String(calibration.updatedAtEpoch) + ",";
    raw += String(calibration.nextRecommendedWeightKg, 2) + ",";
    raw += calibration.nextRecommendationSource + ",";
    raw += String(calibration.nextRecommendationUpdatedAt) + ",";
    raw += String(static_cast<int>(calibration.resultAction)) + ",";
    raw += String(static_cast<int>(calibration.resultConfidence));
  }

  return raw;
}

String LocalPersistenceStore::serializeSessionSummary(const SessionHistoryRecord& record) {
  // Este resumen es el salvavidas local cuando la sesion rica completa no cabe
  // comodamente en NVS. Firebase sigue siendo la fuente del historial profundo.
  String json;
  json.reserve(768);
  json += "{";
  json += "\"sessionId\":\"" + record.sessionId + "\",";
  json += "\"userUid\":\"" + record.userUid + "\",";
  json += "\"userDisplayName\":\"" + record.userDisplayName + "\",";
  json += "\"machineId\":\"" + record.machineId + "\",";
  json += "\"machineTypeId\":\"" + record.machineTypeId + "\",";
  json += "\"goal\":\"" + record.goal + "\",";
  json += "\"anonymous\":" + String(record.anonymous ? "true" : "false") + ",";
  json += "\"calibrationBased\":" + String(record.calibrationBased ? "true" : "false") + ",";
  json += "\"selectedWeightKg\":" + String(record.selectedWeightKg, 2) + ",";
  json += "\"suggestedWeightKg\":" + String(record.suggestedWeightKg, 2) + ",";
  json += "\"userRomPercent\":" + String(record.userRomPercent, 2) + ",";
  json += "\"userRomBottomPct\":" + String(record.userRomBottomPct, 2) + ",";
  json += "\"userRomTopPct\":" + String(record.userRomTopPct, 2) + ",";
  json += "\"targetSets\":" + String(record.targetSets) + ",";
  json += "\"setsCompleted\":" + String(record.setsCompleted) + ",";
  json += "\"validReps\":" + String(record.validReps) + ",";
  json += "\"invalidReps\":" + String(record.invalidReps) + ",";
  json += "\"fastEccentricWarnings\":" + String(record.fastEccentricWarnings) + ",";
  json += "\"avgRomPercent\":" + String(record.avgRomPercent, 2) + ",";
  json += "\"bestRomPercent\":" + String(record.bestRomPercent, 2) + ",";
  json += "\"avgConcentricTimeMs\":" + String(record.avgConcentricTimeMs, 2) + ",";
  json += "\"avgPeakVelocityPctPerSec\":" + String(record.avgPeakVelocityPctPerSec, 2) + ",";
  json += "\"avgPeakEccentricVelocityPctPerSec\":" +
          String(record.avgPeakEccentricVelocityPctPerSec, 2) + ",";
  json += "\"startedAtEpoch\":" + String(record.startedAtEpoch) + ",";
  json += "\"endedAtEpoch\":" + String(record.endedAtEpoch) + ",";
  json += "\"startedAtIso\":\"" + record.startedAtIso + "\",";
  json += "\"endedAtIso\":\"" + record.endedAtIso + "\",";
  json += "\"durationMs\":" + String(record.durationMs) + ",";
  json += "\"totalRestMs\":" + String(record.totalRestMs);
  json += "}";
  return json;
}

bool LocalPersistenceStore::deserializeProfile(const String& raw, UserProfile& profile) {
  if (raw.isEmpty()) {
    return false;
  }

  int fieldStart = 0;
  String fields[11];
  uint8_t fieldIndex = 0;
  for (uint16_t i = 0; i <= raw.length() && fieldIndex < 11; ++i) {
    if (i == raw.length() || raw.charAt(i) == '|') {
      fields[fieldIndex++] = raw.substring(fieldStart, i);
      fieldStart = i + 1;
    }
  }

  if (fieldIndex < 9) {
    return false;
  }

  resetUserProfile(profile);
  profile.rfidUid = fields[0];
  profile.displayName = fields[1];
  profile.hasBasicData = fields[2].toInt() != 0;
  profile.weightKg = fields[3].toFloat();
  profile.age = static_cast<uint8_t>(fields[4].toInt());
  profile.heightCm = fields[5].toFloat();
  uint8_t nextIndex = 6;
  if (fieldIndex >= 11) {
    profile.gender = static_cast<UserGender>(fields[nextIndex].toInt());
    nextIndex++;
  } else {
    profile.gender = UserGender::Unspecified;
  }
  profile.goal = goalFromInt(fields[nextIndex].toInt());
  uint8_t calibrationCount = 0;
  const bool hasTimestampFields = fieldIndex >= 11;
  if (hasTimestampFields) {
    profile.updatedAtEpoch = fields[nextIndex + 1].toInt();
    calibrationCount = static_cast<uint8_t>(fields[nextIndex + 2].toInt());
  } else {
    profile.updatedAtEpoch = 0;
    calibrationCount = static_cast<uint8_t>(fields[nextIndex + 1].toInt());
  }
  profile.machineCalibrationCount = 0;

  const String& calibrationBlob = hasTimestampFields ? fields[nextIndex + 3] : fields[nextIndex + 2];
  if (!calibrationBlob.isEmpty()) {
    int start = 0;
    uint8_t parsed = 0;
    while (start <= calibrationBlob.length() && parsed < calibrationCount &&
           parsed < UserProfile::kMaxMachineCalibrations) {
      const int separator = calibrationBlob.indexOf('^', start);
      const int end = separator >= 0 ? separator : calibrationBlob.length();
      const String token = calibrationBlob.substring(start, end);

      const int a = token.indexOf(',');
      const int b = a >= 0 ? token.indexOf(',', a + 1) : -1;
      const int c = b >= 0 ? token.indexOf(',', b + 1) : -1;
      const int d = c >= 0 ? token.indexOf(',', c + 1) : -1;
      const int e = d >= 0 ? token.indexOf(',', d + 1) : -1;
      const int f = e >= 0 ? token.indexOf(',', e + 1) : -1;
      const int g = f >= 0 ? token.indexOf(',', f + 1) : -1;
      const int h = g >= 0 ? token.indexOf(',', g + 1) : -1;
      const int i = h >= 0 ? token.indexOf(',', h + 1) : -1;
      const int j = i >= 0 ? token.indexOf(',', i + 1) : -1;
      const int k = j >= 0 ? token.indexOf(',', j + 1) : -1;
      if (a > 0 && b > a && c > b) {
        UserMachineCalibration& calibration = profile.machineCalibrations[parsed];
        calibration.machineTypeId = token.substring(0, a);
        calibration.hasCalibration = token.substring(a + 1, b).toInt() != 0;
        calibration.suggestedWeightKg = token.substring(b + 1, c).toFloat();
        calibration.userRomPercent = token.substring(c + 1, d > c ? d : token.length()).toFloat();
        if (f > e) {
          calibration.userBottomPct = token.substring(d + 1, e).toFloat();
          calibration.userTopPct = token.substring(e + 1, f).toFloat();
          const int updatedEnd = g > f ? g : token.length();
          calibration.updatedAtEpoch = static_cast<uint32_t>(token.substring(f + 1, updatedEnd).toInt());
          if (k > j && j > i && i > h && h > g) {
            calibration.nextRecommendedWeightKg = token.substring(g + 1, h).toFloat();
            calibration.nextRecommendationSource = token.substring(h + 1, i);
            calibration.nextRecommendationUpdatedAt = static_cast<uint32_t>(token.substring(i + 1, j).toInt());
            calibration.resultAction =
                static_cast<CalibrationAction>(token.substring(j + 1, k).toInt());
            calibration.resultConfidence =
                static_cast<CalibrationConfidence>(token.substring(k + 1).toInt());
          } else {
            calibration.nextRecommendedWeightKg = calibration.suggestedWeightKg;
            calibration.nextRecommendationSource = "legacy_calibration";
            calibration.nextRecommendationUpdatedAt = calibration.updatedAtEpoch;
            calibration.resultAction = CalibrationAction::Keep;
            calibration.resultConfidence = CalibrationConfidence::Low;
          }
        } else {
          calibration.userBottomPct = 0.0f;
          calibration.userTopPct = calibration.userRomPercent;
          calibration.updatedAtEpoch = (d > c) ? static_cast<uint32_t>(token.substring(d + 1).toInt()) : 0;
          calibration.nextRecommendedWeightKg = calibration.suggestedWeightKg;
          calibration.nextRecommendationSource = "legacy_calibration";
          calibration.nextRecommendationUpdatedAt = calibration.updatedAtEpoch;
          calibration.resultAction = CalibrationAction::Keep;
          calibration.resultConfidence = CalibrationConfidence::Low;
        }
        if (calibration.userTopPct <= calibration.userBottomPct) {
          calibration.userBottomPct = 0.0f;
          calibration.userTopPct = calibration.userRomPercent;
        }
        calibration.schemaVersion = 1;
        calibration.source = "legacy_calibration";
        calibration.goalId = "general";
        calibration.resultRecommendedWeightKg = calibration.suggestedWeightKg;
        parsed++;
      }

      if (separator < 0) {
        break;
      }
      start = separator + 1;
    }
    profile.machineCalibrationCount = parsed;
  }

  profile.updatedAtIso = "";
  for (uint8_t i = 0; i < profile.machineCalibrationCount; ++i) {
    profile.machineCalibrations[i].updatedAtIso = "";
  }

  return !profile.rfidUid.isEmpty();
}
