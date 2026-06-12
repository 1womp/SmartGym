#include "SmartGymTouchApp.h"
#include "SmartGymWifiConfig.h"
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <freertos/task.h>

namespace {
constexpr uint32_t kTickIntervalMs = 4;
constexpr uint32_t kSlowUiRefreshMs = 140;
constexpr uint32_t kDebugUiRefreshMs = 160;
// Keep graph animation aligned to the display instead of invalidating it far
// faster than the RGB panel can show. This reduces small chart/dot flickers.
constexpr uint32_t kChartRefreshMs = 16;
constexpr uint32_t kScreenTransitionMs = 0;
constexpr uint32_t kCloudServiceIntervalMs = 5000;
constexpr uint32_t kCloudServiceIntervalIdentifyMs = 1800;
constexpr uint32_t kCloudSliceBudgetMs = 18;
constexpr uint32_t kDeviceHeartbeatIntervalMs = 60000UL;
constexpr uint32_t kMachinePollIntervalMs = 300000UL;
constexpr uint32_t kUploadRetryIntervalMs = 15000UL;
constexpr uint32_t kCloudBootGraceMs = 12000UL;
constexpr uint32_t kSessionAutoLogoutMs = 35000UL;
constexpr uint32_t kSummaryMinVisibleMs = 2500UL;
constexpr uint32_t kSummarySyncWaitMaxMs = 12000UL;
constexpr uint32_t kSummaryCloudSettleMs = 22000UL;
constexpr uint32_t kSummaryCloudServiceIntervalMs = 1200UL;
constexpr uint32_t kSummarySmallSyncDelayMs = 2500UL;
constexpr uint32_t kSummaryUploadRetryIntervalMs = 1200UL;
constexpr uint32_t kCloudAfterScreenSwitchQuietMs = 5000UL;
constexpr uint32_t kCloudAfterLogoutQuietMs = 10000UL;
constexpr uint32_t kUserSyncTimeoutMs = 12000UL;
constexpr uint32_t kIdealCycleMs = 6000UL;
constexpr uint32_t kMotionGraphWindowMs = 5000UL;
constexpr lv_coord_t kMotionChartPadTopPx = 12;
constexpr lv_coord_t kMotionChartPadBottomPx = 12;
constexpr lv_coord_t kMotionChartPadSidePx = 8;

constexpr float kWeightStepKg = 2.5f;
constexpr float kMinWeightKg = 5.0f;
constexpr float kMaxWeightKg = 120.0f;
constexpr uint16_t kCalibrationModelMinConcentricMs = 180;
constexpr float kCalibrationModelMaxVelocityPctPerSec = 900.0f;
constexpr float kActiveRomBottomReachedPct = 18.0f;
constexpr float kActiveRomTopReachedPct = 87.0f;
constexpr float kActiveRomMinValidRangePct = 58.0f;
constexpr float kAutoMotionSpeedPctPerSec = 180.0f;
// Match the visual rule: the 22px live halo counts as "on track" when it
// touches the wide guide lane, not only when the dot center is near the line.
constexpr float kMotionLaneTolerancePct = 10.0f;
constexpr float kMotionLaneHalfThicknessPct = 8.0f;
constexpr float kDotPredictionLeadSec = 0.025f;
constexpr float kDotPreviewNorm = 0.45f;
constexpr float kVelocityStillDeltaEnterPct = 0.14f;
constexpr float kVelocityStillDeltaExitPct = 0.30f;
constexpr float kVelocityStillSpeedEnterPctPerSec = 8.0f;
constexpr float kVelocityStillSpeedExitPctPerSec = 15.0f;
constexpr float kVelocityDisplayDeadbandPctPerSec = 2.2f;
constexpr float kRepQualityMinRomPct = 62.0f;
constexpr float kRepQualityMinPeakVelPctPerSec = 18.0f;
constexpr uint32_t kSetPauseCloseMs = 8500UL;
constexpr uint32_t kSetPauseStableMs = 1600UL;
constexpr float kSetPauseStillVelPctPerSec = 4.5f;
constexpr float kSetPauseStillDeltaPct = 0.35f;
constexpr uint32_t kNoisyRepMinIntervalMs = 420UL;
constexpr uint32_t kIdleDuringRestSuppressMs = 180000UL;
constexpr uint32_t kCloudUiQuietAfterInputMs = 10000UL;
constexpr uint32_t kHeavyUploadIdleMs = 15000UL;
constexpr uint32_t kScanReconcileDelayMs = 2500UL;
constexpr uint32_t kUploadRetryMaxBackoffMs = 120000UL;
constexpr uint8_t kUploadRetryMaxAttempts = 8;
constexpr uint32_t kUploadStaleDropMs = 72UL * 60UL * 60UL * 1000UL;
constexpr uint32_t kUploadSuccessCadenceMs = 1500UL;
constexpr uint32_t kSummaryUploadUiSmoothCadenceMs = 2000UL;
constexpr uint32_t kReadyUploadFastCadenceMs = 500UL;
constexpr uint32_t kWebDetailWaitingLogCadenceMs = 10000UL;
constexpr uint32_t kUploadFailureCooldown1Ms = 15000UL;
constexpr uint32_t kUploadFailureCooldown2Ms = 30000UL;
constexpr uint32_t kUploadFailureCooldown3Ms = 60000UL;
constexpr uint32_t kUploadFragmentationDelayMs = 1500UL;
constexpr uint32_t kUploadStreakCooldownMs = 2000UL;
constexpr uint8_t kUploadStreakCooldownCount = 6;
constexpr uint32_t kCloudReadPostUploadCooldownMs = 10000UL;
constexpr uint32_t kCloudReadRecentWindowMs = 15000UL;
constexpr uint32_t kCloudReadPreviousTimeoutWindowMs = 30000UL;
constexpr uint32_t kSyncDiagLogCadenceMs = 1000UL;
constexpr bool kSyncRequestTimingTrace = true;
constexpr bool kUploadSetDetailsDocument = true;
constexpr bool kUploadRepSetsDocuments = true;
constexpr bool kUploadSessionRootDocument = true;
constexpr bool kUploadSessionAnalysisChunks = false;
constexpr bool kUploadManifestDocument = false;
constexpr bool kUploadRawSessionDocument = false;
constexpr bool kUploadRawSessionOnlyWhenCompact = true;
constexpr size_t kUploadRawSessionMaxBytes = 2200;
constexpr uint16_t kNvsUploadMaxPayloadBytes = 2200;
constexpr uint16_t kWebAppSessionRootMaxPayloadBytes = 1100;
constexpr uint16_t kWebAppSessionRootHardMaxPayloadBytes = 1400;
constexpr uint16_t kMaxQueueCountAfterWebAppDetails = 8;
constexpr uint8_t kWebDetailBatchPerTick = 2;
constexpr uint16_t kWebAppPatchPayloadTargetBytes = 1000;
constexpr uint16_t kRepSetsPatchPayloadTargetBytes = 700;
constexpr uint16_t kFirebaseClientRepSetSinglePayloadMaxBytes = 3000;
constexpr uint16_t kFirebaseClientFullRepSetHardMaxBytes = 6000;
constexpr uint16_t kFirebaseClientPatchPayloadTargetBytes = 2500;
constexpr uint16_t kFirebaseClientPatchPayloadHardMaxBytes = 3000;
constexpr uint16_t kFirebaseClientSetDetailsSinglePatchMaxBytes = 2500;
constexpr uint16_t kFirebaseClientRepresentativeRepsMaxBytes = 2500;
constexpr uint16_t kFirebaseClientRepSetsSplitPatchTargetBytes = 1900;
constexpr uint16_t kFirebaseClientRepSetsSplitPatchHardMaxBytes = 2100;
constexpr uint8_t kFirebaseClientRepSetsSplitMaxRepsPerPatch = 6;
constexpr uint16_t kFirebaseClientRepSetsSplitPatchTargetBytesConstrained = 900;
constexpr uint16_t kFirebaseClientRepSetsSplitPatchHardMaxBytesConstrained = 1100;
constexpr uint8_t kFirebaseClientRepSetsSplitMaxRepsPerPatchConstrained = 3;
constexpr uint16_t kFirebaseClientRepSetsSplitPatchTargetBytesCritical = 650;
constexpr uint16_t kFirebaseClientRepSetsSplitPatchHardMaxBytesCritical = 850;
constexpr uint8_t kFirebaseClientRepSetsSplitMaxRepsPerPatchCritical = 2;
constexpr uint32_t kUploadLowMemoryBackoffMs = 1500UL;
constexpr uint32_t kUploadLongCallWarnMs = 1000UL;
constexpr uint32_t kWebDetailEnqueueTimeBudgetMs = 30UL;
constexpr uint8_t kNvsQueueCapacity = 16;
constexpr uint8_t kSafeQueueSoftCapCore = 8;
constexpr uint8_t kSafeQueueSoftCapDetails = 10;
constexpr uint32_t kIdleAutoShowMs = 45000UL;
constexpr uint32_t kIdleSuppressAfterActivityMs = 30000UL;
constexpr uint32_t kIdleSuppressAfterRfidMs = 120000UL;

constexpr int kBtnPrimary = 0;
constexpr int kBtnSecondary = 1;
constexpr int kBtnDanger = 2;
constexpr int kBtnGhost = 3;

constexpr bool kEnableHardwareSensor = true;
constexpr bool kEnableHardwareRfid = true;
constexpr bool kEnableCloud = true;
constexpr uint8_t kSensorPin = 17;
constexpr uint8_t kSensorSampleIntervalMs = 5;
constexpr uint8_t kRfidSsPin = 10;   // SD-CS line on this board; unused by our app
constexpr uint8_t kRfidRstPin = UINT8_MAX;  // Leave RC522 NRSTPD unconnected; soft reset is enough.
constexpr uint8_t kRfidSckPin = 12;   // SD CLK line on this board; unused by our app
constexpr uint8_t kRfidMisoPin = 13;   // SD MISO line on this board; unused by our app
constexpr uint8_t kRfidMosiPin = 11;   // SD MOSI line on this board; unused by our app
constexpr bool kUseCloudSyncWorker = false;
constexpr bool kUseUploadTransportWorker = false;
constexpr bool kUseFirebaseClientQueuedTransport = true;
constexpr bool kUseFirebaseClientCloudReads = true;
constexpr bool kUseFirebaseClientDeviceWrites = true;
constexpr bool kUseFirebaseClientProfileWrites = true;
constexpr bool kUseFirebaseClientCoreBundle = true;
constexpr bool kUseFirebaseClientDetailsBundle = true;
constexpr bool kUseFirebaseClientFullRepSetUpload = true;
constexpr bool kUseFirebaseClientLargeFullRepSetUpload = true;
constexpr uint32_t kProfileWriteDedupWindowMs = 15000UL;

class CloudLockGuard {
 public:
  CloudLockGuard(SemaphoreHandle_t mutex, uint32_t waitMs) : mutex_(mutex) {
    if (mutex_ == nullptr) {
      locked_ = true;
      return;
    }
    locked_ = xSemaphoreTakeRecursive(mutex_, pdMS_TO_TICKS(waitMs)) == pdTRUE;
  }
  ~CloudLockGuard() {
    if (mutex_ != nullptr && locked_) {
      xSemaphoreGiveRecursive(mutex_);
    }
  }
  explicit operator bool() const {
    return locked_;
  }

 private:
  SemaphoreHandle_t mutex_ = nullptr;
  bool locked_ = false;
};

uint32_t internalFree8BitHeap() {
  return static_cast<uint32_t>(
      heap_caps_get_free_size(static_cast<uint32_t>(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));
}

uint32_t internalLargestFree8BitBlock() {
  return static_cast<uint32_t>(
      heap_caps_get_largest_free_block(static_cast<uint32_t>(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));
}

void logBootMemPoint(const char* label) {
  const UBaseType_t stackWords = uxTaskGetStackHighWaterMark(nullptr);
  Serial.printf("[BOOTMEM] %s heap=%lu internalHeap=%lu largestBlock=%lu stackHighWaterWords=%lu\n",
                label != nullptr ? label : "-",
                static_cast<unsigned long>(ESP.getFreeHeap()),
                static_cast<unsigned long>(internalFree8BitHeap()),
                static_cast<unsigned long>(internalLargestFree8BitBlock()),
                static_cast<unsigned long>(stackWords));
}

String jsonEscape(const String& input) {
  String out;
  out.reserve(input.length() + 16);
  for (size_t i = 0; i < input.length(); ++i) {
    const char c = input[i];
    switch (c) {
      case '\"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (static_cast<uint8_t>(c) < 0x20) {
          out += '?';
        } else {
          out += c;
        }
        break;
    }
  }
  return out;
}

void clearSessionHistoryRecordInPlace(SessionHistoryRecord& record) {
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

bool splitTopLevelJsonObject(const String& json,
                             String* keys,
                             String* values,
                             size_t capacity,
                             size_t& count) {
  count = 0;
  if (json.length() < 2 || json[0] != '{') {
    return false;
  }
  int i = 1;
  const int n = json.length();
  auto skipWs = [&](int& p) {
    while (p < n && (json[p] == ' ' || json[p] == '\n' || json[p] == '\r' || json[p] == '\t')) {
      ++p;
    }
  };

  while (i < n) {
    skipWs(i);
    if (i < n && json[i] == '}') {
      return true;
    }
    if (i >= n || json[i] != '\"') {
      return false;
    }
    ++i;
    String key;
    while (i < n) {
      const char c = json[i++];
      if (c == '\\') {
        if (i < n) {
          key += json[i++];
        }
      } else if (c == '\"') {
        break;
      } else {
        key += c;
      }
    }
    skipWs(i);
    if (i >= n || json[i] != ':') {
      return false;
    }
    ++i;
    skipWs(i);
    if (i >= n) {
      return false;
    }
    const int valueStart = i;
    if (json[i] == '\"') {
      ++i;
      while (i < n) {
        const char c = json[i++];
        if (c == '\\') {
          if (i < n) {
            ++i;
          }
        } else if (c == '\"') {
          break;
        }
      }
    } else if (json[i] == '{' || json[i] == '[') {
      const char open = json[i];
      const char close = (open == '{') ? '}' : ']';
      int depth = 0;
      bool inString = false;
      while (i < n) {
        const char c = json[i++];
        if (inString) {
          if (c == '\\') {
            if (i < n) {
              ++i;
            }
          } else if (c == '\"') {
            inString = false;
          }
          continue;
        }
        if (c == '\"') {
          inString = true;
          continue;
        }
        if (c == open) {
          ++depth;
        } else if (c == close) {
          --depth;
          if (depth == 0) {
            break;
          }
        }
      }
    } else {
      while (i < n && json[i] != ',' && json[i] != '}') {
        ++i;
      }
    }
    const int valueEnd = i;
    if (count >= capacity) {
      return false;
    }
    keys[count] = key;
    values[count] = json.substring(valueStart, valueEnd);
    ++count;
    skipWs(i);
    if (i < n && json[i] == ',') {
      ++i;
      continue;
    }
    if (i < n && json[i] == '}') {
      return true;
    }
  }
  return false;
}

struct MotionGuideTemplate {
  uint16_t riseMs = 1100;
  uint16_t lowerMs = 1200;
  uint16_t topPauseMs = 200;
  uint16_t bottomPauseMs = 180;
};

enum class RepQualityTier : uint8_t {
  Bad = 0,
  Ok,
  Good,
  Excellent
};

float repQualityScore(const RepMetrics& rep, float machineIdealRomPercent) {
  const float romNorm = constrain(rep.romPercent / max(1.0f, machineIdealRomPercent), 0.0f, 1.15f);
  const float romScore = constrain(romNorm, 0.0f, 1.0f) * 100.0f;
  const float velScore = constrain(rep.peakVelocityPctPerSec / 120.0f, 0.0f, 1.0f) * 100.0f;
  const float durationSec = rep.durationMs / 1000.0f;
  const float controlScore = (durationSec >= 0.8f && durationSec <= 4.2f) ? 100.0f
                            : (durationSec < 0.8f ? (durationSec / 0.8f) * 100.0f
                                                  : max(0.0f, 100.0f - ((durationSec - 4.2f) * 20.0f)));
  return (romScore * 0.50f) + (velScore * 0.30f) + (controlScore * 0.20f);
}

RepQualityTier repQualityTierFromScore(float score) {
  if (score >= 85.0f) {
    return RepQualityTier::Excellent;
  }
  if (score >= 70.0f) {
    return RepQualityTier::Good;
  }
  if (score >= 50.0f) {
    return RepQualityTier::Ok;
  }
  return RepQualityTier::Bad;
}

const char* repQualityTierText(RepQualityTier tier) {
  switch (tier) {
    case RepQualityTier::Excellent:
      return "excellent";
    case RepQualityTier::Good:
      return "good";
    case RepQualityTier::Ok:
      return "ok";
    case RepQualityTier::Bad:
    default:
      return "bad";
  }
}

MotionGuideTemplate buildMotionGuideTemplate(const MachineProfile* machine,
                                             const GoalRecommendation* recommendation) {
  MotionGuideTemplate out;
  if (recommendation != nullptr) {
    out.riseMs = max<uint16_t>(300, recommendation->riseMs);
    out.lowerMs = max<uint16_t>(300, recommendation->lowerMs);
    out.topPauseMs = min<uint16_t>(1200, recommendation->topPauseMs);
    out.bottomPauseMs = min<uint16_t>(1200, recommendation->bottomPauseMs);

    // Goal-dependent defaults (applied even if cloud fields are not set).
    switch (recommendation->goal) {
      case TrainingGoal::Strength:
        out.riseMs = max<uint16_t>(out.riseMs, 1300);
        out.lowerMs = max<uint16_t>(out.lowerMs, 1550);
        out.topPauseMs = max<uint16_t>(out.topPauseMs, 260);
        out.bottomPauseMs = max<uint16_t>(out.bottomPauseMs, 220);
        break;
      case TrainingGoal::Hypertrophy:
        out.riseMs = max<uint16_t>(out.riseMs, 1050);
        out.lowerMs = max<uint16_t>(out.lowerMs, 1250);
        out.topPauseMs = max<uint16_t>(out.topPauseMs, 160);
        out.bottomPauseMs = max<uint16_t>(out.bottomPauseMs, 140);
        break;
      case TrainingGoal::Endurance:
        out.riseMs = min<uint16_t>(out.riseMs, 900);
        out.lowerMs = min<uint16_t>(out.lowerMs, 980);
        out.topPauseMs = min<uint16_t>(out.topPauseMs, 120);
        out.bottomPauseMs = min<uint16_t>(out.bottomPauseMs, 120);
        break;
      case TrainingGoal::Test:
        out.riseMs = min<uint16_t>(out.riseMs, 950);
        out.lowerMs = min<uint16_t>(out.lowerMs, 980);
        out.topPauseMs = min<uint16_t>(out.topPauseMs, 100);
        out.bottomPauseMs = min<uint16_t>(out.bottomPauseMs, 110);
        break;
      case TrainingGoal::General:
      default:
        break;
    }
  }

  // Exercise-category default shaping is only used when no explicit
  // recommendation is available. If recommendations exist (local or cloud),
  // they are considered admin-tuned and should be respected directly.
  if (recommendation == nullptr && machine != nullptr &&
      String(machine->exerciseCategory).equalsIgnoreCase("cable_machine")) {
    out.lowerMs = static_cast<uint16_t>(out.lowerMs + 120);
  } else if (recommendation == nullptr && machine != nullptr &&
             String(machine->exerciseCategory).equalsIgnoreCase("plate_loaded_machine")) {
    out.riseMs = static_cast<uint16_t>(max<uint16_t>(320, out.riseMs - 80));
  }
  return out;
}

uint32_t motionGuideCycleMs(const MotionGuideTemplate& tpl) {
  return static_cast<uint32_t>(tpl.riseMs) + static_cast<uint32_t>(tpl.topPauseMs) +
         static_cast<uint32_t>(tpl.lowerMs) + static_cast<uint32_t>(tpl.bottomPauseMs);
}

#ifndef SMARTGYM_FIREBASE_DATABASE_URL
#define SMARTGYM_FIREBASE_DATABASE_URL ""
#endif
#ifndef SMARTGYM_FIREBASE_AUTH_TOKEN
#define SMARTGYM_FIREBASE_AUTH_TOKEN ""
#endif
constexpr const char* kFirebaseDatabaseUrl = SMARTGYM_FIREBASE_DATABASE_URL;
constexpr const char* kFirebaseAuthToken = SMARTGYM_FIREBASE_AUTH_TOKEN;
// Monterrey / Nuevo Leon uses UTC-6 year-round in 2026, so avoid DST drift.
constexpr const char* kTimezoneRule = "CST6";

constexpr const char* kMachineOptions =
    "Incline Press\nFlat Bench Press\nCable Fly\nIncline Cable Curl\nPreacher Curl\n"
    "Triceps Pushdown\nOverhead Triceps Extension\nLat Pulldown\nSeated Cable Row\n"
    "Shoulder Press\nCable Lateral Raise\nFace Pull\nLeg Press\nCalf Raise\n"
    "Leg Extension\nSeated Leg Curl\nHip Adductor\nHip Abductor";
constexpr const char* kMachineIds[] = {
    "incline_press_1",
    "flat_bench_press_1",
    "cable_fly_1",
    "incline_cable_curl_1",
    "preacher_curl_1",
    "triceps_pushdown_1",
    "overhead_triceps_extension_1",
    "lat_pulldown_1",
    "seated_cable_row_1",
    "shoulder_press_1",
    "cable_lateral_raise_1",
    "face_pull_1",
    "leg_press_1",
    "calf_raise_1",
    "leg_extension_1",
    "seated_leg_curl_1",
    "hip_adductor_1",
    "hip_abductor_1"
};
constexpr uint8_t kMachineCount = 18;
constexpr uint8_t kCalibrationMinValidReps = 3;
constexpr uint8_t kCalibrationMaxTargetReps = 5;
}

const char* SmartGymTouchApp::logLevelText() const {
  switch (logLevel_) {
    case LogLevel::Quiet: return "QUIET";
    case LogLevel::Verbose: return "VERBOSE";
    case LogLevel::Normal:
    default: return "NORMAL";
  }
}

void SmartGymTouchApp::pushLogTrace(const String& line) {
  logTrace_[logTraceHead_] = line;
  logTraceHead_ = static_cast<uint8_t>((logTraceHead_ + 1U) % kLogTraceSize);
  if (logTraceCount_ < kLogTraceSize) {
    logTraceCount_++;
  }
}

void SmartGymTouchApp::dumpLogTrace(const char* reason) {
  Serial.printf("[CRASH] trace dump begin (%s)\n", reason != nullptr ? reason : "-");
  for (uint8_t i = 0; i < logTraceCount_; ++i) {
    const uint8_t idx = static_cast<uint8_t>((logTraceHead_ + kLogTraceSize - logTraceCount_ + i) % kLogTraceSize);
    Serial.println(logTrace_[idx]);
  }
  Serial.println("[CRASH] trace dump end");
}

void SmartGymTouchApp::logEvent(const char* tag, const String& message, LogLevel level) {
  if (static_cast<uint8_t>(level) > static_cast<uint8_t>(logLevel_)) {
    return;
  }
  const String line = "[" + String(tag) + "] " + message;
  Serial.println(line);
  pushLogTrace(line);
}

void SmartGymTouchApp::logStateIfChanged(uint32_t nowMs) {
  const SessionStage stage = getSessionStage(nowMs);
  if (state_ != lastLoggedState_ || stage != lastLoggedStage_) {
    lastLoggedState_ = state_;
    lastLoggedStage_ = stage;
    logEvent("STATE", String(stateToText()) + " stage=" + String(static_cast<int>(stage)));
  }
}

void SmartGymTouchApp::logHeartbeat(uint32_t nowMs) {
  if (logLevel_ == LogLevel::Quiet) {
    return;
  }
  if (lastHeartbeatLogMs_ != 0 && (nowMs - lastHeartbeatLogMs_) < 2500UL) {
    return;
  }
  lastHeartbeatLogMs_ = nowMs;
  logEvent("HB",
           String("ui=") + String(static_cast<int>(currentUiScreen_)) +
           " st=" + String(stateToText()) +
           " rep=" + String(currentSetRepCount_) +
           " set=" + String(completedSets_) +
           " wifi=" + (cloudEnabled_ && firebaseService_.isWifiConnected() ? "1" : "0") +
           " q=" + String(localPersistenceStore_.getPendingUploadCount()),
           LogLevel::Verbose);
}

const char* SmartGymTouchApp::sessionStageToText(SessionStage stage) const {
  switch (stage) {
    case SessionStage::Idle: return "idle";
    case SessionStage::Identify: return "identify";
    case SessionStage::Calibrate: return "calibrate";
    case SessionStage::Train: return "train";
    case SessionStage::Rest: return "rest";
    case SessionStage::Summary: return "summary";
    case SessionStage::Logout: return "logout";
    default: return "unknown";
  }
}

const char* SmartGymTouchApp::uiScreenToText(UiScreenMode mode) const {
  switch (mode) {
    case UiScreenMode::Main: return "main";
    case UiScreenMode::Summary: return "summary";
    case UiScreenMode::Idle: return "idle";
    case UiScreenMode::Calibration: return "calibration";
    case UiScreenMode::CalibrationGate: return "calibration_gate";
    case UiScreenMode::Debug: return "debug";
    case UiScreenMode::Profile: return "profile";
    default: return "unknown";
  }
}

void SmartGymTouchApp::begin() {
  logBootMemPoint("begin_enter");
  if (cloudMutex_ == nullptr) {
    cloudMutex_ = xSemaphoreCreateRecursiveMutex();
  }
  logBootMemPoint("before_beginCore");
  beginCore();
  logBootMemPoint("after_beginCore");
  initStyles();
  logBootMemPoint("after_initStyles");
  logBootMemPoint("before_ui_ready");
  buildUi();
  logBootMemPoint("after_ui_ready");
  // Boot directly into idle waiting mode.
  state_ = State::Idle;
  idleStartupLock_ = true;
  idleOverlaySuppressedUntilMs_ = 0;
  showUiScreen(UiScreenMode::Idle);
  setStatusMessage("Ready. Tap screen, scan RFID, or move machine.");
  refreshUi();
  tickTimer_ = lv_timer_create(onTick, kTickIntervalMs, this);
}

void SmartGymTouchApp::beginCore() {
  logBootMemPoint("before_machine_registry");
  const esp_reset_reason_t rr = esp_reset_reason();
  logEvent("BOOT", String("reset_reason=") + String(static_cast<int>(rr)));
  if (rr == ESP_RST_PANIC || rr == ESP_RST_TASK_WDT || rr == ESP_RST_INT_WDT) {
    dumpLogTrace("previous reset was panic/wdt");
  }

  hardwareSensorEnabled_ = kEnableHardwareSensor;
  hardwareRfidEnabled_ = kEnableHardwareRfid;
  sensorSimulationEnabled_ = !hardwareSensorEnabled_;

  deviceConfigStore_.begin();
  localPersistenceStore_.begin();
  const uint16_t queueRepairMaxPayloadBytes =
      kUseFirebaseClientQueuedTransport ? kFirebaseClientPatchPayloadHardMaxBytes : kNvsUploadMaxPayloadBytes;
  localPersistenceStore_.repairUploadQueue(queueRepairMaxPayloadBytes, false);
  userRegistry_.begin();
  localPersistenceStore_.loadUsers(userRegistry_);
  logBootMemPoint("after_machine_registry");
  fatQueueReady_ = false;

  const String storedMachineId = deviceConfigStore_.loadMachineId();
  machineProfile_ = machineRegistry_.findById(storedMachineId);
  if (machineProfile_ == nullptr) {
    machineProfile_ = machineRegistry_.getDefault();
  }
  if (machineProfile_ != nullptr) {
    logEvent("CFG",
             String("using embedded machine catalog machineId=") + machineProfile_->machineId +
                 " machineTypeId=" + machineProfile_->machineTypeId,
             LogLevel::Normal);
  } else {
    logEvent("CFG", "using embedded machine catalog (no machine selected)", LogLevel::Normal);
  }
  bootMachineRestorePending_ = storedMachineId.isEmpty();
  if (machineProfile_ != nullptr) {
    selectedWeightKg_ = machineProfile_->defaultCalibrationWeightKg;
    logEvent("WEIGHT",
             String("machine pin init kg=") + String(selectedWeightKg_, 1) +
                 " source=machine_default",
             LogLevel::Normal);
  }

  deviceEncoderCalibrationValid_ = false;
  encoderZeroCaptured_ = false;
  encoderFullCaptured_ = false;
  encoderZeroRaw_ = 0;
  encoderFullRaw_ = 0;
  encoderReferenceDistanceMm_ = 1000.0f;
  encoderCalibrationReady_ = false;
  encoderCalibrationSummary_ = "Encoder calibration: not captured.";

  bool localEncoderValid = false;
  if (deviceConfigStore_.loadEncoderCalibration(localEncoderValid,
                                                deviceEncoderZeroRaw_,
                                                deviceEncoderFullRaw_,
                                                encoderReferenceDistanceMm_,
                                                deviceEncoderInvertDirection_) &&
      localEncoderValid) {
    deviceEncoderCalibrationValid_ = true;
    encoderZeroCaptured_ = true;
    encoderFullCaptured_ = true;
    encoderZeroRaw_ = deviceEncoderZeroRaw_;
    encoderFullRaw_ = deviceEncoderFullRaw_;
    encoderCalibrationReady_ = true;
  encoderCalibrationSummary_ =
      "Encoder limits restored: min " + String(encoderZeroRaw_) +
      ", max " + String(encoderFullRaw_) +
      ", distance " + String(encoderReferenceDistanceMm_, 0) + "mm";
  }

  if (kEnableCloud) {
    FirebaseRuntimeConfig firebaseConfig;
    firebaseConfig.wifiPrimarySsid = kWifiPrimarySsid;
    firebaseConfig.wifiPrimaryPassword = kWifiPrimaryPassword;
    firebaseConfig.wifiSecondarySsid = kWifiSecondarySsid;
    firebaseConfig.wifiSecondaryPassword = kWifiSecondaryPassword;
    firebaseConfig.databaseUrl = kFirebaseDatabaseUrl;
    firebaseConfig.authToken = kFirebaseAuthToken;
    firebaseConfig.timezoneRule = kTimezoneRule;
    firebaseConfig.machinePollIntervalMs = kMachinePollIntervalMs;
    firebaseService_.begin(firebaseConfig, timeService_);
    firebaseService_.setUseFirebaseClientCloudReads(kUseFirebaseClientCloudReads);
    firebaseService_.setUseFirebaseClientDeviceWrites(kUseFirebaseClientDeviceWrites);
    firebaseService_.setUseFirebaseClientProfileWrites(kUseFirebaseClientProfileWrites);
    cloudEnabled_ = firebaseService_.isEnabled();
    if (cloudEnabled_ && String(kFirebaseAuthToken).isEmpty()) {
      Serial.println("[Cloud] auth token empty: using unauthenticated RTDB mode (rules must allow it).");
    }
  } else {
    cloudEnabled_ = false;
  }
  if (!cloudEnabled_) {
    timeService_.begin(kTimezoneRule);
  }
  logEvent("WIFI", String("cloud=") + (cloudEnabled_ ? "on" : "off"));
  // Recovery mode defaults to single-context cloud sync. If worker mode is
  // re-enabled later, the worker must start only after app dependencies are
  // initialized and owned consistently.
  startSyncWorker();
  startUploadTransportWorker();

  if (hardwareSensorEnabled_) {
    sensorManager_.begin(kSensorPin, kSensorSampleIntervalMs);
    sensorManager_.setCalibrationRange(1200, 2800);
    sensorManager_.enableAutoRange(true);
    sensorManager_.setInvertDirection(false);
    if (machineProfile_ != nullptr) {
      sensorManager_.setStrokeLengthMm(machineProfile_->strokeLengthMm);
    }
  }

  if (hardwareRfidEnabled_) {
    rfidService_.begin(kRfidSsPin, kRfidRstPin, kRfidSckPin, kRfidMisoPin, kRfidMosiPin);
    rfidInitialized_ = rfidService_.isReady();
    if (rfidInitialized_) {
      Serial.printf("[RFID] initialized at boot version=0x%02X\n", rfidService_.readerVersion());
      logEvent("RFID", String("initialized version=0x") + String(rfidService_.readerVersion(), HEX));
    } else {
      Serial.printf("[RFID] init failed. Check wiring/pins. version=0x%02X\n", rfidService_.readerVersion());
      logEvent("RFID", String("init failed version=0x") + String(rfidService_.readerVersion(), HEX));
    }
  }

  logBootMemPoint("before_calibration_init");
  repDetector_.begin(20.0f, 80.0f, 60.0f);
  configureRepDetectorThresholds();
  setStatusMessage("Ready. SmartGym display runtime online.");
  lastSessionSummary_ = "No completed session yet.";
  lastScannedUid_ = "None";
  uiCache_ = UiCache{};
  lastSensorReading_ = SensorReading{};
  sessionStartMs_ = 0;
  lastSetCompletedMs_ = 0;
  activeSessionTargetSets_ = 0;
  activeSessionTargetRepsMin_ = 0;
  activeSessionTargetRepsMax_ = 0;
  activeSessionRestSeconds_ = 0;
  hasLastCompletedRep_ = false;
  lastRepSummary_ = "Awaiting first rep.";
  pendingMachineConfigUpload_ = false;
  lastUserActivityMs_ = millis();
  lastWeightAdjustMs_ = 0;
  idealGraphStartMs_ = millis();

  if (cloudEnabled_) {
    const uint32_t nowMs = millis();
    lastDeviceHeartbeatMs_ = nowMs;
    lastMachineCloudPollMs_ = nowMs;
    lastUploadRetryMs_ = nowMs;
  }

  applyMachineSensorCalibration();

  startupSelfTestDone_ = true;
  startupSelfTestSummary_ = "SELFTEST ";
  startupSelfTestSummary_ += (machineProfile_ != nullptr) ? "MACH OK" : "MACH FAIL";
  startupSelfTestSummary_ += " | ";
  startupSelfTestSummary_ += (!hardwareRfidEnabled_ || rfidInitialized_) ? "RFID OK" : "RFID WAIT";
  startupSelfTestSummary_ += " | ";
  startupSelfTestSummary_ += hardwareSensorEnabled_ ? "ENC LIVE" : "ENC SIM";
  if (cloudEnabled_) {
    startupSelfTestSummary_ += " | WIFI ";
    startupSelfTestSummary_ += firebaseService_.isWifiConnected() ? "OK" : "SEARCH";
  }
  logBootMemPoint("after_calibration_init");
}

void SmartGymTouchApp::initStyles() {
  lv_style_init(&styleScreen_);
  lv_style_set_bg_color(&styleScreen_, lv_color_hex(0x05080B));
  lv_style_set_bg_grad_color(&styleScreen_, lv_color_hex(0x0C1115));
  lv_style_set_bg_grad_dir(&styleScreen_, LV_GRAD_DIR_VER);
  lv_style_set_bg_opa(&styleScreen_, LV_OPA_COVER);
  lv_style_set_pad_all(&styleScreen_, 0);
  lv_style_set_border_width(&styleScreen_, 0);
  lv_style_set_radius(&styleScreen_, 0);

  lv_style_init(&stylePanel_);
  lv_style_set_bg_color(&stylePanel_, lv_color_hex(0x0D141A));
  lv_style_set_bg_opa(&stylePanel_, LV_OPA_COVER);
  lv_style_set_radius(&stylePanel_, 14);
  lv_style_set_border_width(&stylePanel_, 1);
  lv_style_set_border_color(&stylePanel_, lv_color_hex(0x1D2930));
  lv_style_set_pad_all(&stylePanel_, 14);

  lv_style_init(&stylePanelDark_);
  lv_style_set_bg_color(&stylePanelDark_, lv_color_hex(0x0A1015));
  lv_style_set_bg_opa(&stylePanelDark_, LV_OPA_COVER);
  lv_style_set_radius(&stylePanelDark_, 14);
  lv_style_set_border_width(&stylePanelDark_, 1);
  lv_style_set_border_color(&stylePanelDark_, lv_color_hex(0x182229));
  lv_style_set_pad_all(&stylePanelDark_, 14);

  lv_style_init(&styleHeader_);
  lv_style_set_bg_color(&styleHeader_, lv_color_hex(0x060B0F));
  lv_style_set_bg_opa(&styleHeader_, LV_OPA_COVER);
  lv_style_set_border_width(&styleHeader_, 1);
  lv_style_set_border_color(&styleHeader_, lv_color_hex(0x172027));
  lv_style_set_radius(&styleHeader_, 0);
  lv_style_set_pad_all(&styleHeader_, 8);

  lv_style_init(&styleAccentCard_);
  lv_style_set_bg_color(&styleAccentCard_, lv_color_hex(0x071517));
  lv_style_set_bg_grad_color(&styleAccentCard_, lv_color_hex(0x0D2024));
  lv_style_set_bg_grad_dir(&styleAccentCard_, LV_GRAD_DIR_VER);
  lv_style_set_radius(&styleAccentCard_, 14);
  lv_style_set_border_width(&styleAccentCard_, 1);
  lv_style_set_border_color(&styleAccentCard_, lv_color_hex(0x1D9DAA));
  lv_style_set_pad_all(&styleAccentCard_, 14);

  lv_style_init(&styleButtonPrimary_);
  lv_style_set_radius(&styleButtonPrimary_, 18);
  lv_style_set_bg_color(&styleButtonPrimary_, lv_color_hex(0x1DAA93));
  lv_style_set_bg_opa(&styleButtonPrimary_, LV_OPA_COVER);
  lv_style_set_text_color(&styleButtonPrimary_, lv_color_hex(0x06110E));
  lv_style_set_pad_all(&styleButtonPrimary_, 12);
  lv_style_set_border_width(&styleButtonPrimary_, 0);

  lv_style_init(&styleButtonSecondary_);
  lv_style_set_radius(&styleButtonSecondary_, 18);
  lv_style_set_bg_color(&styleButtonSecondary_, lv_color_hex(0x151E24));
  lv_style_set_bg_opa(&styleButtonSecondary_, LV_OPA_COVER);
  lv_style_set_text_color(&styleButtonSecondary_, lv_color_hex(0xD7E7EC));
  lv_style_set_pad_all(&styleButtonSecondary_, 12);
  lv_style_set_border_width(&styleButtonSecondary_, 1);
  lv_style_set_border_color(&styleButtonSecondary_, lv_color_hex(0x26343D));

  lv_style_init(&styleButtonDanger_);
  lv_style_set_radius(&styleButtonDanger_, 18);
  lv_style_set_bg_color(&styleButtonDanger_, lv_color_hex(0xC84D4D));
  lv_style_set_bg_opa(&styleButtonDanger_, LV_OPA_COVER);
  lv_style_set_text_color(&styleButtonDanger_, lv_color_hex(0xFFFFFF));
  lv_style_set_pad_all(&styleButtonDanger_, 12);
  lv_style_set_border_width(&styleButtonDanger_, 0);

  lv_style_init(&styleButtonGhost_);
  lv_style_set_radius(&styleButtonGhost_, 18);
  lv_style_set_bg_opa(&styleButtonGhost_, LV_OPA_TRANSP);
  lv_style_set_border_width(&styleButtonGhost_, 1);
  lv_style_set_border_color(&styleButtonGhost_, lv_color_hex(0x2C3A43));
  lv_style_set_text_color(&styleButtonGhost_, lv_color_hex(0x9FB2BA));
  lv_style_set_pad_all(&styleButtonGhost_, 10);

  lv_style_init(&styleModal_);
  lv_style_set_bg_color(&styleModal_, lv_color_hex(0x020405));
  lv_style_set_bg_opa(&styleModal_, LV_OPA_COVER);
  lv_style_set_border_width(&styleModal_, 0);
  lv_style_set_radius(&styleModal_, 0);

  lv_style_init(&stylePill_);
  lv_style_set_bg_color(&stylePill_, lv_color_hex(0x11181D));
  lv_style_set_radius(&stylePill_, 999);
  lv_style_set_pad_left(&stylePill_, 10);
  lv_style_set_pad_right(&stylePill_, 10);
  lv_style_set_pad_top(&stylePill_, 5);
  lv_style_set_pad_bottom(&stylePill_, 5);
  lv_style_set_border_width(&stylePill_, 1);
  lv_style_set_border_color(&stylePill_, lv_color_hex(0x26323A));
}

lv_obj_t* SmartGymTouchApp::createPanel(lv_obj_t* parent, bool dark) {
  lv_obj_t* obj = lv_obj_create(parent);
  lv_obj_remove_style_all(obj);
  lv_obj_add_style(obj, dark ? &stylePanelDark_ : &stylePanel_, 0);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  return obj;
}

lv_obj_t* SmartGymTouchApp::createButton(lv_obj_t* parent, const char* text, int kind) {
  lv_obj_t* btn = lv_btn_create(parent);
  lv_obj_remove_style_all(btn);

  if (kind == kBtnPrimary) {
    lv_obj_add_style(btn, &styleButtonPrimary_, 0);
  } else if (kind == kBtnDanger) {
    lv_obj_add_style(btn, &styleButtonDanger_, 0);
  } else if (kind == kBtnGhost) {
    lv_obj_add_style(btn, &styleButtonGhost_, 0);
  } else {
    lv_obj_add_style(btn, &styleButtonSecondary_, 0);
  }

  lv_obj_add_event_cb(btn, onButtonEvent, LV_EVENT_SHORT_CLICKED, this);
  lv_obj_t* label = lv_label_create(btn);
  lv_label_set_text(label, text);
  lv_obj_set_width(label, LV_PCT(100));
  lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
  lv_obj_center(label);
  return btn;
}

bool SmartGymTouchApp::isValidLvObj(lv_obj_t* obj) const {
  return obj != nullptr && lv_obj_is_valid(obj);
}

lv_obj_t* SmartGymTouchApp::getButtonLabelIfValid(lv_obj_t* btn) const {
  if (!isValidLvObj(btn)) {
    return nullptr;
  }
  lv_obj_t* label = lv_obj_get_child(btn, 0);
  return isValidLvObj(label) ? label : nullptr;
}

void SmartGymTouchApp::setButtonTextIfValid(lv_obj_t* btn, const char* text) {
  lv_obj_t* label = getButtonLabelIfValid(btn);
  if (label != nullptr) {
    lv_label_set_text(label, text != nullptr ? text : "");
  }
}

void SmartGymTouchApp::adjustCurrentLoad(float deltaKg, const char* source) {
  const uint32_t nowMs = millis();
  if ((nowMs - lastWeightAdjustMs_) < 200UL) {
    return;
  }
  const bool trainingActive = sessionRecorder_.isActive() || state_ == State::Training;
  if (!trainingActive && state_ != State::Calibration && !canStartTrainingNow(nowMs)) {
    return;
  }

  lastWeightAdjustMs_ = nowMs;
  const float oldKg = selectedWeightKg_;
  const float minKg = machineProfile_ != nullptr && machineProfile_->machineMinKg > 0.0f
                          ? machineProfile_->machineMinKg
                          : kMinWeightKg;
  const float maxKg = machineProfile_ != nullptr && machineProfile_->machineMaxKg > minKg
                          ? machineProfile_->machineMaxKg
                          : kMaxWeightKg;
  const float newKg = roundWeightToMachineIncrement(constrain(selectedWeightKg_ + deltaKg, minKg, maxKg));
  if (fabsf(newKg - oldKg) < 0.01f) {
    return;
  }
  selectedWeightKg_ = newKg;
  const char* stateText = trainingActive ? "training" : (state_ == State::Calibration ? "calibration" : "ready");
  logEvent("WEIGHT",
           String("machine pin changed oldKg=") + String(oldKg, 1) +
               " newKg=" + String(selectedWeightKg_, 1) +
               " source=" + (source != nullptr ? String(source) : String("unknown")) +
               " delta=" + String(deltaKg, 1) +
               " state=" + stateText +
               " set=" + String(completedSets_ + 1) +
               " rep=" + String(currentSetRepCount_),
           LogLevel::Normal);
  if (state_ == State::Calibration) {
    const float suggestedKg = calibrationFlowState_ == CalibrationFlowState::AskNextSet &&
                                      calibrationNextWeightKg_ > 0.0f
                                  ? calibrationNextWeightKg_
                                  : calibrationSuggestedStartWeightKg_;
    logEvent("CAL_UI",
             String("pin load updated kg=") + String(selectedWeightKg_, 1) +
                 " suggestedKg=" + String(suggestedKg, 1),
             LogLevel::Normal);
  }
  setStatusMessage(deltaKg >= 0.0f ? "Pin load increased." : "Pin load decreased.");
  refreshUi();
}

bool SmartGymTouchApp::acceptButtonAction(const char* action, uint32_t debounceMs) {
  const uint32_t nowMs = millis();
  const String actionName = action != nullptr ? String(action) : String("unknown");
  const uint32_t dtMs = nowMs - lastLogicalButtonActionMs_;
  if (actionName == lastLogicalButtonAction_ && dtMs < debounceMs) {
    logEvent("BTN",
             String("ignored reason=debounce action=") + actionName +
                 " dtMs=" + String(dtMs),
             LogLevel::Normal);
    return false;
  }
  if (actionName.startsWith("calibration_") && nowMs < calibrationActionBusyUntilMs_) {
    logEvent("CAL",
             String("action ignored reason=transition_busy action=") + actionName,
             LogLevel::Normal);
    return false;
  }
  lastLogicalButtonAction_ = actionName;
  lastLogicalButtonActionMs_ = nowMs;
  logEvent("BTN",
           String("accepted action=") + actionName +
               " dtMs=" + String(dtMs),
           LogLevel::Normal);
  return true;
}

lv_obj_t* SmartGymTouchApp::createValueTile(lv_obj_t* parent, const char* title, lv_obj_t** valueOut) {
  lv_obj_t* tile = createPanel(parent, true);
  lv_obj_set_size(tile, 130, 96);
  lv_obj_set_layout(tile, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(tile, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(tile, 8, 0);

  lv_obj_t* titleLabel = lv_label_create(tile);
  lv_label_set_text(titleLabel, title);
  lv_obj_set_width(titleLabel, LV_PCT(100));
  lv_label_set_long_mode(titleLabel, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_align(titleLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(titleLabel, lv_color_hex(0x8EA5B3), 0);
  lv_obj_set_style_text_font(titleLabel, &lv_font_montserrat_16, 0);

  *valueOut = lv_label_create(tile);
  lv_obj_set_width(*valueOut, LV_PCT(100));
  lv_label_set_long_mode(*valueOut, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_align(*valueOut, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(*valueOut, lv_color_hex(0xF4FBFF), 0);
  lv_obj_set_style_text_font(*valueOut, &lv_font_montserrat_24, 0);
  lv_label_set_text(*valueOut, "--");

  return tile;
}

void SmartGymTouchApp::buildUi() {
  screen_ = lv_obj_create(nullptr);
  lv_obj_remove_style_all(screen_);
  lv_obj_add_style(screen_, &styleScreen_, 0);
  lv_obj_set_size(screen_, 800, 480);
  lv_obj_clear_flag(screen_, LV_OBJ_FLAG_SCROLLABLE);

  logBootMemPoint("before_buildHeader");
  buildHeader(screen_);
  logBootMemPoint("after_buildHeader");
  logBootMemPoint("before_buildMainPanels");
  buildMainPanels(screen_);
  logBootMemPoint("after_buildMainPanels");
  logBootMemPoint("before_buildRestOverlay");
  buildRestOverlay(screen_);
  logBootMemPoint("after_buildRestOverlay");
  // Keep boot lean: heavy/secondary screens are created lazily on first use.
  logBootMemPoint("defer_optional_ui");

  lv_scr_load(screen_);
}

void SmartGymTouchApp::buildIdleOverlay(lv_obj_t* parent) {
  (void)parent;
  if (idleOverlay_ != nullptr) {
    return;
  }

  idleOverlay_ = lv_obj_create(nullptr);
  lv_obj_remove_style_all(idleOverlay_);
  lv_obj_add_style(idleOverlay_, &styleModal_, 0);
  lv_obj_set_size(idleOverlay_, 800, 480);
  lv_obj_clear_flag(idleOverlay_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(idleOverlay_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(idleOverlay_, onIdleOverlayEvent, LV_EVENT_PRESSED, this);

  idlePanel_ = createPanel(idleOverlay_, true);
  lv_obj_set_size(idlePanel_, 600, 300);
  lv_obj_center(idlePanel_);
  lv_obj_add_flag(idlePanel_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(idlePanel_, onIdleOverlayEvent, LV_EVENT_PRESSED, this);
  lv_obj_set_style_bg_color(idlePanel_, lv_color_hex(0x091217), 0);
  lv_obj_set_style_bg_grad_color(idlePanel_, lv_color_hex(0x091217), 0);
  lv_obj_set_style_bg_grad_dir(idlePanel_, LV_GRAD_DIR_NONE, 0);
  lv_obj_set_style_border_color(idlePanel_, lv_color_hex(0x1F4E56), 0);
  lv_obj_set_style_shadow_width(idlePanel_, 24, 0);
  lv_obj_set_style_shadow_color(idlePanel_, lv_color_hex(0x041014), 0);
  lv_obj_set_style_shadow_opa(idlePanel_, LV_OPA_50, 0);

  idleBadgeLabel_ = lv_label_create(idlePanel_);
  lv_label_set_text(idleBadgeLabel_, "SMARTGYM CLUB");
  lv_obj_set_style_bg_opa(idleBadgeLabel_, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(idleBadgeLabel_, lv_color_hex(0x0E1B20), 0);
  lv_obj_set_style_border_width(idleBadgeLabel_, 1, 0);
  lv_obj_set_style_border_color(idleBadgeLabel_, lv_color_hex(0x24454D), 0);
  lv_obj_set_style_radius(idleBadgeLabel_, 999, 0);
  lv_obj_set_style_pad_hor(idleBadgeLabel_, 12, 0);
  lv_obj_set_style_pad_ver(idleBadgeLabel_, 5, 0);
  lv_obj_set_style_text_font(idleBadgeLabel_, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(idleBadgeLabel_, lv_color_hex(0x8FD9D2), 0);
  lv_obj_align(idleBadgeLabel_, LV_ALIGN_TOP_MID, 0, 8);
  lv_obj_add_flag(idleBadgeLabel_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(idleBadgeLabel_, onIdleOverlayEvent, LV_EVENT_PRESSED, this);

  idleTitleLabel_ = lv_label_create(idlePanel_);
  lv_label_set_text(idleTitleLabel_, "WELCOME BACK");
  lv_obj_set_style_text_font(idleTitleLabel_, &lv_font_montserrat_30, 0);
  lv_obj_set_style_text_color(idleTitleLabel_, lv_color_hex(0xF2FBFF), 0);
  lv_obj_align(idleTitleLabel_, LV_ALIGN_TOP_MID, 0, 48);
  lv_obj_add_flag(idleTitleLabel_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(idleTitleLabel_, onIdleOverlayEvent, LV_EVENT_PRESSED, this);

  idleMachineLabel_ = lv_label_create(idlePanel_);
  lv_obj_set_width(idleMachineLabel_, 540);
  lv_label_set_long_mode(idleMachineLabel_, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(idleMachineLabel_, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(idleMachineLabel_, lv_color_hex(0x7DD2C4), 0);
  lv_obj_set_style_text_align(idleMachineLabel_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(idleMachineLabel_, LV_ALIGN_TOP_MID, 0, 94);
  lv_obj_add_flag(idleMachineLabel_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(idleMachineLabel_, onIdleOverlayEvent, LV_EVENT_PRESSED, this);

  idlePromptLabel_ = lv_label_create(idlePanel_);
  lv_obj_set_width(idlePromptLabel_, 540);
  lv_label_set_long_mode(idlePromptLabel_, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(idlePromptLabel_, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(idlePromptLabel_, lv_color_hex(0xEAF3F6), 0);
  lv_obj_set_style_text_align(idlePromptLabel_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(idlePromptLabel_, LV_ALIGN_TOP_MID, 0, 142);
  lv_obj_add_flag(idlePromptLabel_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(idlePromptLabel_, onIdleOverlayEvent, LV_EVENT_PRESSED, this);

  idleHintLabel_ = lv_label_create(idlePanel_);
  lv_obj_set_width(idleHintLabel_, 540);
  lv_label_set_long_mode(idleHintLabel_, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(idleHintLabel_, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(idleHintLabel_, lv_color_hex(0x9FB3BD), 0);
  lv_obj_set_style_text_align(idleHintLabel_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(idleHintLabel_, LV_ALIGN_BOTTOM_MID, 0, -18);
  lv_obj_add_flag(idleHintLabel_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(idleHintLabel_, onIdleOverlayEvent, LV_EVENT_PRESSED, this);
}

void SmartGymTouchApp::buildUserSwitchPrompt(lv_obj_t* parent) {
  if (userSwitchPrompt_ != nullptr) {
    return;
  }

  userSwitchPrompt_ = lv_obj_create(parent);
  lv_obj_remove_style_all(userSwitchPrompt_);
  lv_obj_add_style(userSwitchPrompt_, &styleModal_, 0);
  lv_obj_set_size(userSwitchPrompt_, 800, 480);
  lv_obj_add_flag(userSwitchPrompt_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(userSwitchPrompt_, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* panel = createPanel(userSwitchPrompt_, true);
  lv_obj_set_size(panel, 620, 260);
  lv_obj_center(panel);
  lv_obj_set_style_bg_color(panel, lv_color_hex(0x091217), 0);
  lv_obj_set_style_bg_grad_color(panel, lv_color_hex(0x0D1820), 0);
  lv_obj_set_style_bg_grad_dir(panel, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_border_color(panel, lv_color_hex(0xB34A4A), 0);
  lv_obj_set_style_shadow_width(panel, 24, 0);
  lv_obj_set_style_shadow_color(panel, lv_color_hex(0x180607), 0);

  userSwitchPromptTitle_ = lv_label_create(panel);
  lv_label_set_text(userSwitchPromptTitle_, "CAMBIAR USUARIO?");
  lv_obj_set_style_text_font(userSwitchPromptTitle_, &lv_font_montserrat_26, 0);
  lv_obj_set_style_text_color(userSwitchPromptTitle_, lv_color_hex(0xFFE5E5), 0);
  lv_obj_align(userSwitchPromptTitle_, LV_ALIGN_TOP_MID, 0, 24);

  userSwitchPromptBody_ = lv_label_create(panel);
  lv_obj_set_width(userSwitchPromptBody_, 540);
  lv_label_set_long_mode(userSwitchPromptBody_, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(userSwitchPromptBody_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(userSwitchPromptBody_, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(userSwitchPromptBody_, lv_color_hex(0xEAF3F6), 0);
  lv_obj_align(userSwitchPromptBody_, LV_ALIGN_TOP_MID, 0, 78);
  lv_label_set_text(userSwitchPromptBody_,
                    "Se detecto otra tarjeta.\nQuieres terminar la sesion actual y continuar con otro usuario?");

  btnUserSwitchCancel_ = createButton(panel, "CANCELAR", kBtnSecondary);
  lv_obj_set_size(btnUserSwitchCancel_, 210, 52);
  lv_obj_align(btnUserSwitchCancel_, LV_ALIGN_BOTTOM_LEFT, 58, -24);

  btnUserSwitchConfirm_ = createButton(panel, "CONTINUAR", kBtnDanger);
  lv_obj_set_size(btnUserSwitchConfirm_, 250, 52);
  lv_obj_align(btnUserSwitchConfirm_, LV_ALIGN_BOTTOM_RIGHT, -58, -24);
}

void SmartGymTouchApp::buildUserLoadingPopup() {
  if (userLoadingScreen_ != nullptr) {
    return;
  }

  userLoadingScreen_ = lv_obj_create(nullptr);
  lv_obj_remove_style_all(userLoadingScreen_);
  lv_obj_add_style(userLoadingScreen_, &styleModal_, 0);
  lv_obj_set_size(userLoadingScreen_, 800, 480);
  lv_obj_clear_flag(userLoadingScreen_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(userLoadingScreen_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_opa(userLoadingScreen_, LV_OPA_70, 0);
  lv_obj_set_style_bg_color(userLoadingScreen_, lv_color_hex(0x02070A), 0);

  userLoadingPanel_ = createPanel(userLoadingScreen_, true);
  lv_obj_set_size(userLoadingPanel_, 420, 190);
  lv_obj_center(userLoadingPanel_);
  lv_obj_set_style_bg_color(userLoadingPanel_, lv_color_hex(0x071318), 0);
  lv_obj_set_style_border_color(userLoadingPanel_, lv_color_hex(0x1D9DAA), 0);
  lv_obj_set_style_shadow_width(userLoadingPanel_, 22, 0);
  lv_obj_set_style_shadow_color(userLoadingPanel_, lv_color_hex(0x031014), 0);
  lv_obj_set_style_shadow_opa(userLoadingPanel_, LV_OPA_60, 0);

  userLoadingTitleLabel_ = lv_label_create(userLoadingPanel_);
  lv_obj_set_width(userLoadingTitleLabel_, 360);
  lv_label_set_long_mode(userLoadingTitleLabel_, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_align(userLoadingTitleLabel_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(userLoadingTitleLabel_, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(userLoadingTitleLabel_, lv_color_hex(0xF2FBFF), 0);
  lv_obj_align(userLoadingTitleLabel_, LV_ALIGN_TOP_MID, 0, 48);
  lv_label_set_text(userLoadingTitleLabel_, "Loading user...");

  userLoadingStageLabel_ = lv_label_create(userLoadingPanel_);
  lv_obj_set_width(userLoadingStageLabel_, 360);
  lv_label_set_long_mode(userLoadingStageLabel_, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_align(userLoadingStageLabel_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(userLoadingStageLabel_, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(userLoadingStageLabel_, lv_color_hex(0x8FD9D2), 0);
  lv_obj_align(userLoadingStageLabel_, LV_ALIGN_TOP_MID, 0, 104);
  lv_label_set_text(userLoadingStageLabel_, "Loading recommendations...");
}

void SmartGymTouchApp::showUserLoadingPopup(const String& uid, const char* stage) {
  buildUserLoadingPopup();
  if (userLoadingScreen_ == nullptr) {
    return;
  }
  userLoadingReturnScreen_ = currentUiScreen_;
  const String title = "Loading user...";
  lv_label_set_text(userLoadingTitleLabel_, title.c_str());
  updateUserLoadingPopupStage(uid, stage != nullptr ? stage : "profile");
  lv_scr_load(userLoadingScreen_);
  lv_refr_now(nullptr);
  logEvent("UI_LOADING", String("show reason=rfid uid=") + uid, LogLevel::Normal);
}

void SmartGymTouchApp::updateUserLoadingPopupStage(const String& uid, const char* stage) {
  if (userLoadingScreen_ == nullptr) {
    return;
  }
  const String stageText = stage != nullptr ? String(stage) : String("profile");
  if (userLoadingStageLabel_ != nullptr) {
    if (stageText == "profile") {
      lv_label_set_text(userLoadingStageLabel_, "Loading profile...");
    } else if (stageText == "recommendation") {
      lv_label_set_text(userLoadingStageLabel_, "Loading recommendations...");
    } else if (stageText == "applying") {
      lv_label_set_text(userLoadingStageLabel_, "Applying calibration...");
    } else {
      lv_label_set_text(userLoadingStageLabel_, stageText.c_str());
    }
  }
  if (userLoadingTitleLabel_ != nullptr && activeUser_ != nullptr && !activeUser_->displayName.isEmpty()) {
    const String title = "Loading " + activeUser_->displayName + "...";
    lv_label_set_text(userLoadingTitleLabel_, title.c_str());
  }
  lv_refr_now(nullptr);
  logEvent("UI_LOADING", String("stage=") + stageText + " uid=" + uid, LogLevel::Normal);
}

void SmartGymTouchApp::hideUserLoadingPopup(const String& uid, const char* result) {
  if (userLoadingScreen_ != nullptr && lv_scr_act() == userLoadingScreen_) {
    lv_scr_load(screen_);
    currentUiScreen_ = UiScreenMode::Main;
  }
  logEvent("UI_LOADING",
           String("hide result=") + (result != nullptr ? String(result) : String("ok")) +
               " uid=" + uid,
           LogLevel::Normal);
}

void SmartGymTouchApp::buildSummaryScreen(lv_obj_t* parent) {
  (void)parent;
  if (summaryScreen_ != nullptr) {
    return;
  }

  summaryScreen_ = lv_obj_create(nullptr);
  lv_obj_remove_style_all(summaryScreen_);
  lv_obj_add_style(summaryScreen_, &styleModal_, 0);
  lv_obj_set_size(summaryScreen_, 800, 480);
  lv_obj_clear_flag(summaryScreen_, LV_OBJ_FLAG_SCROLLABLE);

  summaryPanel_ = createPanel(summaryScreen_, true);
  lv_obj_set_size(summaryPanel_, 720, 398);
  lv_obj_center(summaryPanel_);
  lv_obj_set_style_bg_color(summaryPanel_, lv_color_hex(0x091217), 0);
  lv_obj_set_style_bg_grad_color(summaryPanel_, lv_color_hex(0x0D1820), 0);
  lv_obj_set_style_bg_grad_dir(summaryPanel_, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_border_color(summaryPanel_, lv_color_hex(0x1F4E56), 0);
  lv_obj_set_style_shadow_width(summaryPanel_, 18, 0);
  lv_obj_set_style_shadow_color(summaryPanel_, lv_color_hex(0x041014), 0);
  lv_obj_set_style_shadow_opa(summaryPanel_, LV_OPA_60, 0);

  summaryBadgeLabel_ = lv_label_create(summaryPanel_);
  lv_label_set_text(summaryBadgeLabel_, "WORKOUT SAVED");
  lv_obj_set_style_bg_opa(summaryBadgeLabel_, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(summaryBadgeLabel_, lv_color_hex(0x102026), 0);
  lv_obj_set_style_border_width(summaryBadgeLabel_, 1, 0);
  lv_obj_set_style_border_color(summaryBadgeLabel_, lv_color_hex(0x2E4553), 0);
  lv_obj_set_style_radius(summaryBadgeLabel_, 999, 0);
  lv_obj_set_style_pad_hor(summaryBadgeLabel_, 14, 0);
  lv_obj_set_style_pad_ver(summaryBadgeLabel_, 5, 0);
  lv_obj_set_style_text_font(summaryBadgeLabel_, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(summaryBadgeLabel_, lv_color_hex(0x8FD9D2), 0);
  lv_obj_align(summaryBadgeLabel_, LV_ALIGN_TOP_LEFT, 18, 14);

  summaryTitleLabel_ = lv_label_create(summaryPanel_);
  lv_label_set_text(summaryTitleLabel_, "Workout Summary");
  lv_obj_set_style_text_font(summaryTitleLabel_, &lv_font_montserrat_30, 0);
  lv_obj_set_style_text_color(summaryTitleLabel_, lv_color_hex(0xF2FBFF), 0);
  lv_obj_align(summaryTitleLabel_, LV_ALIGN_TOP_LEFT, 18, 44);

  summaryUserLabel_ = lv_label_create(summaryPanel_);
  lv_obj_set_width(summaryUserLabel_, 300);
  lv_label_set_long_mode(summaryUserLabel_, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_font(summaryUserLabel_, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(summaryUserLabel_, lv_color_hex(0x8FB0BE), 0);
  lv_obj_set_style_text_align(summaryUserLabel_, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_align(summaryUserLabel_, LV_ALIGN_TOP_RIGHT, -18, 48);

  summaryResultLabel_ = lv_label_create(summaryPanel_);
  lv_obj_set_size(summaryResultLabel_, 208, 148);
  lv_label_set_long_mode(summaryResultLabel_, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_bg_opa(summaryResultLabel_, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(summaryResultLabel_, lv_color_hex(0x071517), 0);
  lv_obj_set_style_radius(summaryResultLabel_, 12, 0);
  lv_obj_set_style_pad_hor(summaryResultLabel_, 14, 0);
  lv_obj_set_style_pad_ver(summaryResultLabel_, 34, 0);
  lv_obj_set_style_border_width(summaryResultLabel_, 1, 0);
  lv_obj_set_style_border_color(summaryResultLabel_, lv_color_hex(0x1D9DAA), 0);
  lv_obj_set_style_text_font(summaryResultLabel_, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(summaryResultLabel_, lv_color_hex(0xEAF6FB), 0);
  lv_obj_set_style_text_align(summaryResultLabel_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(summaryResultLabel_, LV_ALIGN_TOP_LEFT, 18, 100);

  auto makeSummaryTile = [&](lv_obj_t** out, lv_coord_t x, lv_coord_t y) {
    lv_obj_t* label = lv_label_create(summaryPanel_);
    lv_obj_set_size(label, 218, 78);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_bg_opa(label, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(label, lv_color_hex(0x0A161A), 0);
    lv_obj_set_style_radius(label, 10, 0);
    lv_obj_set_style_pad_hor(label, 12, 0);
    lv_obj_set_style_pad_ver(label, 8, 0);
    lv_obj_set_style_border_width(label, 1, 0);
    lv_obj_set_style_border_color(label, lv_color_hex(0x20343B), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xD8E4E8), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, x, y);
    *out = label;
  };

  makeSummaryTile(&summaryRepsLabel_, 246, 100);
  makeSummaryTile(&summaryRomLabel_, 480, 100);
  makeSummaryTile(&summaryLoadLabel_, 246, 188);
  makeSummaryTile(&summaryCoachLabel_, 480, 188);

  summaryDetailLabel_ = lv_label_create(summaryPanel_);
  lv_obj_set_width(summaryDetailLabel_, 684);
  lv_obj_set_height(summaryDetailLabel_, 70);
  lv_label_set_long_mode(summaryDetailLabel_, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(summaryDetailLabel_, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(summaryDetailLabel_, lv_color_hex(0xA8BABF), 0);
  lv_obj_set_style_text_align(summaryDetailLabel_, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_align(summaryDetailLabel_, LV_ALIGN_TOP_LEFT, 18, 286);

  summaryCountdownLabel_ = lv_label_create(summaryPanel_);
  lv_obj_set_style_bg_opa(summaryCountdownLabel_, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(summaryCountdownLabel_, lv_color_hex(0x0E1B20), 0);
  lv_obj_set_style_border_width(summaryCountdownLabel_, 1, 0);
  lv_obj_set_style_border_color(summaryCountdownLabel_, lv_color_hex(0x24454D), 0);
  lv_obj_set_style_radius(summaryCountdownLabel_, 999, 0);
  lv_obj_set_style_pad_hor(summaryCountdownLabel_, 12, 0);
  lv_obj_set_style_pad_ver(summaryCountdownLabel_, 5, 0);
  lv_obj_set_style_text_font(summaryCountdownLabel_, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(summaryCountdownLabel_, lv_color_hex(0x8FD9D2), 0);
  lv_obj_align(summaryCountdownLabel_, LV_ALIGN_BOTTOM_RIGHT, -18, -14);

  btnSummarySkip_ = createButton(summaryPanel_, "SKIP", kBtnGhost);
  lv_obj_set_size(btnSummarySkip_, 132, 42);
  lv_obj_align(btnSummarySkip_, LV_ALIGN_BOTTOM_LEFT, 18, -14);
  if (lv_obj_t* lbl = lv_obj_get_child(btnSummarySkip_, 0)) {
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
  }

  lv_obj_clear_flag(summaryScreen_, LV_OBJ_FLAG_HIDDEN);
}

void SmartGymTouchApp::buildRestOverlay(lv_obj_t* parent) {
  (void)parent;
  if (restOverlay_ != nullptr) {
    return;
  }

  restOverlay_ = lv_obj_create(screen_);
  lv_obj_remove_style_all(restOverlay_);
  lv_obj_set_style_bg_opa(restOverlay_, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(restOverlay_, 0, 0);
  lv_obj_set_size(restOverlay_, 800, 480);
  lv_obj_clear_flag(restOverlay_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(restOverlay_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(restOverlay_, LV_OBJ_FLAG_HIDDEN);

  restPanel_ = createPanel(restOverlay_, true);
  lv_obj_set_size(restPanel_, 356, 192);
  lv_obj_align(restPanel_, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(restPanel_, lv_color_hex(0x07151A), 0);
  lv_obj_set_style_bg_grad_color(restPanel_, lv_color_hex(0x0F232B), 0);
  lv_obj_set_style_bg_grad_dir(restPanel_, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_border_color(restPanel_, lv_color_hex(0x2A6B77), 0);
  lv_obj_set_style_shadow_width(restPanel_, 18, 0);
  lv_obj_set_style_shadow_color(restPanel_, lv_color_hex(0x041216), 0);
  lv_obj_set_style_shadow_opa(restPanel_, LV_OPA_40, 0);
  lv_obj_set_style_pad_all(restPanel_, 12, 0);

  restTitleLabel_ = lv_label_create(restPanel_);
  lv_label_set_text(restTitleLabel_, "RECOVERY");
  lv_obj_set_style_text_font(restTitleLabel_, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(restTitleLabel_, lv_color_hex(0x8FD9D2), 0);
  lv_obj_align(restTitleLabel_, LV_ALIGN_TOP_LEFT, 0, 0);

  restArc_ = lv_arc_create(restPanel_);
  lv_obj_set_size(restArc_, 118, 118);
  lv_obj_align(restArc_, LV_ALIGN_LEFT_MID, 8, 10);
  lv_arc_set_range(restArc_, 0, 100);
  lv_arc_set_bg_angles(restArc_, 0, 360);
  lv_arc_set_rotation(restArc_, 90);
  lv_arc_set_value(restArc_, 100);
  lv_obj_set_style_arc_width(restArc_, 10, LV_PART_MAIN);
  lv_obj_set_style_arc_width(restArc_, 10, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(restArc_, lv_color_hex(0x15363F), LV_PART_MAIN);
  lv_obj_set_style_arc_color(restArc_, lv_color_hex(0x2AD6BC), LV_PART_INDICATOR);
  lv_obj_set_style_pad_all(restArc_, 2, 0);
  lv_obj_set_style_border_width(restArc_, 0, 0);
  lv_obj_set_style_bg_opa(restArc_, LV_OPA_TRANSP, 0);

  restCountdownLabel_ = lv_label_create(restPanel_);
  lv_obj_set_width(restCountdownLabel_, 98);
  lv_label_set_long_mode(restCountdownLabel_, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_align(restCountdownLabel_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(restCountdownLabel_, &lv_font_montserrat_32, 0);
  lv_obj_set_style_text_color(restCountdownLabel_, lv_color_hex(0xF2FBFF), 0);
  lv_obj_align_to(restCountdownLabel_, restArc_, LV_ALIGN_CENTER, 0, 0);
  lv_label_set_text(restCountdownLabel_, "--");

  restDetailLabel_ = lv_label_create(restPanel_);
  lv_obj_set_width(restDetailLabel_, 198);
  lv_label_set_long_mode(restDetailLabel_, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(restDetailLabel_, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(restDetailLabel_, lv_color_hex(0xD4E7EE), 0);
  lv_obj_set_style_text_align(restDetailLabel_, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_align(restDetailLabel_, LV_ALIGN_RIGHT_MID, -2, -6);

  btnRestSkip_ = createButton(restPanel_, "SKIP", kBtnSecondary);
  lv_obj_set_size(btnRestSkip_, 112, 42);
  lv_obj_align(btnRestSkip_, LV_ALIGN_BOTTOM_RIGHT, -2, -2);
  if (lv_obj_t* lbl = lv_obj_get_child(btnRestSkip_, 0)) {
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
  }
}

void SmartGymTouchApp::buildCalibrationGateScreen(lv_obj_t* parent) {
  (void)parent;
  if (calibrationGateScreen_ != nullptr) {
    return;
  }

  calibrationGateScreen_ = lv_obj_create(nullptr);
  lv_obj_remove_style_all(calibrationGateScreen_);
  lv_obj_add_style(calibrationGateScreen_, &styleModal_, 0);
  lv_obj_set_size(calibrationGateScreen_, 800, 480);
  lv_obj_clear_flag(calibrationGateScreen_, LV_OBJ_FLAG_SCROLLABLE);

  calibrationGatePanel_ = createPanel(calibrationGateScreen_, true);
  lv_obj_set_size(calibrationGatePanel_, 600, 324);
  lv_obj_center(calibrationGatePanel_);

  calibrationGateTitleLabel_ = lv_label_create(calibrationGatePanel_);
  lv_label_set_text(calibrationGateTitleLabel_, "CALIBRATION CHECK");
  lv_obj_set_style_text_font(calibrationGateTitleLabel_, &lv_font_montserrat_22, 0);
  lv_obj_set_style_text_color(calibrationGateTitleLabel_, lv_color_hex(0x7DD2C4), 0);
  lv_obj_align(calibrationGateTitleLabel_, LV_ALIGN_TOP_LEFT, 0, 0);

  calibrationGateMachineLabel_ = lv_label_create(calibrationGatePanel_);
  lv_obj_set_width(calibrationGateMachineLabel_, 560);
  lv_label_set_long_mode(calibrationGateMachineLabel_, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(calibrationGateMachineLabel_, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(calibrationGateMachineLabel_, lv_color_hex(0xEAF3F6), 0);
  lv_obj_align(calibrationGateMachineLabel_, LV_ALIGN_TOP_LEFT, 0, 32);

  calibrationGateBodyLabel_ = lv_label_create(calibrationGatePanel_);
  lv_obj_set_width(calibrationGateBodyLabel_, 560);
  lv_label_set_long_mode(calibrationGateBodyLabel_, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(calibrationGateBodyLabel_, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(calibrationGateBodyLabel_, lv_color_hex(0xA8BABF), 0);
  lv_obj_align(calibrationGateBodyLabel_, LV_ALIGN_TOP_LEFT, 0, 84);

  calibrationGateHintLabel_ = lv_label_create(calibrationGatePanel_);
  lv_obj_set_width(calibrationGateHintLabel_, 560);
  lv_label_set_long_mode(calibrationGateHintLabel_, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(calibrationGateHintLabel_, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(calibrationGateHintLabel_, lv_color_hex(0x91A4AD), 0);
  lv_obj_align(calibrationGateHintLabel_, LV_ALIGN_TOP_LEFT, 0, 144);

  btnCalibrationGateSkip_ = createButton(calibrationGatePanel_, "SKIP", kBtnGhost);
  lv_obj_set_size(btnCalibrationGateSkip_, 130, 44);
  lv_obj_align(btnCalibrationGateSkip_, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
  if (lv_obj_t* lbl = lv_obj_get_child(btnCalibrationGateSkip_, 0)) {
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
  }

  btnCalibrationGateCalibrate_ = createButton(calibrationGatePanel_, "CALIBRATE NOW", kBtnPrimary);
  lv_obj_set_size(btnCalibrationGateCalibrate_, 184, 44);
  lv_obj_align(btnCalibrationGateCalibrate_, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  if (lv_obj_t* lbl = lv_obj_get_child(btnCalibrationGateCalibrate_, 0)) {
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
  }
}

void SmartGymTouchApp::buildCalibrationScreen(lv_obj_t* parent) {
  (void)parent;
  if (calibrationScreen_ != nullptr) {
    return;
  }

  calibrationScreen_ = lv_obj_create(nullptr);
  lv_obj_remove_style_all(calibrationScreen_);
  lv_obj_add_style(calibrationScreen_, &styleModal_, 0);
  lv_obj_set_size(calibrationScreen_, 800, 480);
  lv_obj_clear_flag(calibrationScreen_, LV_OBJ_FLAG_SCROLLABLE);

  calibrationHeaderLabel_ = lv_label_create(calibrationScreen_);
  lv_obj_set_size(calibrationHeaderLabel_, 360, 30);
  lv_label_set_long_mode(calibrationHeaderLabel_, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(calibrationHeaderLabel_, &lv_font_montserrat_22, 0);
  lv_obj_set_style_text_color(calibrationHeaderLabel_, lv_color_hex(0x7DD2C4), 0);
  lv_obj_set_pos(calibrationHeaderLabel_, 26, 20);
  lv_label_set_text(calibrationHeaderLabel_, "CALIBRATION");

  calibrationMachineLabel_ = lv_label_create(calibrationScreen_);
  lv_obj_set_size(calibrationMachineLabel_, 520, 24);
  lv_label_set_long_mode(calibrationMachineLabel_, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(calibrationMachineLabel_, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(calibrationMachineLabel_, lv_color_hex(0xA8BABF), 0);
  lv_obj_set_pos(calibrationMachineLabel_, 26, 52);

  calibrationVisualPanel_ = createPanel(calibrationScreen_, true);
  lv_obj_set_size(calibrationVisualPanel_, 356, 338);
  lv_obj_set_pos(calibrationVisualPanel_, 26, 92);
  lv_obj_set_style_pad_all(calibrationVisualPanel_, 0, 0);

  lv_obj_t* visualTitle = lv_label_create(calibrationVisualPanel_);
  lv_obj_set_size(visualTitle, 316, 24);
  lv_label_set_long_mode(visualTitle, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(visualTitle, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(visualTitle, lv_color_hex(0xEAF3F6), 0);
  lv_obj_set_pos(visualTitle, 18, 18);
  lv_label_set_text(visualTitle, "Live range");

  calibrationRomBar_ = lv_bar_create(calibrationVisualPanel_);
  lv_obj_set_size(calibrationRomBar_, 44, 230);
  lv_obj_set_pos(calibrationRomBar_, 44, 66);
  lv_bar_set_range(calibrationRomBar_, 0, 100);
  lv_bar_set_value(calibrationRomBar_, 0, LV_ANIM_OFF);

  calibrationMetricLabel_ = lv_label_create(calibrationVisualPanel_);
  lv_obj_set_size(calibrationMetricLabel_, 220, 220);
  lv_label_set_long_mode(calibrationMetricLabel_, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(calibrationMetricLabel_, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(calibrationMetricLabel_, lv_color_hex(0xD4E7EE), 0);
  lv_obj_set_pos(calibrationMetricLabel_, 114, 66);

  calibrationInstructionPanel_ = createPanel(calibrationScreen_, true);
  lv_obj_set_size(calibrationInstructionPanel_, 356, 368);
  lv_obj_set_pos(calibrationInstructionPanel_, 392, 76);
  lv_obj_set_style_pad_all(calibrationInstructionPanel_, 0, 0);

  calibrationStepLabel_ = lv_label_create(calibrationInstructionPanel_);
  lv_obj_set_size(calibrationStepLabel_, 206, 48);
  lv_label_set_long_mode(calibrationStepLabel_, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(calibrationStepLabel_, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(calibrationStepLabel_, lv_color_hex(0xF2FBFF), 0);
  lv_obj_set_pos(calibrationStepLabel_, 18, 14);

  calibrationInstructionLabel_ = lv_label_create(calibrationInstructionPanel_);
  lv_obj_set_size(calibrationInstructionLabel_, 320, 54);
  lv_label_set_long_mode(calibrationInstructionLabel_, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(calibrationInstructionLabel_, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(calibrationInstructionLabel_, lv_color_hex(0xA8BABF), 0);
  lv_obj_set_pos(calibrationInstructionLabel_, 18, 72);

  calibrationLoadLabel_ = lv_label_create(calibrationInstructionPanel_);
  lv_obj_set_size(calibrationLoadLabel_, 320, 48);
  lv_label_set_long_mode(calibrationLoadLabel_, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(calibrationLoadLabel_, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(calibrationLoadLabel_, lv_color_hex(0xF2FBFF), 0);
  lv_obj_set_pos(calibrationLoadLabel_, 18, 108);

  constexpr lv_coord_t kCalWeightButtonGap = 8;
  constexpr lv_coord_t kCalWeightButtonW = (316 - (3 * kCalWeightButtonGap)) / 4;
  constexpr lv_coord_t kCalWeightButtonH = 36;
  constexpr lv_coord_t kCalWeightY = 164;
  btnCalibrationWeightMinusLarge_ = createButton(calibrationInstructionPanel_, "-5", kBtnSecondary);
  lv_obj_set_size(btnCalibrationWeightMinusLarge_, kCalWeightButtonW, kCalWeightButtonH);
  lv_obj_set_pos(btnCalibrationWeightMinusLarge_, 18, kCalWeightY);
  lv_obj_set_style_pad_all(btnCalibrationWeightMinusLarge_, 2, 0);

  btnCalibrationWeightMinus_ = createButton(calibrationInstructionPanel_, "-2.5", kBtnSecondary);
  lv_obj_set_size(btnCalibrationWeightMinus_, kCalWeightButtonW, kCalWeightButtonH);
  lv_obj_set_pos(btnCalibrationWeightMinus_, 18 + kCalWeightButtonW + kCalWeightButtonGap, kCalWeightY);
  lv_obj_set_style_pad_all(btnCalibrationWeightMinus_, 2, 0);

  btnCalibrationWeightPlus_ = createButton(calibrationInstructionPanel_, "+2.5", kBtnSecondary);
  lv_obj_set_size(btnCalibrationWeightPlus_, kCalWeightButtonW, kCalWeightButtonH);
  lv_obj_set_pos(btnCalibrationWeightPlus_, 18 + ((kCalWeightButtonW + kCalWeightButtonGap) * 2), kCalWeightY);
  lv_obj_set_style_pad_all(btnCalibrationWeightPlus_, 2, 0);

  btnCalibrationWeightPlusLarge_ = createButton(calibrationInstructionPanel_, "+5", kBtnSecondary);
  lv_obj_set_size(btnCalibrationWeightPlusLarge_, kCalWeightButtonW, kCalWeightButtonH);
  lv_obj_set_pos(btnCalibrationWeightPlusLarge_, 18 + ((kCalWeightButtonW + kCalWeightButtonGap) * 3), kCalWeightY);
  lv_obj_set_style_pad_all(btnCalibrationWeightPlusLarge_, 2, 0);

  calibrationFeedbackLabel_ = lv_label_create(calibrationInstructionPanel_);
  lv_obj_set_size(calibrationFeedbackLabel_, 320, 48);
  lv_label_set_long_mode(calibrationFeedbackLabel_, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(calibrationFeedbackLabel_, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(calibrationFeedbackLabel_, lv_color_hex(0xD4E7EE), 0);
  lv_obj_set_pos(calibrationFeedbackLabel_, 18, 212);

  btnCalibrationPrimary_ = createButton(calibrationInstructionPanel_, "LOAD SET", kBtnPrimary);
  lv_obj_set_size(btnCalibrationPrimary_, 316, 46);
  lv_obj_set_pos(btnCalibrationPrimary_, 18, 304);

  btnCalibrationSecondary_ = createButton(calibrationInstructionPanel_, "SAVE NOW", kBtnSecondary);
  lv_obj_set_size(btnCalibrationSecondary_, 148, 46);
  lv_obj_set_pos(btnCalibrationSecondary_, 178, 304);

  btnCalibrationCancel_ = createButton(calibrationInstructionPanel_, "CANCEL", kBtnGhost);
  lv_obj_set_size(btnCalibrationCancel_, 96, 38);
  lv_obj_set_pos(btnCalibrationCancel_, 228, 18);

  const lv_coord_t panelW = 356;
  const lv_coord_t panelH = 368;
  const bool primaryClipped = (18 + 316 > panelW) || (304 + 46 > panelH);
  const bool cancelClipped = (228 + 96 > panelW) || (18 + 38 > panelH);
  const bool pinRowClipped = (18 + 316 > panelW) || (kCalWeightY + kCalWeightButtonH > panelH);
  logEvent("CAL_UI_LAYOUT", "screen w=800 h=480", LogLevel::Normal);
  logEvent("CAL_UI_LAYOUT", "left graph x=26 y=92 w=356 h=338", LogLevel::Normal);
  logEvent("CAL_UI_LAYOUT", "right available x=382 w=392", LogLevel::Normal);
  logEvent("CAL_UI_LAYOUT", "card x=392 y=76 w=356 h=368", LogLevel::Normal);
  logEvent("CAL_UI_LAYOUT", "margins leftGap=10 rightGap=52", LogLevel::Normal);
  logEvent("CAL_UI_LAYOUT", "visually_centered=1", LogLevel::Normal);
  logEvent("CAL_UI_LAYOUT",
           String("primary x=18 y=304 w=316 h=46 clipped=") + String(primaryClipped ? 1 : 0),
           LogLevel::Normal);
  logEvent("CAL_UI_LAYOUT",
           String("cancel x=228 y=18 w=96 h=38 clipped=") + String(cancelClipped ? 1 : 0),
           LogLevel::Normal);
  logEvent("CAL_UI_LAYOUT",
           String("pin row x=18 y=") + String(kCalWeightY) +
               " w=316 h=" + String(kCalWeightButtonH) +
               " clipped=" + String(pinRowClipped ? 1 : 0),
           LogLevel::Normal);
}

void SmartGymTouchApp::buildProfileScreen(lv_obj_t* parent) {
  (void)parent;
  if (profileScreen_ != nullptr) {
    return;
  }

  profileScreen_ = lv_obj_create(nullptr);
  lv_obj_remove_style_all(profileScreen_);
  lv_obj_add_style(profileScreen_, &styleModal_, 0);
  lv_obj_set_size(profileScreen_, 800, 480);
  lv_obj_clear_flag(profileScreen_, LV_OBJ_FLAG_SCROLLABLE);

  profilePanel_ = createPanel(profileScreen_, true);
  lv_obj_set_size(profilePanel_, 724, 414);
  lv_obj_center(profilePanel_);

  profileTitleLabel_ = lv_label_create(profilePanel_);
  lv_obj_set_style_text_font(profileTitleLabel_, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(profileTitleLabel_, lv_color_hex(0xEAF6FB), 0);
  lv_obj_align(profileTitleLabel_, LV_ALIGN_TOP_LEFT, 0, 0);

  profileUidLabel_ = lv_label_create(profilePanel_);
  lv_obj_set_style_text_font(profileUidLabel_, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(profileUidLabel_, lv_color_hex(0x8FB0BE), 0);
  lv_obj_align(profileUidLabel_, LV_ALIGN_TOP_LEFT, 0, 28);

  auto makeRow = [&](const char* label, int y) -> lv_obj_t* {
    lv_obj_t* row = createPanel(profilePanel_, true);
    lv_obj_set_size(row, 684, 52);
    lv_obj_align(row, LV_ALIGN_TOP_LEFT, 0, y);
    lv_obj_set_style_pad_all(row, 8, 0);
    lv_obj_t* lbl = lv_label_create(row);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xC9DFE7), 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 8, 0);
    return row;
  };

  lv_obj_t* nameRow = makeRow("Name", 58);
  profileNameValueLabel_ = lv_label_create(nameRow);
  lv_obj_set_size(profileNameValueLabel_, 264, 30);
  lv_obj_set_style_text_align(profileNameValueLabel_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(profileNameValueLabel_, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(profileNameValueLabel_, lv_color_hex(0xF4FBFF), 0);
  lv_obj_set_style_text_align(profileNameValueLabel_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(profileNameValueLabel_, LV_ALIGN_RIGHT_MID, -114, 0);
  btnProfileEditName_ = createButton(nameRow, "EDIT", kBtnSecondary);
  lv_obj_set_size(btnProfileEditName_, 84, 40);
  lv_obj_align(btnProfileEditName_, LV_ALIGN_RIGHT_MID, -2, 0);

  lv_obj_t* ageRow = makeRow("Age", 106);
  btnProfileAgeMinus_ = createButton(ageRow, "-", kBtnSecondary);
  lv_obj_set_size(btnProfileAgeMinus_, 52, 40);
  lv_obj_align(btnProfileAgeMinus_, LV_ALIGN_RIGHT_MID, -204, 0);
  profileAgeValueLabel_ = lv_label_create(ageRow);
  lv_obj_set_size(profileAgeValueLabel_, 120, 30);
  lv_label_set_long_mode(profileAgeValueLabel_, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_align(profileAgeValueLabel_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(profileAgeValueLabel_, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(profileAgeValueLabel_, lv_color_hex(0xF4FBFF), 0);
  lv_obj_set_style_text_align(profileAgeValueLabel_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(profileAgeValueLabel_, LV_ALIGN_RIGHT_MID, -80, 0);
  btnProfileAgePlus_ = createButton(ageRow, "+", kBtnSecondary);
  lv_obj_set_size(btnProfileAgePlus_, 52, 40);
  lv_obj_align(btnProfileAgePlus_, LV_ALIGN_RIGHT_MID, 0, 0);

  lv_obj_t* weightRow = makeRow("Weight (kg)", 154);
  btnProfileWeightMinus_ = createButton(weightRow, "-", kBtnSecondary);
  lv_obj_set_size(btnProfileWeightMinus_, 52, 40);
  lv_obj_align(btnProfileWeightMinus_, LV_ALIGN_RIGHT_MID, -204, 0);
  profileWeightValueLabel_ = lv_label_create(weightRow);
  lv_obj_set_size(profileWeightValueLabel_, 120, 30);
  lv_label_set_long_mode(profileWeightValueLabel_, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_align(profileWeightValueLabel_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(profileWeightValueLabel_, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(profileWeightValueLabel_, lv_color_hex(0xF4FBFF), 0);
  lv_obj_set_style_text_align(profileWeightValueLabel_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(profileWeightValueLabel_, LV_ALIGN_RIGHT_MID, -80, 0);
  btnProfileWeightPlus_ = createButton(weightRow, "+", kBtnSecondary);
  lv_obj_set_size(btnProfileWeightPlus_, 52, 40);
  lv_obj_align(btnProfileWeightPlus_, LV_ALIGN_RIGHT_MID, 0, 0);

  lv_obj_t* heightRow = makeRow("Height (cm)", 202);
  btnProfileHeightMinus_ = createButton(heightRow, "-", kBtnSecondary);
  lv_obj_set_size(btnProfileHeightMinus_, 52, 40);
  lv_obj_align(btnProfileHeightMinus_, LV_ALIGN_RIGHT_MID, -204, 0);
  profileHeightValueLabel_ = lv_label_create(heightRow);
  lv_obj_set_size(profileHeightValueLabel_, 120, 30);
  lv_label_set_long_mode(profileHeightValueLabel_, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_align(profileHeightValueLabel_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(profileHeightValueLabel_, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(profileHeightValueLabel_, lv_color_hex(0xF4FBFF), 0);
  lv_obj_set_style_text_align(profileHeightValueLabel_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(profileHeightValueLabel_, LV_ALIGN_RIGHT_MID, -80, 0);
  btnProfileHeightPlus_ = createButton(heightRow, "+", kBtnSecondary);
  lv_obj_set_size(btnProfileHeightPlus_, 52, 40);
  lv_obj_align(btnProfileHeightPlus_, LV_ALIGN_RIGHT_MID, 0, 0);

  lv_obj_t* goalRow = makeRow("Goal", 250);
  btnProfileGoalPrev_ = createButton(goalRow, "<", kBtnSecondary);
  lv_obj_set_size(btnProfileGoalPrev_, 52, 40);
  lv_obj_align(btnProfileGoalPrev_, LV_ALIGN_RIGHT_MID, -204, 0);
  profileGoalValueLabel_ = lv_label_create(goalRow);
  lv_obj_set_size(profileGoalValueLabel_, 140, 30);
  lv_label_set_long_mode(profileGoalValueLabel_, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_align(profileGoalValueLabel_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(profileGoalValueLabel_, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(profileGoalValueLabel_, lv_color_hex(0xF4FBFF), 0);
  lv_obj_set_style_text_align(profileGoalValueLabel_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(profileGoalValueLabel_, LV_ALIGN_RIGHT_MID, -66, 0);
  btnProfileGoalNext_ = createButton(goalRow, ">", kBtnSecondary);
  lv_obj_set_size(btnProfileGoalNext_, 52, 40);
  lv_obj_align(btnProfileGoalNext_, LV_ALIGN_RIGHT_MID, 0, 0);

  lv_obj_t* genderRow = makeRow("Gender", 298);
  btnProfileGenderPrev_ = createButton(genderRow, "<", kBtnSecondary);
  lv_obj_set_size(btnProfileGenderPrev_, 52, 40);
  lv_obj_align(btnProfileGenderPrev_, LV_ALIGN_RIGHT_MID, -204, 0);
  profileGenderValueLabel_ = lv_label_create(genderRow);
  lv_obj_set_size(profileGenderValueLabel_, 140, 30);
  lv_label_set_long_mode(profileGenderValueLabel_, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_align(profileGenderValueLabel_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(profileGenderValueLabel_, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(profileGenderValueLabel_, lv_color_hex(0xF4FBFF), 0);
  lv_obj_align(profileGenderValueLabel_, LV_ALIGN_RIGHT_MID, -66, 0);
  btnProfileGenderNext_ = createButton(genderRow, ">", kBtnSecondary);
  lv_obj_set_size(btnProfileGenderNext_, 52, 40);
  lv_obj_align(btnProfileGenderNext_, LV_ALIGN_RIGHT_MID, 0, 0);

  lv_obj_t* profileActionRow = lv_obj_create(profilePanel_);
  lv_obj_remove_style_all(profileActionRow);
  lv_obj_set_size(profileActionRow, 684, 46);
  lv_obj_align(profileActionRow, LV_ALIGN_TOP_LEFT, 0, 352);
  lv_obj_set_layout(profileActionRow, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(profileActionRow, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(profileActionRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(profileActionRow, LV_OBJ_FLAG_SCROLLABLE);

  btnProfileCancel_ = createButton(profileActionRow, "CANCEL", kBtnGhost);
  lv_obj_set_size(btnProfileCancel_, 172, 44);

  btnProfileSave_ = createButton(profileActionRow, "SAVE", kBtnGhost);
  lv_obj_set_size(btnProfileSave_, 172, 44);

  profileNameTa_ = lv_textarea_create(profilePanel_);
  lv_obj_set_size(profileNameTa_, 684, 34);
  lv_obj_align(profileNameTa_, LV_ALIGN_BOTTOM_MID, 0, -128);
  lv_textarea_set_one_line(profileNameTa_, true);
  lv_textarea_set_max_length(profileNameTa_, 24);
  lv_obj_add_flag(profileNameTa_, LV_OBJ_FLAG_HIDDEN);

  profileKeyboard_ = lv_keyboard_create(profilePanel_);
  lv_obj_set_size(profileKeyboard_, 684, 112);
  lv_obj_align(profileKeyboard_, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_keyboard_set_textarea(profileKeyboard_, profileNameTa_);
  lv_obj_add_event_cb(profileKeyboard_, onButtonEvent, LV_EVENT_ALL, this);
  lv_obj_add_flag(profileKeyboard_, LV_OBJ_FLAG_HIDDEN);
}

void SmartGymTouchApp::buildHeader(lv_obj_t* parent) {
  lv_obj_t* header = lv_obj_create(parent);
  lv_obj_remove_style_all(header);
  lv_obj_add_style(header, &styleHeader_, 0);
  lv_obj_set_size(header, 800, 72);
  lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

  topStateLabel_ = lv_label_create(header);
  lv_obj_set_width(topStateLabel_, 190);
  lv_label_set_long_mode(topStateLabel_, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(topStateLabel_, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(topStateLabel_, lv_color_hex(0x1ED3A6), 0);
  lv_obj_align(topStateLabel_, LV_ALIGN_LEFT_MID, 10, -12);

  topMachineLabel_ = lv_label_create(header);
  lv_obj_set_width(topMachineLabel_, 250);
  lv_label_set_long_mode(topMachineLabel_, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(topMachineLabel_, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(topMachineLabel_, lv_color_hex(0xEAF6FB), 0);
  lv_obj_align(topMachineLabel_, LV_ALIGN_LEFT_MID, 10, 12);

  topUserLabel_ = lv_label_create(header);
  lv_obj_set_width(topUserLabel_, 200);
  lv_label_set_long_mode(topUserLabel_, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_align(topUserLabel_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(topUserLabel_, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(topUserLabel_, lv_color_hex(0xA8C0CC), 0);
  lv_obj_align(topUserLabel_, LV_ALIGN_CENTER, -82, 0);

  topDateLabel_ = lv_label_create(header);
  lv_obj_set_width(topDateLabel_, 132);
  lv_label_set_long_mode(topDateLabel_, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_align(topDateLabel_, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_style_text_font(topDateLabel_, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(topDateLabel_, lv_color_hex(0xB6CCD7), 0);
  lv_obj_align(topDateLabel_, LV_ALIGN_RIGHT_MID, -218, -10);

  topTimeLabel_ = lv_label_create(header);
  lv_obj_set_width(topTimeLabel_, 132);
  lv_label_set_long_mode(topTimeLabel_, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_align(topTimeLabel_, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_style_text_font(topTimeLabel_, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(topTimeLabel_, lv_color_hex(0xEAF6FB), 0);
  lv_obj_align(topTimeLabel_, LV_ALIGN_RIGHT_MID, -218, 10);

  topSyncLabel_ = lv_label_create(header);
  lv_obj_set_width(topSyncLabel_, 132);
  lv_label_set_long_mode(topSyncLabel_, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_align(topSyncLabel_, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_style_text_font(topSyncLabel_, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(topSyncLabel_, lv_color_hex(0x8FD9D2), 0);
  lv_obj_align(topSyncLabel_, LV_ALIGN_RIGHT_MID, -218, 26);
  lv_label_set_text(topSyncLabel_, "");

  btnAvatar_ = createButton(header, LV_SYMBOL_EDIT, kBtnSecondary);
  lv_obj_set_size(btnAvatar_, 72, 44);
  lv_obj_align(btnAvatar_, LV_ALIGN_RIGHT_MID, -86, 0);
  lv_obj_set_style_pad_hor(btnAvatar_, 10, 0);
  lv_obj_set_style_pad_ver(btnAvatar_, 6, 0);

  btnService_ = createButton(header, LV_SYMBOL_SETTINGS, kBtnGhost);
  lv_obj_set_size(btnService_, 72, 44);
  lv_obj_align(btnService_, LV_ALIGN_RIGHT_MID, -10, 0);
  lv_obj_set_style_pad_hor(btnService_, 10, 0);
  lv_obj_set_style_pad_ver(btnService_, 6, 0);
}

void SmartGymTouchApp::buildMainPanels(lv_obj_t* parent) {
  logBootMemPoint("before_buildMainPanels_body");
  constexpr lv_coord_t kScreenW = 800;
  constexpr lv_coord_t kScreenH = 480;
  constexpr lv_coord_t kLeftCardX = 10;
  constexpr lv_coord_t kLeftCardY = 76;
  constexpr lv_coord_t kLeftCardW = 462;
  constexpr lv_coord_t kLeftCardH = 382;
  constexpr lv_coord_t kRightCardX = 484;
  constexpr lv_coord_t kRightCardY = 76;
  constexpr lv_coord_t kRightCardW = 292;
  constexpr lv_coord_t kRightCardH = 382;
  constexpr lv_coord_t kRightPanelPadX = 22;
  constexpr lv_coord_t kRightPanelPadTop = 20;
  constexpr lv_coord_t kRightPanelPadBottom = 20;
  constexpr lv_coord_t kRightPanelInnerWidth = kRightCardW - (2 * kRightPanelPadX);
  constexpr lv_coord_t kRightActionButtonGap = 12;
  constexpr lv_coord_t kRightActionButtonWidth = (kRightPanelInnerWidth - kRightActionButtonGap) / 2;
  constexpr lv_coord_t kRightActionButtonHeight = 48;
  constexpr lv_coord_t kRightActionY = kRightCardH - kRightPanelPadBottom - kRightActionButtonHeight;
  lv_obj_t* left = createPanel(parent, false);
  lv_obj_set_size(left, kLeftCardW, kLeftCardH);
  lv_obj_set_pos(left, kLeftCardX, kLeftCardY);

  lv_obj_t* topRow = lv_obj_create(left);
  lv_obj_remove_style_all(topRow);
  lv_obj_set_size(topRow, 434, 32);
  lv_obj_align(topRow, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_layout(topRow, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(topRow, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(topRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(topRow, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* leftTitle = lv_label_create(topRow);
  lv_label_set_text(leftTitle, "MOVEMENT");
  lv_obj_set_style_text_font(leftTitle, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(leftTitle, lv_color_hex(0x7C919C), 0);
  lv_obj_set_width(leftTitle, 108);
  lv_label_set_long_mode(leftTitle, LV_LABEL_LONG_CLIP);

  romMainLabel_ = lv_label_create(topRow);
  lv_obj_set_style_text_font(romMainLabel_, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(romMainLabel_, lv_color_hex(0xDCE9EE), 0);
  lv_obj_set_width(romMainLabel_, 92);
  lv_label_set_long_mode(romMainLabel_, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_align(romMainLabel_, LV_TEXT_ALIGN_CENTER, 0);

  velocityMainLabel_ = lv_label_create(topRow);
  lv_obj_set_style_text_font(velocityMainLabel_, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(velocityMainLabel_, lv_color_hex(0xDCE9EE), 0);
  lv_obj_set_width(velocityMainLabel_, 72);
  lv_label_set_long_mode(velocityMainLabel_, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_align(velocityMainLabel_, LV_TEXT_ALIGN_CENTER, 0);

  qualityLabel_ = lv_label_create(topRow);
  lv_obj_set_style_text_font(qualityLabel_, &lv_font_montserrat_16, 0);
  lv_obj_set_style_bg_opa(qualityLabel_, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(qualityLabel_, lv_color_hex(0x0F191E), 0);
  lv_obj_set_style_radius(qualityLabel_, 999, 0);
  lv_obj_set_style_pad_hor(qualityLabel_, 10, 0);
  lv_obj_set_style_pad_ver(qualityLabel_, 5, 0);
  lv_obj_set_style_border_width(qualityLabel_, 1, 0);
  lv_obj_set_style_border_color(qualityLabel_, lv_color_hex(0x26343D), 0);
  lv_obj_set_style_text_color(qualityLabel_, lv_color_hex(0x86D9C9), 0);
  lv_obj_set_size(qualityLabel_, 112, 30);
  lv_obj_set_style_text_align(qualityLabel_, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(qualityLabel_, "READY");

  lv_obj_t* setBarLabel = lv_label_create(left);
  lv_label_set_text(setBarLabel, "SET");
  lv_obj_set_style_text_font(setBarLabel, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(setBarLabel, lv_color_hex(0x8FA7B3), 0);
  lv_obj_align(setBarLabel, LV_ALIGN_TOP_LEFT, 2, 272);

  setProgressBar_ = lv_bar_create(left);
  lv_obj_set_size(setProgressBar_, 434, 12);
  lv_obj_align(setProgressBar_, LV_ALIGN_TOP_LEFT, 0, 292);
  lv_bar_set_range(setProgressBar_, 0, 100);
  lv_obj_set_style_bg_color(setProgressBar_, lv_color_hex(0x091014), LV_PART_MAIN);
  lv_obj_set_style_bg_color(setProgressBar_, lv_color_hex(0x86D9C9), LV_PART_INDICATOR);
  lv_obj_set_style_radius(setProgressBar_, 999, 0);

  lv_obj_t* romBarLabel = lv_label_create(left);
  lv_label_set_text(romBarLabel, "ROM");
  lv_obj_set_style_text_font(romBarLabel, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(romBarLabel, lv_color_hex(0x8FA7B3), 0);
  lv_obj_align(romBarLabel, LV_ALIGN_TOP_LEFT, 2, 316);

  romBar_ = lv_bar_create(left);
  lv_obj_set_size(romBar_, 434, 12);
  lv_obj_align(romBar_, LV_ALIGN_TOP_LEFT, 0, 338);
  lv_bar_set_range(romBar_, 0, 100);
  lv_obj_set_style_bg_color(romBar_, lv_color_hex(0x091014), LV_PART_MAIN);
  lv_obj_set_style_bg_color(romBar_, lv_color_hex(0x1DAA93), LV_PART_INDICATOR);
  lv_obj_set_style_radius(romBar_, 999, 0);

  logBootMemPoint("before_graphTargetUi");
  metricsChart_ = lv_chart_create(left);
  lv_obj_set_size(metricsChart_, 434, 180);
  lv_obj_align(metricsChart_, LV_ALIGN_TOP_MID, 0, 34);
  lv_chart_set_type(metricsChart_, LV_CHART_TYPE_LINE);
  // We fill fixed point IDs manually. Circular mode reorders the draw start
  // internally and makes the guide look torn/jagged on RGB panels.
  lv_chart_set_update_mode(metricsChart_, LV_CHART_UPDATE_MODE_SHIFT);
  lv_chart_set_point_count(metricsChart_, kMotionGraphPoints);
  lv_chart_set_range(metricsChart_, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
  lv_chart_set_div_line_count(metricsChart_, 4, 0);
  lv_obj_set_style_bg_opa(metricsChart_, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(metricsChart_, lv_color_hex(0x081216), 0);
  lv_obj_set_style_border_width(metricsChart_, 0, 0);
  // Deterministic chart: all series are rendered from one frame pipeline.
  lv_obj_set_style_line_width(metricsChart_, 2, LV_PART_ITEMS);
  lv_obj_set_style_line_rounded(metricsChart_, true, LV_PART_ITEMS);
  lv_obj_set_style_size(metricsChart_, 0, LV_PART_INDICATOR);
  lv_obj_set_style_pad_left(metricsChart_, kMotionChartPadSidePx, LV_PART_MAIN);
  lv_obj_set_style_pad_right(metricsChart_, kMotionChartPadSidePx, LV_PART_MAIN);
  lv_obj_set_style_pad_top(metricsChart_, kMotionChartPadTopPx, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(metricsChart_, kMotionChartPadBottomPx, LV_PART_MAIN);
  idealSeriesLower_ = lv_chart_add_series(metricsChart_, lv_color_hex(0xD4BC5A), LV_CHART_AXIS_PRIMARY_Y);
  idealSeriesUpper_ = lv_chart_add_series(metricsChart_, lv_color_hex(0xF0D76A), LV_CHART_AXIS_PRIMARY_Y);
  idealSeries_ = lv_chart_add_series(metricsChart_, lv_color_hex(0xE8D983), LV_CHART_AXIS_PRIMARY_Y);
  metricsSeries_ = lv_chart_add_series(metricsChart_, lv_color_hex(0x86E7F0), LV_CHART_AXIS_PRIMARY_Y);
  logBootMemPoint("after_graphTargetUi");

  lv_obj_t* actualLegend = lv_label_create(left);
  lv_label_set_text(actualLegend, "LIVE DOT");
  lv_obj_set_style_text_font(actualLegend, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(actualLegend, lv_color_hex(0x1DAA93), 0);
  lv_obj_align(actualLegend, LV_ALIGN_TOP_LEFT, 8, 46);
  chartActualLegend_ = actualLegend;

  lv_obj_t* idealLegend = lv_label_create(left);
  lv_label_set_text(idealLegend, "IDEAL");
  lv_obj_set_style_text_font(idealLegend, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(idealLegend, lv_color_hex(0xD9A441), 0);
  lv_obj_align(idealLegend, LV_ALIGN_TOP_LEFT, 80, 46);
  chartIdealLegend_ = idealLegend;

  romTargetBand_ = lv_obj_create(left);
  lv_obj_remove_style_all(romTargetBand_);
  lv_obj_set_style_radius(romTargetBand_, 10, 0);
  lv_obj_set_style_bg_opa(romTargetBand_, LV_OPA_40, 0);
  lv_obj_set_style_bg_color(romTargetBand_, lv_color_hex(0xD9A441), 0);
  lv_obj_set_style_border_width(romTargetBand_, 1, 0);
  lv_obj_set_style_border_color(romTargetBand_, lv_color_hex(0xE7C36A), 0);
  lv_obj_clear_flag(romTargetBand_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(romTargetBand_, LV_OBJ_FLAG_HIDDEN);

  romTargetBandLabel_ = lv_label_create(romTargetBand_);
  lv_obj_set_style_text_font(romTargetBandLabel_, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(romTargetBandLabel_, lv_color_hex(0x122024), 0);
  lv_obj_align(romTargetBandLabel_, LV_ALIGN_CENTER, 0, 0);
  lv_label_set_text(romTargetBandLabel_, "ROM TARGET 80-95%");

  lv_obj_add_flag(metricsChart_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(chartActualLegend_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(chartIdealLegend_, LV_OBJ_FLAG_HIDDEN);

  motionLiveHalo_ = lv_obj_create(left);
  lv_obj_remove_style_all(motionLiveHalo_);
  lv_obj_set_size(motionLiveHalo_, 26, 26);
  lv_obj_set_style_radius(motionLiveHalo_, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(motionLiveHalo_, lv_color_hex(0x1DAA93), 0);
  lv_obj_set_style_bg_opa(motionLiveHalo_, LV_OPA_30, 0);
  lv_obj_set_style_border_width(motionLiveHalo_, 0, 0);
  lv_obj_clear_flag(motionLiveHalo_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(motionLiveHalo_, LV_OBJ_FLAG_HIDDEN);

  motionLiveDot_ = lv_obj_create(left);
  lv_obj_remove_style_all(motionLiveDot_);
  lv_obj_set_size(motionLiveDot_, 12, 12);
  lv_obj_set_style_radius(motionLiveDot_, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(motionLiveDot_, lv_color_hex(0xF3FBFF), 0);
  lv_obj_set_style_bg_opa(motionLiveDot_, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(motionLiveDot_, 0, 0);
  lv_obj_clear_flag(motionLiveDot_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(motionLiveDot_, LV_OBJ_FLAG_HIDDEN);

  statusMainLabel_ = lv_label_create(left);
  lv_obj_set_width(statusMainLabel_, 434);
  lv_obj_set_height(statusMainLabel_, 40);
  lv_label_set_long_mode(statusMainLabel_, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(statusMainLabel_, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(statusMainLabel_, lv_color_hex(0xB2C2C9), 0);
  lv_obj_set_style_pad_hor(statusMainLabel_, 8, 0);
  lv_obj_set_style_pad_ver(statusMainLabel_, 4, 0);
  lv_obj_align(statusMainLabel_, LV_ALIGN_TOP_LEFT, 0, 224);
  lv_obj_add_flag(statusMainLabel_, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t* right = lv_obj_create(parent);
  lv_obj_remove_style_all(right);
  lv_obj_add_style(right, &styleAccentCard_, 0);
  // This card uses explicit fixed-zone coordinates. The shared accent style
  // has padding, so clear it here or the whole grid drifts right/down.
  lv_obj_set_style_pad_all(right, 0, 0);
  lv_obj_set_size(right, kRightCardW, kRightCardH);
  lv_obj_set_pos(right, kRightCardX, kRightCardY);
  lv_obj_clear_flag(right, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* rightTitle = lv_label_create(right);
  lv_label_set_text(rightTitle, "SESSION");
  lv_obj_set_size(rightTitle, kRightPanelInnerWidth, 24);
  lv_label_set_long_mode(rightTitle, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(rightTitle, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(rightTitle, lv_color_hex(0x7DD2C4), 0);
  lv_obj_set_style_text_align(rightTitle, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_set_pos(rightTitle, kRightPanelPadX, kRightPanelPadTop);

  pinLoadTitleLabel_ = lv_label_create(right);
  lv_label_set_text(pinLoadTitleLabel_, "Pin load");
  lv_obj_set_size(pinLoadTitleLabel_, kRightPanelInnerWidth, 22);
  lv_label_set_long_mode(pinLoadTitleLabel_, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(pinLoadTitleLabel_, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(pinLoadTitleLabel_, lv_color_hex(0x93AEB8), 0);
  lv_obj_set_style_text_align(pinLoadTitleLabel_, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_set_pos(pinLoadTitleLabel_, kRightPanelPadX, 58);

  bigWeightLabel_ = lv_label_create(right);
  lv_obj_set_style_text_font(bigWeightLabel_, &lv_font_montserrat_34, 0);
  lv_obj_set_style_text_color(bigWeightLabel_, lv_color_hex(0xF4F8F8), 0);
  lv_obj_set_size(bigWeightLabel_, kRightPanelInnerWidth, 46);
  lv_label_set_long_mode(bigWeightLabel_, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_align(bigWeightLabel_, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_set_style_bg_opa(bigWeightLabel_, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(bigWeightLabel_, 0, 0);
  lv_obj_set_style_pad_all(bigWeightLabel_, 0, 0);
  lv_obj_set_pos(bigWeightLabel_, kRightPanelPadX, 82);

  speedCueLabel_ = lv_label_create(left);
  lv_obj_set_style_text_font(speedCueLabel_, &lv_font_montserrat_16, 0);
  lv_obj_set_style_bg_opa(speedCueLabel_, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(speedCueLabel_, lv_color_hex(0x101B20), 0);
  lv_obj_set_style_radius(speedCueLabel_, 8, 0);
  lv_obj_set_style_pad_hor(speedCueLabel_, 8, 0);
  lv_obj_set_style_pad_ver(speedCueLabel_, 4, 0);
  lv_obj_set_style_border_width(speedCueLabel_, 1, 0);
  lv_obj_set_style_border_color(speedCueLabel_, lv_color_hex(0x2E4C57), 0);
  lv_obj_set_style_text_color(speedCueLabel_, lv_color_hex(0xD9A441), 0);
  lv_obj_set_size(speedCueLabel_, kRightPanelInnerWidth, 24);
  lv_obj_set_style_text_align(speedCueLabel_, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(speedCueLabel_, "FOLLOW THE CURVE");
  lv_label_set_long_mode(speedCueLabel_, LV_LABEL_LONG_CLIP);
  lv_obj_align(speedCueLabel_, LV_ALIGN_TOP_LEFT, 8, 46);
  lv_obj_add_flag(speedCueLabel_, LV_OBJ_FLAG_HIDDEN);

  historyMiniLabel_ = lv_label_create(left);
  lv_obj_set_size(historyMiniLabel_, 434, 22);
  lv_label_set_long_mode(historyMiniLabel_, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(historyMiniLabel_, &lv_font_montserrat_16, 0);
  lv_obj_set_style_bg_opa(historyMiniLabel_, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(historyMiniLabel_, 0, 0);
  lv_obj_set_style_pad_all(historyMiniLabel_, 0, 0);
  lv_obj_set_style_text_color(historyMiniLabel_, lv_color_hex(0xA8BABF), 0);
  lv_obj_set_style_text_align(historyMiniLabel_, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_align(historyMiniLabel_, LV_ALIGN_TOP_LEFT, 0, 224);
  lv_obj_add_flag(historyMiniLabel_, LV_OBJ_FLAG_HIDDEN);

  recommendationTitleLabel_ = lv_label_create(right);
  lv_label_set_text(recommendationTitleLabel_, "Recommended");
  lv_obj_set_size(recommendationTitleLabel_, kRightPanelInnerWidth, 22);
  lv_label_set_long_mode(recommendationTitleLabel_, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(recommendationTitleLabel_, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(recommendationTitleLabel_, lv_color_hex(0x93AEB8), 0);
  lv_obj_set_style_text_align(recommendationTitleLabel_, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_set_pos(recommendationTitleLabel_, kRightPanelPadX, 268);

  suggestionLabel_ = lv_label_create(right);
  lv_obj_set_size(suggestionLabel_, kRightPanelInnerWidth, 24);
  lv_label_set_long_mode(suggestionLabel_, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_font(suggestionLabel_, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(suggestionLabel_, lv_color_hex(0xC8D7DC), 0);
  lv_obj_set_style_text_align(suggestionLabel_, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_set_pos(suggestionLabel_, kRightPanelPadX, 292);

  sessionTimerLabel_ = lv_label_create(right);
  lv_obj_set_width(sessionTimerLabel_, 92);
  lv_label_set_long_mode(sessionTimerLabel_, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_font(sessionTimerLabel_, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(sessionTimerLabel_, lv_color_hex(0xAFC3CC), 0);
  lv_obj_set_style_text_align(sessionTimerLabel_, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_align(sessionTimerLabel_, LV_ALIGN_TOP_RIGHT, -kRightPanelPadX, kRightPanelPadTop);

  constexpr lv_coord_t kRepSetGap = 16;
  constexpr lv_coord_t kRepSetCardW = (kRightPanelInnerWidth - kRepSetGap) / 2;
  constexpr lv_coord_t kRepSetGroupW = (kRepSetCardW * 2) + kRepSetGap;
  constexpr lv_coord_t kRepSetGroupX = (kRightCardW - kRepSetGroupW) / 2;
  lv_obj_t* repsTile = createValueTile(right, "REPS", &repMainLabel_);
  lv_obj_set_size(repsTile, kRepSetCardW, 62);
  lv_obj_set_pos(repsTile, kRepSetGroupX, 144);

  lv_obj_t* setTile = createValueTile(right, "SET", &setMainLabel_);
  lv_obj_set_size(setTile, kRepSetCardW, 62);
  lv_obj_set_pos(setTile, kRepSetGroupX + kRepSetCardW + kRepSetGap, 144);

  lv_obj_t* paceTile = createValueTile(left, "PACE", &paceMainLabel_);
  lv_obj_set_size(paceTile, 1, 1);
  lv_obj_align(paceTile, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_add_flag(paceTile, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_text_font(paceMainLabel_, &lv_font_montserrat_16, 0);

  sessionMetaLabel_ = lv_label_create(left);
  lv_obj_set_size(sessionMetaLabel_, 434, 18);
  lv_label_set_long_mode(sessionMetaLabel_, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(sessionMetaLabel_, &lv_font_montserrat_14, 0);
  lv_obj_set_style_bg_opa(sessionMetaLabel_, LV_OPA_TRANSP, 0);
  lv_obj_set_style_bg_color(sessionMetaLabel_, lv_color_hex(0x0A161A), 0);
  lv_obj_set_style_radius(sessionMetaLabel_, 10, 0);
  lv_obj_set_style_pad_hor(sessionMetaLabel_, 0, 0);
  lv_obj_set_style_pad_ver(sessionMetaLabel_, 0, 0);
  lv_obj_set_style_border_width(sessionMetaLabel_, 0, 0);
  lv_obj_set_style_border_color(sessionMetaLabel_, lv_color_hex(0x20343B), 0);
  lv_obj_set_style_text_color(sessionMetaLabel_, lv_color_hex(0x96B4BC), 0);
  lv_obj_set_style_text_align(sessionMetaLabel_, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_align(sessionMetaLabel_, LV_ALIGN_TOP_LEFT, 0, 254);
  lv_label_set_text(sessionMetaLabel_, "");
  lv_obj_add_flag(sessionMetaLabel_, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t* weightControls = lv_obj_create(right);
  lv_obj_remove_style_all(weightControls);
  constexpr lv_coord_t kWeightButtonGap = 8;
  constexpr lv_coord_t kWeightButtonW = (kRightPanelInnerWidth - (3 * kWeightButtonGap)) / 4;
  constexpr lv_coord_t kWeightButtonH = 36;
  constexpr lv_coord_t kWeightGroupW = (kWeightButtonW * 4) + (kWeightButtonGap * 3);
  constexpr lv_coord_t kWeightGroupX = (kRightCardW - kWeightGroupW) / 2;
  lv_obj_set_size(weightControls, kWeightGroupW, kWeightButtonH);
  lv_obj_set_style_bg_opa(weightControls, LV_OPA_40, 0);
  lv_obj_set_style_bg_color(weightControls, lv_color_hex(0x12202A), 0);
  lv_obj_set_style_radius(weightControls, 12, 0);
  lv_obj_set_style_border_width(weightControls, 0, 0);
  lv_obj_set_style_pad_all(weightControls, 0, 0);
  lv_obj_set_pos(weightControls, kWeightGroupX, 218);
  lv_obj_clear_flag(weightControls, LV_OBJ_FLAG_SCROLLABLE);

  btnWeightMinusLarge_ = createButton(weightControls, "-5", kBtnSecondary);
  lv_obj_set_size(btnWeightMinusLarge_, kWeightButtonW, kWeightButtonH);
  lv_obj_set_pos(btnWeightMinusLarge_, 0, 0);
  lv_obj_set_style_pad_all(btnWeightMinusLarge_, 2, 0);

  btnWeightMinus_ = createButton(weightControls, "-2.5", kBtnSecondary);
  lv_obj_set_size(btnWeightMinus_, kWeightButtonW, kWeightButtonH);
  lv_obj_set_pos(btnWeightMinus_, kWeightButtonW + kWeightButtonGap, 0);
  lv_obj_set_style_pad_all(btnWeightMinus_, 2, 0);

  btnWeightPlus_ = createButton(weightControls, "+2.5", kBtnSecondary);
  lv_obj_set_size(btnWeightPlus_, kWeightButtonW, kWeightButtonH);
  lv_obj_set_pos(btnWeightPlus_, (kWeightButtonW + kWeightButtonGap) * 2, 0);
  lv_obj_set_style_pad_all(btnWeightPlus_, 2, 0);

  btnWeightPlusLarge_ = createButton(weightControls, "+5", kBtnSecondary);
  lv_obj_set_size(btnWeightPlusLarge_, kWeightButtonW, kWeightButtonH);
  lv_obj_set_pos(btnWeightPlusLarge_, (kWeightButtonW + kWeightButtonGap) * 3, 0);
  lv_obj_set_style_pad_all(btnWeightPlusLarge_, 2, 0);

  constexpr lv_coord_t kActionGroupW = (kRightActionButtonWidth * 2) + kRightActionButtonGap;
  constexpr lv_coord_t kActionGroupX = (kRightCardW - kActionGroupW) / 2;
  btnTrain_ = createButton(right, "START", kBtnSecondary);
  lv_obj_set_size(btnTrain_, kRightActionButtonWidth, kRightActionButtonHeight);
  lv_obj_set_pos(btnTrain_, kActionGroupX, kRightActionY);
  lv_obj_set_style_pad_all(btnTrain_, 5, 0);
  lv_obj_set_style_border_color(btnTrain_, lv_color_hex(0x1D9DAA), 0);
  lv_obj_set_style_text_color(btnTrain_, lv_color_hex(0xDDF6FB), 0);

  btnCalibrate_ = createButton(right, "CALIBRATE", kBtnGhost);
  lv_obj_set_size(btnCalibrate_, kRightActionButtonWidth, kRightActionButtonHeight);
  lv_obj_set_pos(btnCalibrate_, kActionGroupX + kRightActionButtonWidth + kRightActionButtonGap, kRightActionY);
  lv_obj_set_style_pad_all(btnCalibrate_, 5, 0);

  auto setSessionButtonFont = [](lv_obj_t* button) {
    if (button == nullptr) {
      return;
    }
    lv_obj_t* label = lv_obj_get_child(button, 0);
    if (label != nullptr) {
      lv_obj_set_style_text_font(label, &lv_font_montserrat_18, 0);
    }
  };

  auto setSmallWeightButtonFont = [](lv_obj_t* button) {
    if (button == nullptr) {
      return;
    }
    lv_obj_t* label = lv_obj_get_child(button, 0);
    if (label != nullptr) {
      lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
    }
  };

  setSmallWeightButtonFont(btnWeightMinusLarge_);
  setSmallWeightButtonFont(btnWeightMinus_);
  setSmallWeightButtonFont(btnWeightPlus_);
  setSmallWeightButtonFont(btnWeightPlusLarge_);
  setSessionButtonFont(btnTrain_);
  setSessionButtonFont(btnCalibrate_);

  calibrationBar_ = lv_bar_create(right);
  lv_obj_set_size(calibrationBar_, kRightPanelInnerWidth, 10);
  lv_obj_set_pos(calibrationBar_, kRightPanelPadX, kRightActionY - 16);
  lv_bar_set_range(calibrationBar_, 0, CalibrationService::kTargetReps);
  lv_obj_set_style_bg_color(calibrationBar_, lv_color_hex(0x0B1A1D), LV_PART_MAIN);
  lv_obj_set_style_bg_color(calibrationBar_, lv_color_hex(0x86E7F0), LV_PART_INDICATOR);
  lv_obj_add_flag(calibrationBar_, LV_OBJ_FLAG_HIDDEN);
  logEvent("UI_LAYOUT",
           String("screen w=") + String(kScreenW) + " h=" + String(kScreenH),
           LogLevel::Normal);
  logEvent("UI_LAYOUT",
           String("left card x=") + String(kLeftCardX) +
               " y=" + String(kLeftCardY) +
               " w=" + String(kLeftCardW) +
               " h=" + String(kLeftCardH),
           LogLevel::Normal);
  logEvent("UI_LAYOUT",
           String("right available x=") + String(kLeftCardX + kLeftCardW) +
               " w=" + String(kScreenW - (kLeftCardX + kLeftCardW)),
           LogLevel::Normal);
  logEvent("UI_LAYOUT",
           String("right card x=") + String(kRightCardX) +
               " y=" + String(kRightCardY) +
               " w=" + String(kRightCardW) +
               " h=" + String(kRightCardH),
           LogLevel::Normal);
  logEvent("UI_LAYOUT",
           String("right grid padX=") + String(kRightPanelPadX) +
               " innerW=" + String(kRightPanelInnerWidth) +
               " centerX=" + String(kRightCardW / 2),
           LogLevel::Normal);
  logEvent("UI_LAYOUT",
           String("right margins leftGap=") + String(kRightCardX - (kLeftCardX + kLeftCardW)) +
               " rightGap=" + String(kScreenW - (kRightCardX + kRightCardW)),
           LogLevel::Normal);
  logEvent("UI_LAYOUT",
           String("repset groupX=") + String(kRepSetGroupX) +
               " groupW=" + String(kRepSetGroupW) +
               " y=144 cardW=" + String(kRepSetCardW) +
               " gap=" + String(kRepSetGap),
           LogLevel::Normal);
  logEvent("UI_LAYOUT",
           String("weight row groupX=") + String(kWeightGroupX) +
               " groupW=" + String(kWeightGroupW) +
               " y=218 btnW=" + String(kWeightButtonW) +
               " gap=" + String(kWeightButtonGap),
           LogLevel::Normal);
  logEvent("UI_LAYOUT",
           String("action groupX=") + String(kActionGroupX) +
               " groupW=" + String(kActionGroupW) +
               " y=" + String(kRightActionY) +
               " h=" + String(kRightActionButtonHeight) +
               " btnW=" + String(kRightActionButtonWidth) +
               " gap=" + String(kRightActionButtonGap),
           LogLevel::Normal);
  logEvent("UI_LAYOUT",
           String("start x=") + String(kActionGroupX) +
               " y=" + String(kRightActionY) +
               " w=" + String(kRightActionButtonWidth) +
               " h=" + String(kRightActionButtonHeight),
           LogLevel::Normal);
  logEvent("UI_LAYOUT",
           String("secondary x=") +
               String(kActionGroupX + kRightActionButtonWidth + kRightActionButtonGap) +
               " y=" + String(kRightActionY) +
               " w=" + String(kRightActionButtonWidth) +
               " h=" + String(kRightActionButtonHeight),
           LogLevel::Normal);
  logEvent("UI_LAYOUT", "overlap start_secondary=0", LogLevel::Normal);
  logBootMemPoint("after_buildMainPanels_body");
}

void SmartGymTouchApp::buildBottomControls(lv_obj_t* parent) {
  (void)parent;
}

void SmartGymTouchApp::buildDebugModal(lv_obj_t* parent) {
  (void)parent;
  if (debugModal_ != nullptr) {
    return;
  }
  logBootMemPoint("before_debug_modal_body");

  debugModal_ = lv_obj_create(nullptr);
  lv_obj_remove_style_all(debugModal_);
  lv_obj_add_style(debugModal_, &styleModal_, 0);
  lv_obj_set_size(debugModal_, 800, 480);
  lv_obj_clear_flag(debugModal_, LV_OBJ_FLAG_SCROLLABLE);

  debugPanel_ = createPanel(debugModal_, false);
  lv_obj_set_size(debugPanel_, 776, 412);
  lv_obj_center(debugPanel_);

  lv_obj_t* title = lv_label_create(debugPanel_);
  lv_label_set_text(title, "SERVICE & DIAGNOSTICS");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(title, lv_color_hex(0xF2FBFF), 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, -2);

  btnCloseDebug_ = createButton(debugPanel_, "CLOSE", kBtnGhost);
  lv_obj_set_size(btnCloseDebug_, 112, 38);
  lv_obj_align(btnCloseDebug_, LV_ALIGN_TOP_RIGHT, 0, -2);

  lv_obj_t* statusCard = createPanel(debugPanel_, true);
  lv_obj_set_size(statusCard, 748, 86);
  lv_obj_align(statusCard, LV_ALIGN_TOP_LEFT, 0, 34);
  lv_obj_set_style_pad_all(statusCard, 8, 0);

  debugStatusLabel_ = lv_label_create(statusCard);
  lv_obj_set_size(debugStatusLabel_, 732, 38);
  lv_label_set_long_mode(debugStatusLabel_, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(debugStatusLabel_, &lv_font_montserrat_10, 0);
  lv_obj_set_style_text_color(debugStatusLabel_, lv_color_hex(0xA8C0CC), 0);
  lv_obj_align(debugStatusLabel_, LV_ALIGN_TOP_LEFT, 0, 0);

  debugHardwareLabel_ = lv_label_create(statusCard);
  lv_obj_set_size(debugHardwareLabel_, 732, 32);
  lv_label_set_long_mode(debugHardwareLabel_, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(debugHardwareLabel_, &lv_font_montserrat_10, 0);
  lv_obj_set_style_text_color(debugHardwareLabel_, lv_color_hex(0x8FD9D2), 0);
  lv_obj_align(debugHardwareLabel_, LV_ALIGN_TOP_LEFT, 0, 44);

  lv_obj_t* leftCard = createPanel(debugPanel_, true);
  lv_obj_set_size(leftCard, 252, 254);
  lv_obj_align(leftCard, LV_ALIGN_TOP_LEFT, 0, 124);
  lv_obj_set_style_pad_all(leftCard, 8, 0);

  lv_obj_t* middleCard = createPanel(debugPanel_, true);
  lv_obj_set_size(middleCard, 190, 254);
  lv_obj_align(middleCard, LV_ALIGN_TOP_LEFT, 260, 124);
  lv_obj_set_style_pad_all(middleCard, 8, 0);

  lv_obj_t* rightCard = createPanel(debugPanel_, true);
  lv_obj_set_size(rightCard, 290, 254);
  lv_obj_align(rightCard, LV_ALIGN_TOP_LEFT, 458, 124);
  lv_obj_set_style_pad_all(rightCard, 8, 0);

  lv_obj_t* machineLbl = lv_label_create(leftCard);
  lv_label_set_text(machineLbl, "Machine & Motion");
  lv_obj_set_style_text_font(machineLbl, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(machineLbl, lv_color_hex(0xD9EDF5), 0);
  lv_obj_align(machineLbl, LV_ALIGN_TOP_LEFT, 0, 0);

  logBootMemPoint("before_machineOptions");
  machineDropdown_ = lv_dropdown_create(leftCard);
  lv_dropdown_set_options(machineDropdown_, kMachineOptions);
  lv_obj_set_size(machineDropdown_, 216, 34);
  lv_obj_align(machineDropdown_, LV_ALIGN_TOP_LEFT, 0, 24);
  lv_obj_add_event_cb(machineDropdown_, onMachineChanged, LV_EVENT_VALUE_CHANGED, this);
  logBootMemPoint("after_machineOptions");

  lv_obj_t* romLbl = lv_label_create(leftCard);
  lv_label_set_text(romLbl, "ROM injector");
  lv_obj_set_style_text_font(romLbl, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(romLbl, lv_color_hex(0xD9EDF5), 0);
  lv_obj_align(romLbl, LV_ALIGN_TOP_LEFT, 0, 72);

  romSlider_ = lv_slider_create(leftCard);
  lv_obj_set_size(romSlider_, 216, 18);
  lv_obj_align(romSlider_, LV_ALIGN_TOP_LEFT, 0, 94);
  lv_slider_set_range(romSlider_, 0, 100);
  syncRomDebugSliderIfPresent();
  lv_obj_add_event_cb(romSlider_, onSliderEvent, LV_EVENT_VALUE_CHANGED, this);

  btnJumpBottom_ = createButton(leftCard, "ROM 0", kBtnSecondary);
  lv_obj_set_size(btnJumpBottom_, 68, 30);
  lv_obj_align(btnJumpBottom_, LV_ALIGN_TOP_LEFT, 0, 122);

  btnJumpTop_ = createButton(leftCard, "ROM 100", kBtnSecondary);
  lv_obj_set_size(btnJumpTop_, 78, 30);
  lv_obj_align(btnJumpTop_, LV_ALIGN_TOP_LEFT, 74, 122);

  btnResetMotion_ = createButton(leftCard, "RESET", kBtnSecondary);
  lv_obj_set_size(btnResetMotion_, 68, 30);
  lv_obj_align(btnResetMotion_, LV_ALIGN_TOP_LEFT, 148, 122);

  btnToggleSensorMode_ = createButton(leftCard, "LIVE/SIM", kBtnSecondary);
  lv_obj_set_size(btnToggleSensorMode_, 104, 32);
  lv_obj_align(btnToggleSensorMode_, LV_ALIGN_TOP_LEFT, 0, 146);

  btnDebugCalibrate_ = createButton(leftCard, "USER CALIB", kBtnSecondary);
  lv_obj_set_size(btnDebugCalibrate_, 104, 32);
  lv_obj_align(btnDebugCalibrate_, LV_ALIGN_TOP_LEFT, 112, 146);

  btnAutoRep_ = createButton(leftCard, "AUTO REP", kBtnGhost);
  lv_obj_set_size(btnAutoRep_, 104, 32);
  lv_obj_align(btnAutoRep_, LV_ALIGN_TOP_LEFT, 0, 182);
  lv_obj_add_event_cb(btnAutoRep_, onButtonEvent, LV_EVENT_LONG_PRESSED, this);
  lv_obj_add_event_cb(btnAutoRep_, onButtonEvent, LV_EVENT_RELEASED, this);

  btnSyncCloud_ = createButton(leftCard, "SYNC", kBtnPrimary);
  lv_obj_set_size(btnSyncCloud_, 104, 32);
  lv_obj_align(btnSyncCloud_, LV_ALIGN_TOP_LEFT, 112, 182);

  btnLogLevel_ = createButton(leftCard, "LOG:NORMAL", kBtnSecondary);
  lv_obj_set_size(btnLogLevel_, 216, 26);
  lv_obj_align(btnLogLevel_, LV_ALIGN_TOP_LEFT, 0, 216);

  lv_obj_t* userLbl = lv_label_create(middleCard);
  lv_label_set_text(userLbl, "User / RFID");
  lv_obj_set_style_text_font(userLbl, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(userLbl, lv_color_hex(0xD9EDF5), 0);
  lv_obj_align(userLbl, LV_ALIGN_TOP_LEFT, 0, 0);

  btnUser1_ = createButton(middleCard, "U1", kBtnSecondary);
  lv_obj_set_size(btnUser1_, 76, 34);
  lv_obj_align(btnUser1_, LV_ALIGN_TOP_LEFT, 0, 24);

  btnUser2_ = createButton(middleCard, "U2", kBtnSecondary);
  lv_obj_set_size(btnUser2_, 76, 34);
  lv_obj_align(btnUser2_, LV_ALIGN_TOP_LEFT, 84, 24);

  btnNewUser_ = createButton(middleCard, "NEW USER", kBtnSecondary);
  lv_obj_set_size(btnNewUser_, 160, 34);
  lv_obj_align(btnNewUser_, LV_ALIGN_TOP_LEFT, 0, 64);

  btnAnonymous_ = createButton(middleCard, "ANON MODE", kBtnSecondary);
  lv_obj_set_size(btnAnonymous_, 160, 34);
  lv_obj_align(btnAnonymous_, LV_ALIGN_TOP_LEFT, 0, 104);

  lv_obj_t* encoderLbl = lv_label_create(rightCard);
  lv_label_set_text(encoderLbl, "Encoder Limits");
  lv_obj_set_style_text_font(encoderLbl, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(encoderLbl, lv_color_hex(0xD9EDF5), 0);
  lv_obj_align(encoderLbl, LV_ALIGN_TOP_LEFT, 0, 0);

  btnEncoderZero_ = createButton(rightCard, "SET MIN", kBtnSecondary);
  lv_obj_set_size(btnEncoderZero_, 126, 32);
  lv_obj_align(btnEncoderZero_, LV_ALIGN_TOP_LEFT, 0, 24);

  btnEncoderFull_ = createButton(rightCard, "SET MAX", kBtnSecondary);
  lv_obj_set_size(btnEncoderFull_, 126, 32);
  lv_obj_align(btnEncoderFull_, LV_ALIGN_TOP_LEFT, 136, 24);

  btnEncoderApply_ = createButton(rightCard, "SAVE", kBtnPrimary);
  lv_obj_set_size(btnEncoderApply_, 126, 32);
  lv_obj_align(btnEncoderApply_, LV_ALIGN_TOP_LEFT, 0, 62);

  btnEncoderReset_ = createButton(rightCard, "RESET", kBtnGhost);
  lv_obj_set_size(btnEncoderReset_, 126, 32);
  lv_obj_align(btnEncoderReset_, LV_ALIGN_TOP_LEFT, 136, 62);

  lv_obj_t* userRomLbl = lv_label_create(rightCard);
  lv_label_set_text(userRomLbl, "User ROM");
  lv_obj_set_style_text_font(userRomLbl, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(userRomLbl, lv_color_hex(0xD9EDF5), 0);
  lv_obj_align(userRomLbl, LV_ALIGN_TOP_LEFT, 0, 106);

  btnUserRomBottom_ = createButton(rightCard, "BOTTOM", kBtnSecondary);
  lv_obj_set_size(btnUserRomBottom_, 126, 32);
  lv_obj_align(btnUserRomBottom_, LV_ALIGN_TOP_LEFT, 0, 130);

  btnUserRomTop_ = createButton(rightCard, "TOP", kBtnSecondary);
  lv_obj_set_size(btnUserRomTop_, 126, 32);
  lv_obj_align(btnUserRomTop_, LV_ALIGN_TOP_LEFT, 136, 130);

  btnUserRomApply_ = createButton(rightCard, "SAVE ROM", kBtnPrimary);
  lv_obj_set_size(btnUserRomApply_, 262, 32);
  lv_obj_align(btnUserRomApply_, LV_ALIGN_TOP_LEFT, 0, 168);

  lv_obj_t* quickLbl = lv_label_create(rightCard);
  lv_label_set_text(quickLbl, "Quick ops");
  lv_obj_set_style_text_font(quickLbl, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(quickLbl, lv_color_hex(0xD9EDF5), 0);
  lv_obj_align(quickLbl, LV_ALIGN_TOP_LEFT, 0, 198);

  lv_obj_t* quickHint = lv_label_create(rightCard);
  lv_obj_set_width(quickHint, 262);
  lv_label_set_long_mode(quickHint, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(quickHint, &lv_font_montserrat_10, 0);
  lv_obj_set_style_text_color(quickHint, lv_color_hex(0x97AFB8), 0);
  lv_label_set_text(quickHint, "AUTO REP: hold. SYNC: upload queue.");
  lv_obj_align(quickHint, LV_ALIGN_TOP_LEFT, 0, 216);

  auto setDebugButtonFont = [](lv_obj_t* button) {
    if (button == nullptr) {
      return;
    }
    lv_obj_t* label = lv_obj_get_child(button, 0);
    if (label != nullptr) {
      lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    }
  };
  setDebugButtonFont(btnCloseDebug_);
  setDebugButtonFont(btnJumpBottom_);
  setDebugButtonFont(btnJumpTop_);
  setDebugButtonFont(btnResetMotion_);
  setDebugButtonFont(btnToggleSensorMode_);
  setDebugButtonFont(btnDebugCalibrate_);
  setDebugButtonFont(btnAutoRep_);
  setDebugButtonFont(btnSyncCloud_);
  setDebugButtonFont(btnLogLevel_);
  setDebugButtonFont(btnUser1_);
  setDebugButtonFont(btnUser2_);
  setDebugButtonFont(btnNewUser_);
  setDebugButtonFont(btnAnonymous_);
  setDebugButtonFont(btnEncoderZero_);
  setDebugButtonFont(btnEncoderFull_);
  setDebugButtonFont(btnEncoderApply_);
  setDebugButtonFont(btnEncoderReset_);
  setDebugButtonFont(btnUserRomBottom_);
  setDebugButtonFont(btnUserRomTop_);
  setDebugButtonFont(btnUserRomApply_);
  if (machineProfile_ != nullptr) {
    for (uint8_t i = 0; i < kMachineCount; ++i) {
      if (String(kMachineIds[i]).equalsIgnoreCase(machineProfile_->machineId)) {
        lv_dropdown_set_selected(machineDropdown_, i);
        uiCache_.lastDropdownIndex = i;
        break;
      }
    }
  }
  syncRomDebugSliderIfPresent();
  logEvent("UI", "debug controls initialized", LogLevel::Normal);
  logBootMemPoint("after_debug_modal_body");
}

void SmartGymTouchApp::destroyDebugModalIfPresent() {
  if (debugModal_ == nullptr) {
    return;
  }
  if (currentUiScreen_ == UiScreenMode::Debug) {
    showUiScreen(UiScreenMode::Main);
  }
  lv_obj_del(debugModal_);
  debugModal_ = nullptr;
  debugPanel_ = nullptr;
  debugStatusLabel_ = nullptr;
  debugHardwareLabel_ = nullptr;
  machineDropdown_ = nullptr;
  romSlider_ = nullptr;
  btnCloseDebug_ = nullptr;
  btnUser1_ = nullptr;
  btnUser2_ = nullptr;
  btnNewUser_ = nullptr;
  btnAnonymous_ = nullptr;
  btnAutoRep_ = nullptr;
  btnResetMotion_ = nullptr;
  btnJumpTop_ = nullptr;
  btnJumpBottom_ = nullptr;
  btnSyncCloud_ = nullptr;
  btnToggleSensorMode_ = nullptr;
  btnLogLevel_ = nullptr;
  btnEncoderZero_ = nullptr;
  btnEncoderFull_ = nullptr;
  btnEncoderApply_ = nullptr;
  btnEncoderReset_ = nullptr;
  btnUserRomBottom_ = nullptr;
  btnUserRomTop_ = nullptr;
  btnUserRomApply_ = nullptr;
  btnDebugCalibrate_ = nullptr;
  uiCache_.lastDropdownIndex = 0xFFFF;
}

void SmartGymTouchApp::destroyIdleOverlayIfPresent() {
  if (idleOverlay_ == nullptr) {
    return;
  }
  if (currentUiScreen_ == UiScreenMode::Idle) {
    showUiScreen(UiScreenMode::Main);
  }
  lv_obj_del(idleOverlay_);
  idleOverlay_ = nullptr;
  idlePanel_ = nullptr;
  idleBadgeLabel_ = nullptr;
  idleTitleLabel_ = nullptr;
  idleMachineLabel_ = nullptr;
  idlePromptLabel_ = nullptr;
  idleHintLabel_ = nullptr;
  uiCache_.lastIdleVisible = false;
}

void SmartGymTouchApp::destroyProfileScreenIfPresent() {
  if (profileScreen_ == nullptr) {
    return;
  }
  if (currentUiScreen_ == UiScreenMode::Profile) {
    showUiScreen(UiScreenMode::Main);
  }
  lv_obj_del(profileScreen_);
  profileScreen_ = nullptr;
  profilePanel_ = nullptr;
  profileTitleLabel_ = nullptr;
  profileUidLabel_ = nullptr;
  profileNameValueLabel_ = nullptr;
  btnProfileEditName_ = nullptr;
  profileAgeValueLabel_ = nullptr;
  profileWeightValueLabel_ = nullptr;
  profileHeightValueLabel_ = nullptr;
  profileGoalValueLabel_ = nullptr;
  profileGenderValueLabel_ = nullptr;
  btnProfileNamePrev_ = nullptr;
  btnProfileNameNext_ = nullptr;
  btnProfileAgeMinus_ = nullptr;
  btnProfileAgePlus_ = nullptr;
  btnProfileWeightMinus_ = nullptr;
  btnProfileWeightPlus_ = nullptr;
  btnProfileHeightMinus_ = nullptr;
  btnProfileHeightPlus_ = nullptr;
  btnProfileGoalPrev_ = nullptr;
  btnProfileGoalNext_ = nullptr;
  btnProfileGenderPrev_ = nullptr;
  btnProfileGenderNext_ = nullptr;
  btnProfileSave_ = nullptr;
  btnProfileCancel_ = nullptr;
  profileNameTa_ = nullptr;
  profileKeyboard_ = nullptr;
}

void SmartGymTouchApp::destroyCalibrationUiIfPresent() {
  if (calibrationScreen_ != nullptr) {
    if (currentUiScreen_ == UiScreenMode::Calibration) {
      showUiScreen(UiScreenMode::Main);
    }
    lv_obj_t* screenToDelete = calibrationScreen_;
    clearCalibrationUiPointers();
    lv_obj_del(screenToDelete);
    logEvent("UI", String("calibration ui destroyed heap=") + String(ESP.getFreeHeap()), LogLevel::Normal);
  }
  destroyCalibrationGateScreenIfPresent();
}

void SmartGymTouchApp::clearCalibrationUiPointers() {
  calibrationScreen_ = nullptr;
  calibrationVisualPanel_ = nullptr;
  calibrationInstructionPanel_ = nullptr;
  calibrationHeaderLabel_ = nullptr;
  calibrationMachineLabel_ = nullptr;
  calibrationStepLabel_ = nullptr;
  calibrationInstructionLabel_ = nullptr;
  calibrationLoadLabel_ = nullptr;
  calibrationMetricLabel_ = nullptr;
  calibrationFeedbackLabel_ = nullptr;
  calibrationRomBar_ = nullptr;
  btnCalibrationWeightMinusLarge_ = nullptr;
  btnCalibrationWeightMinus_ = nullptr;
  btnCalibrationWeightPlus_ = nullptr;
  btnCalibrationWeightPlusLarge_ = nullptr;
  btnCalibrationPrimary_ = nullptr;
  btnCalibrationSecondary_ = nullptr;
  btnCalibrationCancel_ = nullptr;
  calibrationPinButtonsEnabled_ = true;
}

void SmartGymTouchApp::destroyCalibrationUiForCancel(const char* reason) {
  const bool cancelReason = reason == nullptr || String(reason).equalsIgnoreCase("calibration_cancel");
  logEvent("CAL",
           cancelReason ? String("cancel ui destroy begin")
                        : String("ui destroy begin reason=") + reason,
           LogLevel::Normal);
  if (calibrationScreen_ != nullptr) {
    lv_obj_t* screenToDelete = calibrationScreen_;
    clearCalibrationUiPointers();
    lv_obj_del_async(screenToDelete);
  }
  logEvent("CAL",
           cancelReason ? String("cancel ui destroy end")
                        : String("ui destroy end reason=") + reason,
           LogLevel::Normal);
}

void SmartGymTouchApp::destroyCalibrationGateScreenIfPresent() {
  if (calibrationGateScreen_ == nullptr) {
    return;
  }
  if (currentUiScreen_ == UiScreenMode::CalibrationGate) {
    showUiScreen(UiScreenMode::Main);
  }
  lv_obj_del(calibrationGateScreen_);
  calibrationGateScreen_ = nullptr;
  calibrationGatePanel_ = nullptr;
  calibrationGateTitleLabel_ = nullptr;
  calibrationGateBodyLabel_ = nullptr;
  calibrationGateMachineLabel_ = nullptr;
  calibrationGateHintLabel_ = nullptr;
  btnCalibrationGateSkip_ = nullptr;
  btnCalibrationGateCalibrate_ = nullptr;
  uiCache_.lastCalibrationGateVisible = false;
}

void SmartGymTouchApp::destroyUserSwitchPromptIfPresent() {
  if (userSwitchPrompt_ == nullptr) {
    return;
  }
  lv_obj_del(userSwitchPrompt_);
  userSwitchPrompt_ = nullptr;
  userSwitchPromptTitle_ = nullptr;
  userSwitchPromptBody_ = nullptr;
  btnUserSwitchConfirm_ = nullptr;
  btnUserSwitchCancel_ = nullptr;
  pendingSwitchUid_ = "";
}

void SmartGymTouchApp::freeOptionalUiForMemory(const char* reason, bool keepSummary) {
  const uint32_t heapBefore = ESP.getFreeHeap();
  const uint32_t largestBefore = internalLargestFree8BitBlock();
  if (!keepSummary && summaryScreen_ != nullptr) {
    if (currentUiScreen_ == UiScreenMode::Summary) {
      showUiScreen(UiScreenMode::Main);
    }
    lv_obj_del(summaryScreen_);
    summaryScreen_ = nullptr;
    summaryPanel_ = nullptr;
    summaryBadgeLabel_ = nullptr;
    summaryTitleLabel_ = nullptr;
    summaryUserLabel_ = nullptr;
    summaryResultLabel_ = nullptr;
    summaryRepsLabel_ = nullptr;
    summaryRomLabel_ = nullptr;
    summaryLoadLabel_ = nullptr;
    summaryCoachLabel_ = nullptr;
    summaryDetailLabel_ = nullptr;
    summaryCountdownLabel_ = nullptr;
    btnSummarySkip_ = nullptr;
    uiCache_.lastSummaryVisible = false;
  }
  destroyDebugModalIfPresent();
  destroyIdleOverlayIfPresent();
  destroyProfileScreenIfPresent();
  destroyCalibrationUiIfPresent();
  destroyUserSwitchPromptIfPresent();
  const uint32_t heapAfter = ESP.getFreeHeap();
  const uint32_t largestAfter = internalLargestFree8BitBlock();
  logEvent("UI",
           String("freed optional ui reason=") + (reason != nullptr ? String(reason) : String("unknown")) +
               " heapBefore=" + String(heapBefore) +
               " heapAfter=" + String(heapAfter) +
               " largestBefore=" + String(largestBefore) +
               " largestAfter=" + String(largestAfter),
           LogLevel::Normal);
}

void SmartGymTouchApp::freeOptionalUiForUpload(const char* reason, bool keepSummary) {
  freeOptionalUiForMemory(reason, keepSummary);
}

void SmartGymTouchApp::refreshUi() {
  const uint32_t nowMs = millis();
  constexpr lv_coord_t kRightCardW = 292;
  constexpr lv_coord_t kRightPanelPadX = 22;
  constexpr lv_coord_t kRightActionButtonGap = 12;
  constexpr lv_coord_t kRightPanelInnerWidth = kRightCardW - (2 * kRightPanelPadX);
  constexpr lv_coord_t kRightActionButtonWidth = (kRightPanelInnerWidth - kRightActionButtonGap) / 2;
  constexpr lv_coord_t kRightActionButtonHeight = 48;
  constexpr lv_coord_t kRightActionButtonY = 314;
  constexpr lv_coord_t kActionGroupW = (kRightActionButtonWidth * 2) + kRightActionButtonGap;
  constexpr lv_coord_t kActionGroupX = (kRightCardW - kActionGroupW) / 2;
  const bool mainControlsVisible = currentUiScreen_ == UiScreenMode::Main;
  if (mainControlsVisible && isValidLvObj(btnTrain_)) {
    const bool sessionActive = canFinishNow(nowMs);
    if (sessionActive) {
      lv_obj_set_size(btnTrain_, kRightPanelInnerWidth, kRightActionButtonHeight);
      lv_obj_set_pos(btnTrain_, kRightPanelPadX, kRightActionButtonY);
    } else {
      lv_obj_set_size(btnTrain_, kRightActionButtonWidth, kRightActionButtonHeight);
      lv_obj_set_pos(btnTrain_, kActionGroupX, kRightActionButtonY);
    }
    if (state_ == State::Calibration) {
      const char* secondaryText = "CANCEL";
      if (calibrationFlowState_ == CalibrationFlowState::ConfirmStartWeight) {
        secondaryText = "USE DIFFERENT";
      } else if (calibrationFlowState_ == CalibrationFlowState::AskNextSet ||
                 calibrationFlowState_ == CalibrationFlowState::Result) {
        secondaryText = "FINISH";
      }
      setButtonTextIfValid(btnTrain_, secondaryText);
    } else {
      setButtonTextIfValid(btnTrain_, sessionActive ? "FINISH" : "START");
    }
    if (state_ == State::Calibration) {
      lv_obj_set_style_bg_color(btnTrain_, lv_color_hex(0x151E24), 0);
      lv_obj_set_style_border_color(btnTrain_, lv_color_hex(0x2C3A43), 0);
      lv_obj_set_style_text_color(btnTrain_, lv_color_hex(0xDDF6FB), 0);
    } else if (sessionActive) {
      lv_obj_set_style_bg_color(btnTrain_, lv_color_hex(0xC84D4D), 0);
      lv_obj_set_style_border_color(btnTrain_, lv_color_hex(0xC84D4D), 0);
      lv_obj_set_style_text_color(btnTrain_, lv_color_hex(0xFFFFFF), 0);
    } else {
      lv_obj_set_style_bg_color(btnTrain_, lv_color_hex(0x1E4250), 0);
      lv_obj_set_style_border_color(btnTrain_, lv_color_hex(0x1D9DAA), 0);
      lv_obj_set_style_text_color(btnTrain_, lv_color_hex(0xDDF6FB), 0);
    }
  }
  if (mainControlsVisible && isValidLvObj(btnCalibrate_)) {
    lv_obj_clear_flag(btnCalibrate_, LV_OBJ_FLAG_HIDDEN);
    const bool hasUser = activeUser_ != nullptr || anonymousMode_;
    if (state_ == State::Training || state_ == State::Summary) {
      lv_obj_add_flag(btnCalibrate_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(btnCalibrate_, LV_OBJ_FLAG_CLICKABLE);
    } else if (state_ == State::Calibration) {
      lv_obj_add_flag(btnCalibrate_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(btnCalibrate_, LV_OBJ_FLAG_CLICKABLE);
    } else if (!hasUser) {
      lv_obj_set_size(btnCalibrate_, kRightActionButtonWidth, kRightActionButtonHeight);
      lv_obj_set_pos(btnCalibrate_, kActionGroupX + kRightActionButtonWidth + kRightActionButtonGap,
                     kRightActionButtonY);
      setButtonTextIfValid(btnCalibrate_, "SCAN RFID");
      lv_obj_clear_flag(btnCalibrate_, LV_OBJ_FLAG_CLICKABLE);
    } else {
      lv_obj_set_size(btnCalibrate_, kRightActionButtonWidth, kRightActionButtonHeight);
      lv_obj_set_pos(btnCalibrate_, kActionGroupX + kRightActionButtonWidth + kRightActionButtonGap,
                     kRightActionButtonY);
      setButtonTextIfValid(btnCalibrate_, "CALIBRATE");
      lv_obj_add_flag(btnCalibrate_, LV_OBJ_FLAG_CLICKABLE);
    }
  }

  if (currentUiScreen_ == UiScreenMode::Main) {
    refreshTopBar();
    refreshMainStats(nowMs);
    refreshStatusArea(nowMs);
    refreshRestOverlay(nowMs);
    refreshIdleOverlay(nowMs);
  } else if (currentUiScreen_ == UiScreenMode::Summary) {
    refreshSummaryScreen(nowMs);
  } else if (currentUiScreen_ == UiScreenMode::Idle) {
    refreshIdleOverlay(nowMs);
  } else if (currentUiScreen_ == UiScreenMode::Calibration) {
    refreshCalibrationScreen(nowMs);
  } else if (currentUiScreen_ == UiScreenMode::CalibrationGate) {
    refreshCalibrationGateScreen(nowMs);
  } else if (currentUiScreen_ == UiScreenMode::Debug) {
    refreshDebugPanel(nowMs);
  } else if (currentUiScreen_ == UiScreenMode::Profile) {
    refreshProfileScreen(nowMs);
  }
}

SmartGymTouchApp::SessionStage SmartGymTouchApp::getSessionStage(uint32_t nowMs) const {
  if (finishTransitionInProgress_) {
    return SessionStage::Logout;
  }
  if (state_ == State::Summary) {
    return SessionStage::Summary;
  }
  if (state_ == State::Calibration) {
    return SessionStage::Calibrate;
  }
  if (state_ == State::Training) {
    if (isRestCountdownActive(nowMs, nullptr)) {
      return SessionStage::Rest;
    }
    return SessionStage::Train;
  }
  if (state_ == State::Idle) {
    if (activeUser_ == nullptr && !anonymousMode_) {
      return SessionStage::Identify;
    }
    return SessionStage::Idle;
  }
  return SessionStage::Idle;
}

bool SmartGymTouchApp::canStartTrainingNow(uint32_t nowMs) const {
  if (nowMs < trainingStartBlockedUntilMs_) {
    return false;
  }
  const SessionStage stage = getSessionStage(nowMs);
  return stage == SessionStage::Idle || stage == SessionStage::Identify;
}

bool SmartGymTouchApp::canFinishNow(uint32_t nowMs) const {
  if (!sessionRecorder_.isActive()) {
    return false;
  }
  const SessionStage stage = getSessionStage(nowMs);
  return stage == SessionStage::Train || stage == SessionStage::Rest;
}

bool SmartGymTouchApp::shouldShowIdleOverlay(uint32_t nowMs) const {
  if (idleStartupLock_ && state_ == State::Idle && activeUser_ == nullptr && !anonymousMode_) {
    return true;
  }
  if (state_ != State::Idle) {
    return false;
  }
  if (isRestCountdownActive(nowMs, nullptr)) {
    return false;
  }
  if (activeUser_ != nullptr || anonymousMode_) {
    return false;
  }
  if (currentUiScreen_ == UiScreenMode::Debug) {
    return false;
  }
  if (currentUiScreen_ == UiScreenMode::CalibrationGate) {
    return false;
  }
  if (currentUiScreen_ == UiScreenMode::Profile) {
    return false;
  }
  if (currentUiScreen_ == UiScreenMode::Summary) {
    return false;
  }
  return nowMs >= idleOverlaySuppressedUntilMs_ && (nowMs - lastUserActivityMs_ >= kIdleAutoShowMs);
}

bool SmartGymTouchApp::isRestCountdownActive(uint32_t nowMs, uint32_t* remainingMs) const {
  if (state_ != State::Training) {
    return false;
  }
  if (currentSetRepCount_ != 0 || lastSetCompletedMs_ == 0 || activeSessionRestSeconds_ == 0) {
    return false;
  }

  const uint32_t restElapsed = nowMs >= lastSetCompletedMs_ ? nowMs - lastSetCompletedMs_ : 0;
  const uint32_t totalRestMs = static_cast<uint32_t>(activeSessionRestSeconds_) * 1000UL;
  if (restElapsed >= totalRestMs) {
    return false;
  }

  if (remainingMs != nullptr) {
    *remainingMs = totalRestMs - restElapsed;
  }
  return true;
}

void SmartGymTouchApp::wakeIdleOverlay(uint32_t nowMs, const String& reason) {
  idleOverlaySuppressedUntilMs_ = nowMs + kIdleSuppressAfterActivityMs;
  idleShowCandidateMs_ = 0;
  idleHideCandidateMs_ = 0;
  markUserActivity(nowMs);
  if (!reason.isEmpty()) {
    setStatusMessage(reason);
  }
  refreshIdleOverlay(nowMs);
}

void SmartGymTouchApp::markUserActivity(uint32_t nowMs) {
  lastUserActivityMs_ = nowMs;
  idleOverlaySuppressedUntilMs_ = nowMs + kIdleSuppressAfterActivityMs;
  idleShowCandidateMs_ = 0;
  idleHideCandidateMs_ = 0;
}

void SmartGymTouchApp::showUiScreen(UiScreenMode mode) {
  if (mode == currentUiScreen_) {
    return;
  }
  const uint32_t nowMs = millis();
  constexpr uint32_t kMinScreenSwitchGapMs = 40UL;
  if (lastScreenSwitchMs_ != 0 && (nowMs - lastScreenSwitchMs_) < kMinScreenSwitchGapMs) {
    return;
  }

  lv_obj_t* target = nullptr;
  switch (mode) {
    case UiScreenMode::Main:
      target = screen_;
      break;
    case UiScreenMode::Summary:
      if (summaryScreen_ == nullptr) {
        logBootMemPoint("before_buildSummaryScreen");
        buildSummaryScreen(screen_);
        logBootMemPoint("after_buildSummaryScreen");
      }
      target = summaryScreen_;
      break;
    case UiScreenMode::Idle:
      if (idleOverlay_ == nullptr) {
        logBootMemPoint("before_buildIdleOverlay");
        buildIdleOverlay(screen_);
        logBootMemPoint("after_buildIdleOverlay");
      }
      target = idleOverlay_;
      break;
    case UiScreenMode::Calibration:
      if (calibrationScreen_ == nullptr) {
        logBootMemPoint("before_calibration_ui_create");
        buildCalibrationScreen(screen_);
        logBootMemPoint("after_calibration_ui_create");
      }
      target = calibrationScreen_;
      break;
    case UiScreenMode::CalibrationGate:
      if (calibrationGateScreen_ == nullptr) {
        logBootMemPoint("before_buildCalibrationGateScreen");
        buildCalibrationGateScreen(screen_);
        logBootMemPoint("after_buildCalibrationGateScreen");
      }
      target = calibrationGateScreen_;
      break;
    case UiScreenMode::Debug:
      if (debugModal_ == nullptr) {
        logBootMemPoint("before_buildDebugModal");
        buildDebugModal(screen_);
        logBootMemPoint("after_buildDebugModal");
      }
      target = debugModal_;
      break;
    case UiScreenMode::Profile:
      if (profileScreen_ == nullptr) {
        logBootMemPoint("before_buildProfileScreen");
        buildProfileScreen(screen_);
        logBootMemPoint("after_buildProfileScreen");
      }
      target = profileScreen_;
      break;
  }

  if (target == nullptr) {
    return;
  }
  if (mode == UiScreenMode::Summary) {
    lv_obj_clear_flag(summaryScreen_, LV_OBJ_FLAG_HIDDEN);
  }
  if (lv_scr_act() == target) {
    currentUiScreen_ = mode;
    return;
  }

  if (kScreenTransitionMs == 0) {
    lv_scr_load(target);
  } else {
    lv_scr_load_anim(target, LV_SCR_LOAD_ANIM_FADE_ON, kScreenTransitionMs, 0, false);
  }
  lastScreenSwitchMs_ = nowMs;
  cloudBlockedUntilMs_ = max<uint32_t>(cloudBlockedUntilMs_, nowMs + kCloudAfterScreenSwitchQuietMs);
  currentUiScreen_ = mode;
}

void SmartGymTouchApp::openUserSwitchPrompt(const String& uid) {
  if (userSwitchPrompt_ == nullptr) {
    logBootMemPoint("before_buildUserSwitchPrompt");
    buildUserSwitchPrompt(screen_);
    logBootMemPoint("after_buildUserSwitchPrompt");
  }
  if (userSwitchPrompt_ == nullptr) {
    return;
  }
  pendingSwitchUid_ = uid;
  lv_obj_clear_flag(userSwitchPrompt_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(userSwitchPrompt_);
  setStatusMessage("Tarjeta de otro usuario detectada. Confirmar cambio.");
  refreshUi();
}

void SmartGymTouchApp::closeUserSwitchPrompt() {
  pendingSwitchUid_ = "";
  if (userSwitchPrompt_ != nullptr) {
    lv_obj_add_flag(userSwitchPrompt_, LV_OBJ_FLAG_HIDDEN);
  }
}

void SmartGymTouchApp::refreshIdleOverlay(uint32_t nowMs) {
  const bool eligible = shouldShowIdleOverlay(nowMs);
  const bool quietEnough = fabsf(simulatedVelocityPctPerSec_) <= 0.5f;
  bool showIdle = currentUiScreen_ == UiScreenMode::Idle;

  if (eligible && quietEnough) {
    if (idleShowCandidateMs_ == 0) {
      idleShowCandidateMs_ = nowMs;
    }
    idleHideCandidateMs_ = 0;
    if (!showIdle && (nowMs - idleShowCandidateMs_) >= 4200UL) {
      showIdle = true;
    }
  } else {
    idleShowCandidateMs_ = 0;
  }

  const bool motionBusy = fabsf(simulatedVelocityPctPerSec_) > 5.0f;
  if (currentUiScreen_ == UiScreenMode::Idle && (!eligible || motionBusy)) {
    if (idleHideCandidateMs_ == 0) {
      idleHideCandidateMs_ = nowMs;
    }
    if ((nowMs - idleHideCandidateMs_) >= 1600UL) {
      showIdle = false;
    } else {
      showIdle = true;
    }
  } else {
    idleHideCandidateMs_ = 0;
  }

  if (showIdle) {
    if (idleOverlay_ == nullptr || idlePanel_ == nullptr) {
      buildIdleOverlay(screen_);
    }
  }

  if (idleOverlay_ == nullptr || idlePanel_ == nullptr) {
    return;
  }

  if (showIdle != uiCache_.lastIdleVisible) {
    uiCache_.lastIdleVisible = showIdle;
    if (showIdle) {
      showUiScreen(UiScreenMode::Idle);
    } else if (currentUiScreen_ == UiScreenMode::Idle) {
      showUiScreen(UiScreenMode::Main);
    }
  }

  if (showIdle) {
    const String machineLine = machineProfile_ != nullptr ? buildMachineText() : String("Select a station");
    lv_label_set_text(idleMachineLabel_, machineLine.c_str());

    const String prompt =
        hardwareRfidEnabled_ ? "Tap your RFID band or card to begin your workout." :
                               "Tap the screen, scan in, or move the machine to get started.";
    lv_label_set_text(idlePromptLabel_, prompt.c_str());

    const String hint = "Personalized coaching, live movement tracking, and guided sets are ready when you are.";
    lv_label_set_text(idleHintLabel_, hint.c_str());
  } else {
    if (currentUiScreen_ == UiScreenMode::Idle) {
      showUiScreen(UiScreenMode::Main);
    }
    return;
  }
}

void SmartGymTouchApp::refreshSummaryScreen(uint32_t nowMs) {
  if (summaryScreen_ == nullptr || summaryPanel_ == nullptr || summaryBadgeLabel_ == nullptr ||
      summaryTitleLabel_ == nullptr || summaryUserLabel_ == nullptr ||
      summaryResultLabel_ == nullptr || summaryRepsLabel_ == nullptr ||
      summaryRomLabel_ == nullptr || summaryLoadLabel_ == nullptr ||
      summaryCoachLabel_ == nullptr || summaryDetailLabel_ == nullptr ||
      summaryCountdownLabel_ == nullptr) {
    return;
  }

  const bool showSummary = currentUiScreen_ == UiScreenMode::Summary;
  uiCache_.lastSummaryVisible = showSummary;

  if (!showSummary) {
    return;
  }

  const SessionHistoryRecord& rec = lastSummaryRecord_;
  const bool hasRecord = !rec.sessionId.isEmpty() ||
                         (rec.validReps + rec.invalidReps) > 0 ||
                         rec.durationMs > 0;

  const String userLine = (lastSessionUserName_.isEmpty() ? String("Card User")
                                                          : lastSessionUserName_) +
                          " | " +
                          (lastSessionMachineName_.isEmpty() ? String("Unknown machine")
                                                             : lastSessionMachineName_);
  if (userLine != uiCache_.lastSummaryUser) {
    lv_label_set_text(summaryUserLabel_, userLine.c_str());
    uiCache_.lastSummaryUser = userLine;
  }

  if (!hasRecord) {
    const String emptyResult = "DONE\nSession saved";
    if (emptyResult != uiCache_.lastSummaryResult) {
      lv_obj_set_style_bg_color(summaryResultLabel_, lv_color_hex(0x071517), 0);
      lv_obj_set_style_border_color(summaryResultLabel_, lv_color_hex(0x1D9DAA), 0);
      lv_obj_set_style_text_color(summaryResultLabel_, lv_color_hex(0xEAF6FB), 0);
      lv_label_set_text(summaryResultLabel_, emptyResult.c_str());
      lv_label_set_text(summaryRepsLabel_, "REPS\nNo rep data");
      lv_label_set_text(summaryRomLabel_, "ROM\nNo ROM data");
      lv_label_set_text(summaryLoadLabel_, "PIN LOAD\nKeep current");
      lv_label_set_text(summaryCoachLabel_, "COACH\nStart again when ready");
      uiCache_.lastSummaryResult = emptyResult;
    }
  } else {
    String tier = rec.sessionQualityTier.isEmpty() ? String("ok") : rec.sessionQualityTier;
    tier.toUpperCase();
    const float score = constrain(rec.sessionQualityScore, 0.0f, 100.0f);
    const uint32_t totalAttempts =
        static_cast<uint32_t>(rec.validReps) + static_cast<uint32_t>(rec.invalidReps);
    const uint16_t plannedReps =
        static_cast<uint16_t>(max<uint16_t>(1, rec.targetSets) *
                              max<uint16_t>(1, rec.targetRepsMax));
    const float validRate = totalAttempts > 0
                                ? (static_cast<float>(rec.validReps) * 100.0f) /
                                      static_cast<float>(totalAttempts)
                                : 0.0f;
    const float completion = plannedReps > 0
                                 ? (static_cast<float>(rec.validReps) * 100.0f) /
                                       static_cast<float>(plannedReps)
                                 : 0.0f;
    const uint32_t durationSec = rec.durationMs / 1000UL;
    const uint32_t durationMin = durationSec / 60UL;
    const uint32_t durationRemSec = durationSec % 60UL;

    lv_color_t resultBorder = lv_color_hex(0x1D9DAA);
    lv_color_t resultFg = lv_color_hex(0xEAF6FB);
    if (score >= 82.0f) {
      lv_obj_set_style_bg_color(summaryResultLabel_, lv_color_hex(0x0B2A22), 0);
      resultBorder = lv_color_hex(0x1DAA93);
      resultFg = lv_color_hex(0xCFFCF1);
    } else if (score < 62.0f) {
      lv_obj_set_style_bg_color(summaryResultLabel_, lv_color_hex(0x2A1111), 0);
      resultBorder = lv_color_hex(0xC84D4D);
      resultFg = lv_color_hex(0xFFD8C9);
    } else if (score < 72.0f) {
      lv_obj_set_style_bg_color(summaryResultLabel_, lv_color_hex(0x2D2110), 0);
      resultBorder = lv_color_hex(0xD28D2A);
      resultFg = lv_color_hex(0xFFE6B7);
    } else {
      lv_obj_set_style_bg_color(summaryResultLabel_, lv_color_hex(0x071517), 0);
    }
    lv_obj_set_style_border_color(summaryResultLabel_, resultBorder, 0);
    lv_obj_set_style_text_color(summaryResultLabel_, resultFg, 0);

    const String resultText = "QUALITY\n" + tier + "\n" + String(score, 0) + "/100";
    const String repsText = "REPS\n" + String(rec.validReps) + "/" + String(plannedReps) +
                            " good\n" + String(rec.invalidReps) + " check";
    const String romText = "ROM\nAvg " + String(rec.avgRomPercent, 0) + "% | Best " +
                           String(rec.bestRomPercent, 0) + "%\nTiming quality tracked";

    float nextWeight = resolvedRecommendation_.hasRecommendation
                           ? resolvedRecommendation_.kg
                           : rec.selectedWeightKg;
    String loadAction = "keep";
    String loadReason = resolvedRecommendation_.reason.isEmpty()
                            ? String("Session quality held target.")
                            : resolvedRecommendation_.reason;
    const bool strongSession = score >= 82.0f && completion >= 95.0f &&
                               validRate >= 90.0f && rec.avgRomPercent >= 85.0f &&
                               rec.invalidReps <= 1;
    const bool weakSession = score < 62.0f || completion < 70.0f ||
                             validRate < 70.0f || rec.avgRomPercent < 65.0f;
    if (strongSession) {
      loadAction = "increase";
    } else if (weakSession) {
      loadAction = "decrease";
    }
    const String loadText = "PIN LOAD\n" + String(selectedWeightKg_, 1) +
                            " kg\nNEW REC " + String(nextWeight, 1) +
                            " kg | " + loadAction;

    String coachText = "COACH\nGood control\nrepeat this setup";
    if (rec.invalidReps > 0) {
      coachText = "COACH\nFix checked reps\nbefore adding load";
    } else if (rec.avgRomPercent < 85.0f) {
      coachText = "COACH\nReach full ROM\nbefore adding load";
    } else if (rec.avgPeakVelocityPctPerSec < 45.0f) {
      coachText = "COACH\nMove with intent\nor lower load";
    } else if (strongSession) {
      coachText = "COACH\nStrong session\nready to progress";
    }

    const String summaryKey = resultText + repsText + romText + loadText + coachText;
    if (summaryKey != uiCache_.lastSummaryResult) {
      lv_label_set_text(summaryResultLabel_, resultText.c_str());
      lv_label_set_text(summaryRepsLabel_, repsText.c_str());
      lv_label_set_text(summaryRomLabel_, romText.c_str());
      lv_label_set_text(summaryLoadLabel_, loadText.c_str());
      lv_label_set_text(summaryCoachLabel_, coachText.c_str());
      uiCache_.lastSummaryResult = summaryKey;
    }
  }

  String detail;
  bool uploadBusy = false;
  uint16_t pendingTotal = 0;
  const char* busyReason = nullptr;
  if (!cloudEnabled_) {
    detail = "Session saved locally | Cloud off";
    summaryWaitingForSync_ = false;
  } else {
    uint16_t busyQueueCount = 0;
    uploadBusy = isSessionUploadBusy(nowMs, &busyReason, &busyQueueCount);
    const uint16_t pendingCompactJobs = pendingSessionQueueEnqueue_ ? 1U : 0U;
    pendingTotal = static_cast<uint16_t>(busyQueueCount + pendingCompactJobs);
    summaryWaitingForSync_ = uploadBusy || pendingTotal > 0;
    if (uploadBusy || pendingTotal > 0) {
      detail = summaryUploadStageText_.isEmpty() ? String("Saving session... ")
                                                 : (summaryUploadStageText_ + " ");
      detail += "Queue ";
      detail += String(pendingTotal);
      if (busyReason != nullptr) {
        detail += " | ";
        detail += busyReason;
      }
    } else {
      detail = "Session saved.";
    }

    String syncStatus = summaryWaitingForSync_ ? "saving" : "saved";
    if (syncStatus != lastSummarySyncStatus_) {
      if (summaryWaitingForSync_) {
        logEvent("SUMMARY",
                 String("upload status saving queue=") + String(pendingTotal),
                 LogLevel::Normal);
      } else {
        logEvent("SUMMARY", "upload status saved", LogLevel::Normal);
      }
      lastSummarySyncStatus_ = syncStatus;
    }
  }
  detail += "\n";
  if (hasRecord) {
    const uint32_t durationSec = rec.durationMs / 1000UL;
    detail += "Time " + String(durationSec / 60UL) + "m " +
              (durationSec % 60UL < 10 ? String("0") : String("")) +
              String(durationSec % 60UL) + "s";
    detail += " | UID ";
    detail += lastSessionUserUid_.isEmpty() ? String("anonymous") : lastSessionUserUid_;
  } else {
    detail += "Tap START to train again.";
  }
  if (sessionLogoutPending_ && sessionSummaryStartedMs_ != 0) {
    const uint32_t elapsedMs = nowMs >= sessionSummaryStartedMs_ ? nowMs - sessionSummaryStartedMs_ : 0;
    const uint32_t remainingMs = elapsedMs >= kSessionAutoLogoutMs ? 0 : kSessionAutoLogoutMs - elapsedMs;
    lv_obj_clear_flag(summaryCountdownLabel_, LV_OBJ_FLAG_HIDDEN);
    const String countdownText = "AUTO LOGOUT " + String((remainingMs + 999UL) / 1000UL) + "s";
    if (countdownText != uiCache_.lastSummaryCountdown) {
      lv_label_set_text(summaryCountdownLabel_, countdownText.c_str());
      uiCache_.lastSummaryCountdown = countdownText;
    }
  } else {
    lv_obj_add_flag(summaryCountdownLabel_, LV_OBJ_FLAG_HIDDEN);
    uiCache_.lastSummaryCountdown = "";
  }
  if (detail != uiCache_.lastSummaryDetail) {
    lv_label_set_text(summaryDetailLabel_, detail.c_str());
    uiCache_.lastSummaryDetail = detail;
  }
}

void SmartGymTouchApp::refreshCalibrationScreen(uint32_t nowMs) {
  (void)nowMs;
  static bool sLoggedCalibrationRefreshSkipped = false;
  if (currentUiScreen_ != UiScreenMode::Calibration || calibrationScreen_ == nullptr ||
      calibrationStepLabel_ == nullptr ||
      calibrationInstructionLabel_ == nullptr || calibrationMetricLabel_ == nullptr ||
      calibrationFeedbackLabel_ == nullptr || calibrationRomBar_ == nullptr) {
    if (!sLoggedCalibrationRefreshSkipped) {
      logEvent("CAL_UI", "refresh skipped reason=not_visible", LogLevel::Normal);
      sLoggedCalibrationRefreshSkipped = true;
    }
    return;
  }
  sLoggedCalibrationRefreshSkipped = false;

  const String machineName = machineProfile_ != nullptr ? machineProfile_->displayName : String("Machine");
  const String userName = activeUser_ != nullptr ? activeUser_->displayName : String("User");
  const String machineLine = machineName + " | " + userName;
  lv_label_set_text(calibrationMachineLabel_, machineLine.c_str());

  const uint8_t romValue = static_cast<uint8_t>(constrain(buildDisplayedLiveRomPercent(), 0.0f, 100.0f));
  lv_bar_set_value(calibrationRomBar_, romValue, LV_ANIM_OFF);

  String stepTitle = "Step 1 of 5\nStart load";
  String instruction = "Set the physical pin to the suggested safe load. Use the weight buttons if needed.";
  String primary = "LOAD SET";
  String secondary = "CANCEL";
  bool showSecondary = false;
  bool enablePinButtons = true;
  String feedback = "Pin load stays physical. The recommendation is advice only.";
  float suggestedForStep = 0.0f;
  if (calibrationFlowState_ == CalibrationFlowState::AskNextSet ||
      calibrationFlowState_ == CalibrationFlowState::AnalyzeSet) {
    suggestedForStep = calibrationNextWeightKg_;
  } else if (calibrationFlowState_ == CalibrationFlowState::ConfirmStartWeight ||
             calibrationFlowState_ == CalibrationFlowState::RecommendStartWeight ||
             calibrationFlowState_ == CalibrationFlowState::Intro ||
             calibrationFlowState_ == CalibrationFlowState::SetBottomRom ||
             calibrationFlowState_ == CalibrationFlowState::SetTopRom) {
    suggestedForStep = calibrationSuggestedStartWeightKg_;
  }

  switch (calibrationFlowState_) {
    case CalibrationFlowState::ConfirmStartWeight:
    case CalibrationFlowState::RecommendStartWeight:
    case CalibrationFlowState::Intro:
      stepTitle = "Step 1 of 5\nStart load";
      instruction = "Set the pin to the suggested load.";
      primary = "LOAD SET";
      showSecondary = false;
      feedback = calibrationSuggestedStartWeightKg_ > 0.0f &&
                         fabsf(selectedWeightKg_ - calibrationSuggestedStartWeightKg_) > 0.01f
                     ? "Target: " + String(calibrationSuggestedStartWeightKg_, 1) + " kg"
                     : "Pin matches the suggested start load.";
      break;
    case CalibrationFlowState::CollectSet:
      stepTitle = "Step 2 of 5: Set " + String(max<uint8_t>(1, calibrationCurrentSetIndex_));
      instruction = "Do 3 to 5 smooth full-range reps. The app measures range and speed automatically.";
      primary = "COLLECTING";
      showSecondary = false;
      enablePinButtons = true;
      feedback = "Move smoothly through your full range.";
      break;
    case CalibrationFlowState::AnalyzeSet:
    case CalibrationFlowState::AskNextSet:
      if (calibrationFlowState_ == CalibrationFlowState::AskNextSet) {
        stepTitle = "Set next load";
        instruction = "Move the physical pin to the suggested load, then tap LOAD SET.";
        primary = "LOAD SET";
        feedback = suggestedForStep > 0.0f && fabsf(selectedWeightKg_ - suggestedForStep) > 0.01f
                       ? "Target: " + String(suggestedForStep, 1) + " kg"
                       : "Pin matches the suggested next load.";
      } else {
        stepTitle = "Step 3 of 5: Review set";
        instruction = "Review this set. You can save now or do one more set for a better estimate.";
        primary = "NEXT SET";
        enablePinButtons = false;
      }
      secondary = "SAVE NOW";
      showSecondary = true;
      if (calibrationFlowState_ == CalibrationFlowState::AnalyzeSet) {
        feedback = calibrationResultReason_;
        if (feedback.isEmpty()) {
          feedback = "Good calibration set.";
        }
      }
      break;
    case CalibrationFlowState::Result:
      stepTitle = "Step 4 of 5: Recommendations";
      instruction = "These are starting weights for future workouts. Save when ready.";
      primary = "SAVE";
      secondary = "CANCEL";
      showSecondary = false;
      enablePinButtons = false;
      feedback = "Recommended: " + String(calibrationResultRecommendedKg_, 1) +
                 " kg. Confidence: " + calibrationResultConfidence_ + ".";
      if (calibrationResultConfidence_.equalsIgnoreCase("low")) {
        feedback = "Basic calibration. Add another load later for better accuracy.";
      } else if (calibrationResultConfidence_.equalsIgnoreCase("medium")) {
        feedback = "Medium confidence. Beginner-safe recommendations.";
      }
      break;
    case CalibrationFlowState::Saving:
      stepTitle = "Step 5 of 5: Save";
      instruction = "Saving recommendations for future sessions.";
      primary = "SAVING";
      showSecondary = false;
      enablePinButtons = false;
      feedback = "Please wait.";
      break;
    case CalibrationFlowState::Saved:
      stepTitle = "Step 5 of 5: Saved";
      instruction = "Calibration saved.";
      primary = "DONE";
      showSecondary = false;
      enablePinButtons = false;
      feedback = "Recommended: " + String(calibrationResultRecommendedKg_, 1) + " kg.";
      break;
    case CalibrationFlowState::SetBottomRom:
    case CalibrationFlowState::SetTopRom:
      stepTitle = "Step 1 of 5\nStart load";
      instruction = "The app now measures range from reps. Tap LOAD SET to continue.";
      primary = "LOAD SET";
      showSecondary = false;
      feedback = "Manual bottom/top capture is skipped.";
      break;
    case CalibrationFlowState::Cancelled:
    case CalibrationFlowState::Idle:
    default:
      break;
  }

  String metrics = "Current ROM: " + String(romValue) + "%\n";
  String loadText = calibrationFlowState_ == CalibrationFlowState::CollectSet
                        ? "Set load: " + String(calibrationCurrentSetWeightKg_, 1) + " kg"
                        : "Pin load: " + String(selectedWeightKg_, 1) + " kg";
  metrics += loadText + "\n";
  if (suggestedForStep > 0.0f) {
    metrics += "Suggested: " + String(suggestedForStep, 1) + " kg\n";
  }
  metrics += "Valid reps: " + String(calibrationValidRepsInSet_) + " / " +
             String(calibrationTargetRepsPerSet_);
  if (calibrationRejectedRepsInSet_ > 0) {
    metrics += "\nRejected: " + String(calibrationRejectedRepsInSet_);
  }
  const float measuredRange = userRomTopCapturePct_ > userRomBottomCapturePct_
                                  ? userRomTopCapturePct_ - userRomBottomCapturePct_
                                  : 0.0f;
  if (measuredRange > 0.0f) {
    metrics += "\nRange: " + String(measuredRange, 0) + "%";
  } else {
    metrics += "\nRange: measuring";
  }

  lv_label_set_text(calibrationStepLabel_, stepTitle.c_str());
  lv_label_set_text(calibrationInstructionLabel_, instruction.c_str());
  String loadCardText = calibrationFlowState_ == CalibrationFlowState::CollectSet
                            ? "Set load: " + String(calibrationCurrentSetWeightKg_, 1) + " kg"
                            : "Pin load: " + String(selectedWeightKg_, 1) + " kg";
  if (suggestedForStep > 0.0f) {
    loadCardText += "\nSuggested: " + String(suggestedForStep, 1) + " kg";
  }
  if (calibrationLoadLabel_ != nullptr) {
    lv_label_set_text(calibrationLoadLabel_, loadCardText.c_str());
  }
  lv_label_set_text(calibrationMetricLabel_, metrics.c_str());
  lv_label_set_text(calibrationFeedbackLabel_, feedback.c_str());
  setButtonTextIfValid(btnCalibrationPrimary_, primary.c_str());
  setButtonTextIfValid(btnCalibrationSecondary_, secondary.c_str());
  auto setPinButtonVisible = [&](lv_obj_t* button) {
    if (button == nullptr) {
      return;
    }
    if (enablePinButtons) {
      lv_obj_clear_flag(button, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
    } else {
      lv_obj_add_flag(button, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(button, LV_OBJ_FLAG_CLICKABLE);
    }
  };
  setPinButtonVisible(btnCalibrationWeightMinusLarge_);
  setPinButtonVisible(btnCalibrationWeightMinus_);
  setPinButtonVisible(btnCalibrationWeightPlus_);
  setPinButtonVisible(btnCalibrationWeightPlusLarge_);
  if (enablePinButtons != calibrationPinButtonsEnabled_) {
    calibrationPinButtonsEnabled_ = enablePinButtons;
    logEvent("CAL_UI",
             String("pin buttons enabled=") + String(enablePinButtons ? 1 : 0) +
                 " reason=" + (enablePinButtons ? String("set_load") : String("recommendation_screen")),
             LogLevel::Normal);
  }
  if (showSecondary) {
    lv_obj_set_size(btnCalibrationPrimary_, 148, 46);
    lv_obj_set_pos(btnCalibrationPrimary_, 18, 304);
    lv_obj_set_size(btnCalibrationSecondary_, 148, 46);
    lv_obj_set_pos(btnCalibrationSecondary_, 178, 304);
    lv_obj_clear_flag(btnCalibrationSecondary_, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_set_size(btnCalibrationPrimary_, 316, 46);
    lv_obj_set_pos(btnCalibrationPrimary_, 18, 304);
    lv_obj_add_flag(btnCalibrationSecondary_, LV_OBJ_FLAG_HIDDEN);
  }

  const String logKey = stepTitle + "|" + instruction;
  if (logKey != uiCache_.lastTargetPace) {
    uiCache_.lastTargetPace = logKey;
    logEvent("CAL_UI", String("step=") + stepTitle + " text=" + instruction, LogLevel::Normal);
  }
  const String loadUiKey = stepTitle + "|" + String(selectedWeightKg_, 1) + "|" +
                           String(suggestedForStep, 1) + "|" +
                           String(calibrationCurrentSetWeightKg_, 1);
  if (loadUiKey != lastCalibrationLoadUiLogKey_) {
    lastCalibrationLoadUiLogKey_ = loadUiKey;
    logEvent("CAL_LOAD",
             String("ui step=") + stepTitle +
                 " pin=" + String(selectedWeightKg_, 1) +
                 " suggested=" + String(suggestedForStep, 1) +
                 " activeSet=" + String(calibrationCurrentSetWeightKg_, 1),
             LogLevel::Normal);
  }
  if (calibrationFlowState_ == CalibrationFlowState::CollectSet) {
    const String liveLoadKey = String(calibrationCurrentSetIndex_) + ":" +
                               String(calibrationCurrentSetWeightKg_, 1);
    if (liveLoadKey != lastCalibrationLiveLoadLogKey_) {
      lastCalibrationLiveLoadLogKey_ = liveLoadKey;
      logEvent("CAL_UI",
               String("live set load kg=") + String(calibrationCurrentSetWeightKg_, 1) +
                   " source=confirmed_pin",
               LogLevel::Normal);
    }
  }
}

void SmartGymTouchApp::refreshRestOverlay(uint32_t nowMs) {
  if (restOverlay_ == nullptr || restPanel_ == nullptr || restArc_ == nullptr ||
      restCountdownLabel_ == nullptr || restTitleLabel_ == nullptr || restDetailLabel_ == nullptr) {
    return;
  }

  uint32_t remainingMs = 0;
  const bool restActive = isRestCountdownActive(nowMs, &remainingMs);
  if (restActive != uiCache_.lastRestVisible) {
    uiCache_.lastRestVisible = restActive;
    if (!restActive) {
      lv_obj_add_flag(restOverlay_, LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (restActive) {
    // Never fall into idle while between-set rest is running.
    idleOverlaySuppressedUntilMs_ = nowMs + kIdleDuringRestSuppressMs;
    if (currentUiScreen_ != UiScreenMode::Main) {
      return;
    }
    lv_obj_clear_flag(restOverlay_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(restOverlay_);

    const uint32_t remainingSeconds = (remainingMs + 999UL) / 1000UL;
    const uint8_t progress =
        activeSessionRestSeconds_ == 0 ? 0 : static_cast<uint8_t>((remainingMs * 100UL) /
                                                                  (static_cast<uint32_t>(activeSessionRestSeconds_) * 1000UL));
    lv_arc_set_value(restArc_, static_cast<int16_t>(100 - progress));
    lv_label_set_text(restCountdownLabel_, String(remainingSeconds).c_str());

    const String detail = "Next set " + String(completedSets_ + 1) + "/" + String(activeSessionTargetSets_) +
                          "\nBreathe and reset grip";
    lv_label_set_text(restDetailLabel_, detail.c_str());
    lv_label_set_text(restTitleLabel_, remainingSeconds <= 3 ? "GO TIME" : "RECOVERY");
    lv_obj_set_style_text_color(restTitleLabel_,
                                remainingSeconds <= 3 ? lv_color_hex(0xFFD36C) : lv_color_hex(0x8FD9D2),
                                0);
  } else {
    return;
  }
}

void SmartGymTouchApp::refreshCalibrationGateScreen(uint32_t nowMs) {
  (void)nowMs;
  if (calibrationGateScreen_ == nullptr || calibrationGatePanel_ == nullptr) {
    return;
  }

  const bool shouldShowGate = currentUiScreen_ == UiScreenMode::CalibrationGate;
  if (shouldShowGate != uiCache_.lastCalibrationGateVisible) {
    uiCache_.lastCalibrationGateVisible = shouldShowGate;
    if (shouldShowGate) {
      showUiScreen(UiScreenMode::CalibrationGate);
    } else if (currentUiScreen_ == UiScreenMode::CalibrationGate) {
      showUiScreen(UiScreenMode::Main);
    }
  }

  if (shouldShowGate) {
    const String machineLine = machineProfile_ != nullptr ? buildMachineText() : String("Machine not selected");
    lv_label_set_text(calibrationGateMachineLabel_, machineLine.c_str());

    String body;
    if (activeUser_ != nullptr) {
      body = "No saved calibration exists for this machine and user yet. "
             "Run the guided calibration flow or skip to start training now.";
    } else {
      body = "No saved calibration exists for this machine yet. "
             "Skip to train immediately, or calibrate first if needed.";
    }
    lv_label_set_text(calibrationGateBodyLabel_, body.c_str());

    String hint = "Calibrate now to improve weight suggestions and ROM tracking.";
    if (activeUser_ == nullptr && !anonymousMode_) {
      hint = "Load a user first, or skip to continue in anonymous mode.";
    }
    lv_label_set_text(calibrationGateHintLabel_, hint.c_str());
  } else {
    return;
  }
}

void SmartGymTouchApp::refreshTopBar() {
  String topState = String(stateToText());
  if (state_ == State::Idle && activeUser_ == nullptr && !anonymousMode_) {
    topState = "RFID REQUIRED";
  }
  if (topState != uiCache_.lastTopState) {
    lv_label_set_text(topStateLabel_, topState.c_str());
    uiCache_.lastTopState = topState;
    lv_color_t topColor = lv_color_hex(0x1ED3A6);
    if (topState == "RFID REQUIRED") {
      topColor = lv_color_hex(0xE7A94B);
    }
    lv_obj_set_style_text_color(topStateLabel_, topColor, 0);
  }

  const String machine = buildMachineText();
  if (machine != uiCache_.lastMachine) {
    lv_label_set_text(topMachineLabel_, machine.c_str());
    uiCache_.lastMachine = machine;
  }

  String topLegend = "USER: NONE";
  if (anonymousMode_) {
    topLegend = "USER: ANONYMOUS";
  } else if (activeUser_ != nullptr) {
    const String name = activeUser_->displayName.isEmpty() ? String("CARD USER") : activeUser_->displayName;
    topLegend = "USER: " + name;
  }
  if (topLegend != uiCache_.lastUser) {
    lv_label_set_text(topUserLabel_, topLegend.c_str());
    uiCache_.lastUser = topLegend;
  }

  const String clock = buildTimeText();
  if (clock != uiCache_.lastClock) {
    const int sep = clock.indexOf('?');
    const String datePart = sep >= 0 ? clock.substring(0, sep) : clock;
    const String timePart = sep >= 0 ? clock.substring(sep + 1) : String("");
    lv_label_set_text(topDateLabel_, datePart.c_str());
    lv_label_set_text(topTimeLabel_, timePart.isEmpty() ? "--:--" : timePart.c_str());
    uiCache_.lastClock = clock;
  }

  if (topSyncLabel_ != nullptr) {
    const uint16_t pending = localPersistenceStore_.getPendingUploadCount();
    const uint16_t pendingCompactJobs = pendingSessionQueueEnqueue_ ? 1U : 0U;
    const uint16_t pendingTotal = static_cast<uint16_t>(pending + pendingCompactJobs);
    const bool syncing = cloudEnabled_ &&
                         (pendingTotal > 0 || pendingScanReconcile_ || forceCloudSyncAfterFinish_);
    String cue;
    if (syncing) {
      const uint8_t phase = static_cast<uint8_t>((millis() / 350UL) % 4UL);
      cue = "SYNC";
      for (uint8_t i = 0; i < phase; ++i) {
        cue += ".";
      }
      cue += " ";
      cue += String(pendingTotal);
    }
    if (cue != uiCache_.lastSyncCue) {
      lv_label_set_text(topSyncLabel_, cue.c_str());
      uiCache_.lastSyncCue = cue;
    }
  }
}

void SmartGymTouchApp::refreshMainStats(uint32_t nowMs) {
  const float shownLoadKg = selectedWeightKg_;
  const String weight = String(shownLoadKg, 1) + " kg";
  if (weight != uiCache_.lastBigWeight) {
    lv_label_set_text(bigWeightLabel_, weight.c_str());
    uiCache_.lastBigWeight = weight;
  }

  const float recommendedKg = resolvedRecommendation_.hasRecommendation ? resolvedRecommendation_.kg : 0.0f;
  const String recommendationSource = resolvedRecommendation_.source;
  String recommendedText;
  if (recommendedKg > 0.0f) {
    recommendedText = String(recommendedKg, 1) + " kg";
    if (recommendationSource.equalsIgnoreCase("session_summary")) {
      recommendedText += "  Last";
    } else if (recommendationSource.equalsIgnoreCase("calibration") || recommendationSource.isEmpty()) {
      recommendedText += "  Cal";
    } else {
      recommendedText += "  " + recommendationSource;
    }
  } else {
    recommendedText = String("None");
  }

  const String pathStatus = buildPathStatusText(nowMs);
  if (pathStatus != uiCache_.lastQuality) {
    lv_label_set_text(qualityLabel_, pathStatus.c_str());
    uiCache_.lastQuality = pathStatus;
    lv_color_t cueBg = lv_color_hex(0x0F191E);
    lv_color_t cueFg = lv_color_hex(0x86D9C9);
    lv_color_t cueBorder = lv_color_hex(0x26343D);
    if (pathStatus == "REST") {
      cueBg = lv_color_hex(0x101A21);
      cueFg = lv_color_hex(0x90B6C9);
      cueBorder = lv_color_hex(0x2E4553);
    } else if (pathStatus == "IN PATH") {
      cueBg = lv_color_hex(0x0E1B17);
      cueFg = lv_color_hex(0x1DAA93);
      cueBorder = lv_color_hex(0x1D9DAA);
    } else if (pathStatus == "CALIBRATION") {
      cueBg = lv_color_hex(0x102026);
      cueFg = lv_color_hex(0x86D9C9);
      cueBorder = lv_color_hex(0x1D9DAA);
    } else if (pathStatus == "OFF PATH") {
      cueBg = lv_color_hex(0x231015);
      cueFg = lv_color_hex(0xE36B6B);
      cueBorder = lv_color_hex(0xC84D4D);
    } else if (pathStatus.indexOf("SLOW") >= 0 || pathStatus.indexOf("SETTLE") >= 0) {
      cueBg = lv_color_hex(0x231A10);
      cueFg = lv_color_hex(0xE7A94B);
      cueBorder = lv_color_hex(0xD28D2A);
    }
    lv_obj_set_style_bg_color(qualityLabel_, cueBg, 0);
    lv_obj_set_style_text_color(qualityLabel_, cueFg, 0);
    lv_obj_set_style_border_color(qualityLabel_, cueBorder, 0);
  }

  const bool chartVisible = currentUiScreen_ == UiScreenMode::Main;
  const bool chartExpanded = currentUiScreen_ == UiScreenMode::Main && state_ == State::Training;
  if (chartVisible != uiCache_.lastChartVisible || chartExpanded != uiCache_.lastChartExpanded) {
    uiCache_.lastChartVisible = chartVisible;
    uiCache_.lastChartExpanded = chartExpanded;
    if (chartVisible) {
      lv_obj_clear_flag(metricsChart_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(chartActualLegend_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(chartIdealLegend_, LV_OBJ_FLAG_HIDDEN);
      if (motionLiveDot_ != nullptr) {
        lv_obj_clear_flag(motionLiveDot_, LV_OBJ_FLAG_HIDDEN);
      }
      if (motionLiveHalo_ != nullptr) {
        lv_obj_clear_flag(motionLiveHalo_, LV_OBJ_FLAG_HIDDEN);
      }
    } else {
      lv_obj_add_flag(metricsChart_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(chartActualLegend_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(chartIdealLegend_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(motionLiveDot_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(motionLiveHalo_, LV_OBJ_FLAG_HIDDEN);
    }
  }

  if (calibrationBar_ != nullptr) {
    if (state_ == State::Calibration) {
      lv_obj_clear_flag(calibrationBar_, LV_OBJ_FLAG_HIDDEN);
      uint8_t calStepNumber = 1;
      switch (calibrationFlowState_) {
        case CalibrationFlowState::SetBottomRom:
        case CalibrationFlowState::Intro:
          calStepNumber = 1;
          break;
        case CalibrationFlowState::SetTopRom:
          calStepNumber = 2;
          break;
        case CalibrationFlowState::RecommendStartWeight:
        case CalibrationFlowState::ConfirmStartWeight:
          calStepNumber = 3;
          break;
        case CalibrationFlowState::CollectSet:
        case CalibrationFlowState::AnalyzeSet:
        case CalibrationFlowState::AskNextSet:
          calStepNumber = 4;
          break;
        case CalibrationFlowState::Result:
        case CalibrationFlowState::Saving:
        case CalibrationFlowState::Saved:
        case CalibrationFlowState::Cancelled:
        case CalibrationFlowState::Idle:
        default:
          calStepNumber = 5;
          break;
      }
      lv_bar_set_range(calibrationBar_, 0, 5);
      lv_bar_set_value(calibrationBar_, calStepNumber, LV_ANIM_OFF);
    } else {
      lv_bar_set_range(calibrationBar_, 0, CalibrationService::kTargetReps);
      lv_obj_add_flag(calibrationBar_, LV_OBJ_FLAG_HIDDEN);
    }
  }

  lv_obj_set_style_shadow_width(qualityLabel_, 0, 0);
  lv_obj_set_style_shadow_spread(qualityLabel_, 0, 0);
  lv_obj_set_style_shadow_opa(qualityLabel_, LV_OPA_TRANSP, 0);

  float riseSec = static_cast<float>(sessionTplRiseMs_) / 1000.0f;
  float lowerSec = static_cast<float>(sessionTplLowerMs_) / 1000.0f;
  float topPauseSec = static_cast<float>(sessionTplTopPauseMs_) / 1000.0f;
  float bottomPauseSec = static_cast<float>(sessionTplBottomPauseMs_) / 1000.0f;
  if (!sessionMotionTemplateLatched_) {
    MotionTargetConfig timingCfg;
    const TrainingGoal timingGoal = normalizeGoalId(
        activeUser_ != nullptr ? activeUser_->goal : TrainingGoal::Hypertrophy,
        "ui_timing_text",
        isDebugGoalSelectionEnabled());
    if (deriveMotionTargetsForActiveMachine(timingCfg, timingGoal, isDebugGoalSelectionEnabled(), "ui_timing_text")) {
      riseSec = timingCfg.riseTimeSecDefault;
      lowerSec = timingCfg.lowerTimeSecDefault;
      topPauseSec = timingCfg.topPauseSec;
      bottomPauseSec = timingCfg.bottomPauseSec;
    }
  }
  if ((riseSec != 0.0f || lowerSec != 0.0f || topPauseSec != 0.0f || bottomPauseSec != 0.0f) &&
      uiCache_.lastPaceTile.isEmpty()) {
    logEvent("GRAPH",
             String("ui timing text rise=") + String(riseSec, 2) +
                 " lower=" + String(lowerSec, 2) +
                 " topPause=" + String(topPauseSec, 2) +
                 " bottomPause=" + String(bottomPauseSec, 2),
             LogLevel::Normal);
    uiCache_.lastPaceTile = "logged";
  }
  if (uiCache_.lastHistory != "__hidden__") {
    lv_label_set_text(historyMiniLabel_, "");
    uiCache_.lastHistory = "__hidden__";
  }
  if (uiCache_.lastRepPhase != "__hidden__") {
    lv_label_set_text(speedCueLabel_, "");
    uiCache_.lastRepPhase = "__hidden__";
  }
  lv_obj_add_flag(historyMiniLabel_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(speedCueLabel_, LV_OBJ_FLAG_HIDDEN);

  if (state_ == State::Calibration) {
    String stepLabel = "Step 1 of 5: Set bottom";
    String stepInstruction = "Move to the bottom position, then tap Set Bottom.";
    switch (calibrationFlowState_) {
      case CalibrationFlowState::SetTopRom:
        stepLabel = "Step 2 of 5: Set top";
        stepInstruction = "Move to the top position, then tap Set Top.";
        break;
      case CalibrationFlowState::RecommendStartWeight:
      case CalibrationFlowState::ConfirmStartWeight:
        stepLabel = "Step 3 of 5: Set weight";
        stepInstruction = "Set the machine to " + String(selectedWeightKg_, 1) +
                          " kg for calibration. Tap Weight set or Use different weight.";
        break;
      case CalibrationFlowState::CollectSet:
      case CalibrationFlowState::AnalyzeSet:
      case CalibrationFlowState::AskNextSet:
        stepLabel = "Step 4 of 5: Do reps";
        stepInstruction = "Perform 3 to 5 smooth reps. Follow the timing guide.";
        break;
      case CalibrationFlowState::Result:
      case CalibrationFlowState::Saving:
      case CalibrationFlowState::Saved:
        stepLabel = "Step 5 of 5: Save result";
        stepInstruction = "Recommended working load: " +
                          String(calibrationResultRecommendedKg_ > 0.0f ? calibrationResultRecommendedKg_ : selectedWeightKg_, 1) +
                          " kg. Confidence: " + calibrationResultConfidence_ + ".";
        break;
      case CalibrationFlowState::SetBottomRom:
      case CalibrationFlowState::Intro:
      default:
        break;
    }
    if (stepLabel != uiCache_.lastTargetPace) {
      lv_label_set_text(suggestionLabel_, stepLabel.c_str());
      uiCache_.lastTargetPace = stepLabel;
      logEvent("CAL_UI", String("step=") + stepLabel + " text=" + stepInstruction, LogLevel::Normal);
    }
    if (stepInstruction != uiCache_.lastHistory) {
      lv_obj_clear_flag(historyMiniLabel_, LV_OBJ_FLAG_HIDDEN);
      lv_label_set_text(historyMiniLabel_, stepInstruction.c_str());
      uiCache_.lastHistory = stepInstruction;
    }
    const String calMeta =
        "Current ROM: " + String(buildDisplayedLiveRomPercent(), 0) + "%\n" +
        "Valid reps: " + String(calibrationValidRepsInSet_) + " / " + String(calibrationTargetRepsPerSet_) +
        (calibrationRejectedRepsInSet_ > 0 ? (" | Rejected: " + String(calibrationRejectedRepsInSet_)) : "");
    if (calMeta != uiCache_.lastSessionMeta) {
      lv_obj_clear_flag(sessionMetaLabel_, LV_OBJ_FLAG_HIDDEN);
      lv_label_set_text(sessionMetaLabel_, calMeta.c_str());
      uiCache_.lastSessionMeta = calMeta;
      logEvent("CAL_UI",
               String("reps valid=") + String(calibrationValidRepsInSet_) +
                   " rejected=" + String(calibrationRejectedRepsInSet_),
               LogLevel::Normal);
    }
    String feedback = "Good rep";
    String statusLower = statusMessage_;
    statusLower.toLowerCase();
    if (statusLower.indexOf("short rom") >= 0 || statusLower.indexOf("low rom") >= 0) {
      feedback = "Use full ROM";
    } else if (statusLower.indexOf("eccentric") >= 0 || statusLower.indexOf("too fast lower") >= 0) {
      feedback = "Slow the lowering";
    } else if (statusLower.indexOf("pause") >= 0) {
      feedback = "Pause briefly";
    } else if (statusLower.indexOf("noisy") >= 0 || statusLower.indexOf("rejected") >= 0) {
      feedback = "Motion noisy, repeat";
    }
    if (feedback != uiCache_.lastSuggestion) {
      lv_obj_clear_flag(speedCueLabel_, LV_OBJ_FLAG_HIDDEN);
      lv_label_set_text(speedCueLabel_, feedback.c_str());
      uiCache_.lastRepPhase = feedback;
      uiCache_.lastSuggestion = feedback;
      logEvent("CAL_UI", String("feedback=") + feedback, LogLevel::Normal);
    }
  } else {
    lv_obj_add_flag(speedCueLabel_, LV_OBJ_FLAG_HIDDEN);
    if (recommendedText != uiCache_.lastTargetPace) {
      lv_label_set_text(suggestionLabel_, recommendedText.c_str());
      uiCache_.lastTargetPace = recommendedText;
      if (recommendedKg > 0.0f) {
        logEvent("UI",
                 String("recommendation label kg=") + String(recommendedKg, 1) +
                     " source=" + (recommendationSource.isEmpty() ? String("calibration") : recommendationSource),
                 LogLevel::Normal);
      } else {
        logEvent("UI",
                 (activeUser_ != nullptr || anonymousMode_)
                     ? "recommendation label empty reason=no_recommendation"
                     : "recommendation label empty reason=no_user",
                 LogLevel::Normal);
      }
    }
    const String loadMeta = "";
    lv_obj_add_flag(historyMiniLabel_, LV_OBJ_FLAG_HIDDEN);
    if (loadMeta != uiCache_.lastSessionMeta) {
      if (loadMeta.isEmpty()) {
        lv_label_set_text(sessionMetaLabel_, "");
        lv_obj_add_flag(sessionMetaLabel_, LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_clear_flag(sessionMetaLabel_, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(sessionMetaLabel_, loadMeta.c_str());
      }
      uiCache_.lastSessionMeta = loadMeta;
    }
  }

  const String sessionTimer = buildSessionTimerText(nowMs);
  if (sessionTimer != uiCache_.lastSessionTimer) {
    const bool timerVisible = sessionTimer != "--:--";
    lv_label_set_text(sessionTimerLabel_, timerVisible ? sessionTimer.c_str() : "");
    uiCache_.lastSessionTimer = sessionTimer;
  }

  const String rep = String(currentSetRepCount_) + " / " + String(getTargetReps());
  if (rep != uiCache_.lastRep) {
    lv_label_set_text(repMainLabel_, rep.c_str());
    uiCache_.lastRep = rep;
  }

  const uint8_t currentSetDisplay = completedSets_ + (state_ == State::Training ? 1 : 0);
  const String setText = String(currentSetDisplay) + " / " + String(getTargetSets());
  if (setText != uiCache_.lastSet) {
    lv_label_set_text(setMainLabel_, setText.c_str());
    uiCache_.lastSet = setText;
  }

  const float displayedRomNow = buildDisplayedLiveRomPercent();
  float romTargetPct = machineProfile_ != nullptr ? constrain(machineProfile_->idealRomPercent, 60.0f, 100.0f) : 90.0f;
  if (activeCalibration_ != nullptr && activeCalibration_->hasCalibration &&
      activeCalibration_->userRomPercent > 1.0f) {
    romTargetPct = constrain(activeCalibration_->userRomPercent, 40.0f, 100.0f);
  }
  float userBottomPct = 0.0f;
  float userTopPct = romTargetPct;
  bool hasDirectionalUserRom = false;
  if (activeCalibration_ != nullptr && activeCalibration_->hasCalibration) {
    const float capturedBottom = constrain(activeCalibration_->userBottomPct, 0.0f, 100.0f);
    const float capturedTop = constrain(activeCalibration_->userTopPct, 0.0f, 100.0f);
    const float capturedSpan = fabsf(capturedTop - capturedBottom);
    if (capturedSpan >= 5.0f) {
      userBottomPct = capturedBottom;
      userTopPct = capturedTop;
      romTargetPct = capturedSpan;
      hasDirectionalUserRom = true;
    }
  }
  float romProgressPct = 0.0f;
  if (hasDirectionalUserRom && userTopPct < userBottomPct) {
    romProgressPct = ((userBottomPct - displayedRomNow) / max(1.0f, romTargetPct)) * 100.0f;
  } else {
    romProgressPct = ((displayedRomNow - userBottomPct) / max(1.0f, romTargetPct)) * 100.0f;
  }
  romProgressPct = constrain(romProgressPct, 0.0f, 100.0f);

  const String rom = "ROM " + String(displayedRomNow, 0) + "%";
  if (rom != uiCache_.lastRom) {
    lv_label_set_text(romMainLabel_, rom.c_str());
    uiCache_.lastRom = rom;
  }

  const String velocity = "V " + String(simulatedVelocityPctPerSec_, 0);
  if (velocity != uiCache_.lastVelocity) {
    lv_label_set_text(velocityMainLabel_, velocity.c_str());
    uiCache_.lastVelocity = velocity;
  }

  lv_bar_set_value(romBar_, static_cast<int16_t>(romProgressPct), LV_ANIM_ON);
  const uint8_t targetReps = getTargetReps();
  const uint16_t setProgress = targetReps == 0 ? 0 : static_cast<uint16_t>((currentSetRepCount_ * 100U) / targetReps);
  lv_bar_set_value(setProgressBar_, static_cast<int16_t>(setProgress), LV_ANIM_ON);
  if (state_ == State::Training && activeSessionTargetSets_ > 0) {
    const uint16_t progress = static_cast<uint16_t>((completedSets_ * 100U) / activeSessionTargetSets_);
    lv_bar_set_value(calibrationBar_, static_cast<int16_t>(progress), LV_ANIM_OFF);
  } else if (state_ == State::Summary) {
    lv_bar_set_value(calibrationBar_, 100, LV_ANIM_OFF);
  }

  if (machineProfile_ != nullptr) {
    for (uint8_t i = 0; i < kMachineCount; ++i) {
      if (String(kMachineIds[i]).equalsIgnoreCase(machineProfile_->machineId)) {
        if (uiCache_.lastDropdownIndex != i) {
          if (machineDropdown_ != nullptr) {
            lv_dropdown_set_selected(machineDropdown_, i);
          }
          uiCache_.lastDropdownIndex = i;
        }
        break;
      }
    }
  }
}

void SmartGymTouchApp::refreshStatusArea(uint32_t nowMs) {
  const String status = buildStatusText();
  if (status != uiCache_.lastStatus) {
    lv_label_set_text(statusMainLabel_, status.c_str());
    uiCache_.lastStatus = status;
  }
  if (statusMainLabel_ != nullptr) {
    lv_obj_clear_flag(statusMainLabel_, LV_OBJ_FLAG_HIDDEN);
    String statusLower = status;
    statusLower.toLowerCase();
    const bool isWarning =
        statusLower.indexOf("warning") >= 0 ||
        statusLower.indexOf("rejected") >= 0 ||
        statusLower.indexOf("not counted") >= 0 ||
        statusLower.indexOf("invalid") >= 0 ||
        statusLower.indexOf("ignored") >= 0 ||
        statusLower.indexOf("error") >= 0 ||
        statusLower.indexOf("fail") >= 0;
    if (isWarning) {
      lv_obj_set_style_bg_opa(statusMainLabel_, LV_OPA_COVER, 0);
      lv_obj_set_style_bg_color(statusMainLabel_, lv_color_hex(0x3A1612), 0);
      lv_obj_set_style_border_width(statusMainLabel_, 1, 0);
      lv_obj_set_style_border_color(statusMainLabel_, lv_color_hex(0xD06A4E), 0);
      lv_obj_set_style_pad_hor(statusMainLabel_, 10, 0);
      lv_obj_set_style_pad_ver(statusMainLabel_, 5, 0);
      lv_obj_set_style_text_color(statusMainLabel_, lv_color_hex(0xFFD8C9), 0);
    } else {
      lv_obj_set_style_bg_opa(statusMainLabel_, LV_OPA_COVER, 0);
      lv_obj_set_style_bg_color(statusMainLabel_, lv_color_hex(0x0A1519), 0);
      lv_obj_set_style_border_width(statusMainLabel_, 1, 0);
      lv_obj_set_style_border_color(statusMainLabel_, lv_color_hex(0x17262D), 0);
      lv_obj_set_style_pad_hor(statusMainLabel_, 10, 0);
      lv_obj_set_style_pad_ver(statusMainLabel_, 5, 0);
      lv_obj_set_style_text_color(statusMainLabel_, lv_color_hex(0xC8D7DC), 0);
    }
  }

  (void)nowMs;
}

void SmartGymTouchApp::refreshDebugPanel(uint32_t nowMs) {
  if (currentUiScreen_ != UiScreenMode::Debug) {
    return;
  }
  if (debugStatusLabel_ == nullptr || debugHardwareLabel_ == nullptr) {
    return;
  }
  const String debugStatus = buildDebugStatusText();
  if (debugStatus != uiCache_.lastDebugStatus) {
    lv_label_set_text(debugStatusLabel_, debugStatus.c_str());
    uiCache_.lastDebugStatus = debugStatus;
  }
  const String hardwareStatus = buildHardwareStatusText();
  if (hardwareStatus != uiCache_.lastDebugStatusHardware) {
    lv_label_set_text(debugHardwareLabel_, hardwareStatus.c_str());
    uiCache_.lastDebugStatusHardware = hardwareStatus;
  }

  if (btnAutoRep_ != nullptr) {
    lv_obj_t* autoRepLabel = lv_obj_get_child(btnAutoRep_, 0);
    if (autoRepLabel != nullptr) {
      lv_label_set_text(autoRepLabel, autoMotionEnabled_ ? "AUTO ON" : "AUTO REP");
    }
    if (autoMotionEnabled_) {
      lv_obj_set_style_bg_opa(btnAutoRep_, LV_OPA_COVER, 0);
      lv_obj_set_style_bg_color(btnAutoRep_, lv_color_hex(0x10332E), 0);
      lv_obj_set_style_border_color(btnAutoRep_, lv_color_hex(0x1DAA93), 0);
      lv_obj_set_style_text_color(btnAutoRep_, lv_color_hex(0x86E7F0), 0);
    } else {
      lv_obj_set_style_bg_opa(btnAutoRep_, LV_OPA_TRANSP, 0);
      lv_obj_set_style_border_color(btnAutoRep_, lv_color_hex(0x2C3A43), 0);
      lv_obj_set_style_text_color(btnAutoRep_, lv_color_hex(0x9FB2BA), 0);
    }
  }
  if (btnLogLevel_ != nullptr) {
    lv_obj_t* logLabel = lv_obj_get_child(btnLogLevel_, 0);
    if (logLabel != nullptr) {
      lv_label_set_text(logLabel, (String("LOG:") + logLevelText()).c_str());
    }
  }
  (void)nowMs;
}

void SmartGymTouchApp::refreshProfileScreen(uint32_t nowMs) {
  (void)nowMs;
  if (currentUiScreen_ != UiScreenMode::Profile || profilePanel_ == nullptr || activeUser_ == nullptr) {
    return;
  }
  const String title = profileCreateMode_ ? "CREATE YOUR PROFILE" : "EDIT PROFILE";
  lv_label_set_text(profileTitleLabel_, title.c_str());
  lv_label_set_text(profileUidLabel_, ("Card UID: " + activeUser_->rfidUid).c_str());
  lv_label_set_text(profileNameValueLabel_, profileEditName_.c_str());
  lv_label_set_text(profileAgeValueLabel_, String(profileEditAge_).c_str());
  lv_label_set_text(profileWeightValueLabel_, String(profileEditWeightKg_, 1).c_str());
  lv_label_set_text(profileHeightValueLabel_, String(profileEditHeightCm_, 0).c_str());
  lv_label_set_text(profileGoalValueLabel_, goalDisplayName(profileEditGoal_));
  lv_label_set_text(profileGenderValueLabel_, genderDisplayName(profileEditGender_));
}

void SmartGymTouchApp::resetMotionGraph() {
  motionHistoryCount_ = 0;
  motionHistoryHead_ = 0;
  idealPhaseMs_ = 0;
  idealPhaseLastTickMs_ = 0;
  idealBottomStillSinceMs_ = 0;
  memset(motionActualHistory_, 0, sizeof(motionActualHistory_));
  memset(motionIdealHistory_, 0, sizeof(motionIdealHistory_));
  memset(motionHistoryTimeMs_, 0, sizeof(motionHistoryTimeMs_));
  chartActualFiltered_ = 0.0f;
  lastMotionDotX_ = -32768;
  lastMotionDotY_ = -32768;
  lastMotionDotOnPath_ = false;
  perfLastChartMs_ = 0;
  if (metricsChart_ != nullptr) {
    lv_chart_refresh(metricsChart_);
  }
}

void SmartGymTouchApp::appendMotionGraphSample(float actualPercent, float idealPercent, uint32_t nowMs) {
  const float actualClamped = constrain(actualPercent, 0.0f, 100.0f);
  if (motionHistoryCount_ == 0) {
    chartActualFiltered_ = actualClamped;
  } else {
    chartActualFiltered_ += (actualClamped - chartActualFiltered_) * 0.72f;
  }
  motionActualHistory_[motionHistoryHead_] = chartActualFiltered_;
  motionIdealHistory_[motionHistoryHead_] = idealPercent;
  motionHistoryTimeMs_[motionHistoryHead_] = nowMs;
  motionHistoryHead_ = static_cast<uint16_t>((motionHistoryHead_ + 1U) % kMotionHistoryPoints);
  if (motionHistoryCount_ < kMotionHistoryPoints) {
    motionHistoryCount_++;
  }

  if (metricsChart_ == nullptr || metricsSeries_ == nullptr ||
      idealSeries_ == nullptr) {
    (void)idealPercent;
    return;
  }
}

bool SmartGymTouchApp::getMotionChartPlotRect(lv_area_t& outRect) const {
  if (metricsChart_ == nullptr) {
    return false;
  }
  const lv_coord_t chartWidth = lv_obj_get_width(metricsChart_);
  const lv_coord_t chartHeight = lv_obj_get_height(metricsChart_);
  if (chartWidth <= 0 || chartHeight <= 0) {
    return false;
  }
  const lv_coord_t padLeft = lv_obj_get_style_pad_left(metricsChart_, LV_PART_MAIN);
  const lv_coord_t padRight = lv_obj_get_style_pad_right(metricsChart_, LV_PART_MAIN);
  const lv_coord_t padTop = lv_obj_get_style_pad_top(metricsChart_, LV_PART_MAIN);
  const lv_coord_t padBottom = lv_obj_get_style_pad_bottom(metricsChart_, LV_PART_MAIN);
  const lv_coord_t plotLeft = lv_obj_get_x(metricsChart_) + padLeft;
  const lv_coord_t plotTop = lv_obj_get_y(metricsChart_) + padTop;
  const lv_coord_t plotRight = lv_obj_get_x(metricsChart_) + chartWidth - padRight - 1;
  const lv_coord_t plotBottom = lv_obj_get_y(metricsChart_) + chartHeight - padBottom - 1;
  if (plotRight <= plotLeft || plotBottom <= plotTop) {
    return false;
  }
  outRect.x1 = plotLeft;
  outRect.y1 = plotTop;
  outRect.x2 = plotRight;
  outRect.y2 = plotBottom;
  return true;
}

lv_coord_t SmartGymTouchApp::mapRomPercentToChartY(float romPct) const {
  lv_area_t plotRect{};
  if (!getMotionChartPlotRect(plotRect)) {
    return 0;
  }
  const float bounded = constrain(romPct, 0.0f, 100.0f);
  const lv_coord_t plotHeight = static_cast<lv_coord_t>(max<lv_coord_t>(1, plotRect.y2 - plotRect.y1));
  return static_cast<lv_coord_t>(plotRect.y1 + ((100.0f - bounded) / 100.0f) * plotHeight);
}

void SmartGymTouchApp::renderMotionChartFrame(uint32_t nowMs) {
  if (metricsChart_ == nullptr || metricsSeries_ == nullptr || idealSeries_ == nullptr ||
      idealSeriesUpper_ == nullptr || idealSeriesLower_ == nullptr) {
    return;
  }

  const uint16_t points = static_cast<uint16_t>(lv_chart_get_point_count(metricsChart_));
  if (points < 8) {
    return;
  }
  const uint16_t center = points / 2U;
  const uint32_t spanMs = kMotionGraphWindowMs;
  const uint32_t stepMs = max<uint32_t>(1, spanMs / points);

  const uint32_t chartStart = millis();
  for (uint16_t i = 0; i < points; ++i) {
    const int32_t rel = static_cast<int32_t>(i) - static_cast<int32_t>(center);
    const int32_t offsetMs = rel * static_cast<int32_t>(stepMs);
    uint32_t sampleTs = nowMs;
    if (offsetMs < 0) {
      const uint32_t back = static_cast<uint32_t>(-offsetMs);
      sampleTs = nowMs > back ? (nowMs - back) : 0;
    } else {
      sampleTs = nowMs + static_cast<uint32_t>(offsetMs);
    }

    const float ideal = constrain(buildIdealMotionPercent(sampleTs), 0.0f, 100.0f);
    idealSeries_->y_points[i] = static_cast<lv_coord_t>(roundf(ideal));
    // Disable extra rails to avoid edge aliasing/flicker from overlapping
    // multi-color lines on RGB panels. Keep one single ideal path only.
    idealSeriesUpper_->y_points[i] = LV_CHART_POINT_NONE;
    idealSeriesLower_->y_points[i] = LV_CHART_POINT_NONE;

    // Live-dot mode: hide actual trace line entirely.
    metricsSeries_->y_points[i] = LV_CHART_POINT_NONE;
  }
  lv_chart_refresh(metricsChart_);
  perfLastChartMs_ = millis() - chartStart;
  if (perfAvgChartMsQ8_ == 0) {
    perfAvgChartMsQ8_ = perfLastChartMs_ << 8;
  } else {
    perfAvgChartMsQ8_ = ((perfAvgChartMsQ8_ * 7U) + ((perfLastChartMs_ << 8) * 1U)) / 8U;
  }
}

bool SmartGymTouchApp::isMotionOnPath(float actualPercent, float idealPercent) const {
  // Path quality follows the rendered experience: the live halo is accepted
  // when it overlaps the forgiving guide lane, not only at the centerline.
  const float actualCenter = constrain(actualPercent, 0.0f, 100.0f);
  const float idealCenter = constrain(idealPercent, 0.0f, 100.0f);
  float haloPct = 0.0f;
  if (metricsChart_ != nullptr) {
    lv_area_t plotRect{};
    const lv_coord_t h = getMotionChartPlotRect(plotRect)
                             ? static_cast<lv_coord_t>(plotRect.y2 - plotRect.y1)
                             : lv_obj_get_height(metricsChart_);
    if (h > 0) {
      // Dot halo radius is ~11 px in current UI.
      haloPct = (11.0f / static_cast<float>(h)) * 100.0f;
    }
  }
  const float acceptanceHalfBand = kMotionLaneTolerancePct + kMotionLaneHalfThicknessPct + haloPct;
  return fabsf(actualCenter - idealCenter) <= acceptanceHalfBand;
}

float SmartGymTouchApp::getActiveRomPercent(float* rawOut, const char** sourceOut) const {
  const float sourceRaw = (!sensorSimulationEnabled_ && hardwareSensorEnabled_)
                              ? ((lastSensorReading_.romPercentInstant * 0.70f) +
                                 (lastSensorReading_.romPercent * 0.30f))
                              : liveRomPercent_;
  if (rawOut != nullptr) {
    *rawOut = constrain(sourceRaw, 0.0f, 100.0f);
  }
  if (sourceOut != nullptr) {
    *sourceOut = (state_ != State::Calibration && activeCalibration_ != nullptr &&
                  activeCalibration_->hasCalibration &&
                  activeCalibration_->userTopPct > activeCalibration_->userBottomPct + 1.0f)
                     ? "user_calibration"
                     : "machine_default";
  }
  float value = constrain(sourceRaw, 0.0f, 100.0f);
  if (state_ != State::Calibration && activeCalibration_ != nullptr &&
      activeCalibration_->hasCalibration &&
      activeCalibration_->userTopPct > activeCalibration_->userBottomPct + 1.0f) {
    const float bottom = constrain(activeCalibration_->userBottomPct, 0.0f, 98.0f);
    const float top = constrain(activeCalibration_->userTopPct, bottom + 1.0f, 100.0f);
    value = ((value - bottom) * 100.0f) / max(1.0f, top - bottom);
  }
  value = constrain(value, 0.0f, 100.0f);
  return value;
}

float SmartGymTouchApp::buildDisplayedLiveRomPercent() const {
  float raw = 0.0f;
  const char* source = nullptr;
  float value = getActiveRomPercent(&raw, &source);
  if (value <= 0.8f) value = 0.0f;
  if (value >= 99.2f) value = 100.0f;
  return value;
}

String SmartGymTouchApp::buildPathStatusText(uint32_t nowMs) {
  if (state_ == State::Idle && activeUser_ == nullptr && !anonymousMode_) {
    return "READY";
  }
  if (state_ == State::Calibration) {
    return "CALIBRATION";
  }
  if (state_ == State::Training) {
    if (currentSetRepCount_ == 0 && lastSetCompletedMs_ != 0 && activeSessionRestSeconds_ > 0) {
      return "REST";
    }
    const float ideal = buildIdealMotionPercent(nowMs);
    const float actual = buildDisplayedLiveRomPercent();
    return isMotionOnPath(actual, ideal) ? "IN PATH" : "OFF PATH";
  }
  if (state_ == State::Summary) {
    return "SESSION COMPLETE";
  }
  return "READY";
}

void SmartGymTouchApp::updateMotionLiveDot(uint32_t nowMs) {
  if (metricsChart_ == nullptr || motionLiveDot_ == nullptr || motionLiveHalo_ == nullptr ||
      lv_obj_has_flag(metricsChart_, LV_OBJ_FLAG_HIDDEN)) {
    if (motionLiveDot_ != nullptr) {
      lv_obj_add_flag(motionLiveDot_, LV_OBJ_FLAG_HIDDEN);
    }
    if (motionLiveHalo_ != nullptr) {
      lv_obj_add_flag(motionLiveHalo_, LV_OBJ_FLAG_HIDDEN);
    }
    return;
  }

  lv_area_t plotRect{};
  if (!getMotionChartPlotRect(plotRect)) {
    return;
  }

  const float dotSource = buildDisplayedLiveRomPercent();
  const float bounded = dotSource;
  const lv_coord_t plotWidth = static_cast<lv_coord_t>(max<lv_coord_t>(1, plotRect.x2 - plotRect.x1));
  const lv_coord_t liveX = static_cast<lv_coord_t>(plotRect.x1 + (plotWidth / 2));
  const lv_coord_t liveY = mapRomPercentToChartY(bounded);
  if (!graphLayoutLogged_) {
    const lv_coord_t chartWidth = lv_obj_get_width(metricsChart_);
    const lv_coord_t chartHeight = lv_obj_get_height(metricsChart_);
    const lv_coord_t padTop = lv_obj_get_style_pad_top(metricsChart_, LV_PART_MAIN);
    const lv_coord_t padBottom = lv_obj_get_style_pad_bottom(metricsChart_, LV_PART_MAIN);
    logEvent("GRAPH_LAYOUT",
             String("graph x=") + String(lv_obj_get_x(metricsChart_)) +
                 " y=" + String(lv_obj_get_y(metricsChart_)) +
                 " w=" + String(chartWidth) +
                 " h=" + String(chartHeight) +
                 " padTop=" + String(padTop) +
                 " padBottom=" + String(padBottom),
             LogLevel::Normal);
    logEvent("GRAPH_LAYOUT",
             String("romToY rom=0 y=") + String(mapRomPercentToChartY(0.0f)) +
                 " rom=100 y=" + String(mapRomPercentToChartY(100.0f)),
             LogLevel::Normal);
    logEvent("GRAPH_LAYOUT", "clippingSafe=1", LogLevel::Normal);
    graphLayoutLogged_ = true;
  }
  // Keep touch detection on the same phase/timebase as rendering center.
  const float idealAtDot = buildIdealMotionPercent(nowMs);
  const bool onPath = isMotionOnPath(dotSource, idealAtDot);

  lv_obj_clear_flag(motionLiveDot_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(motionLiveHalo_, LV_OBJ_FLAG_HIDDEN);
  if (liveX != lastMotionDotX_ || liveY != lastMotionDotY_) {
    lv_obj_set_pos(motionLiveHalo_, liveX - 11, liveY - 11);
    lv_obj_set_pos(motionLiveDot_, liveX - 5, liveY - 5);
    lastMotionDotX_ = liveX;
    lastMotionDotY_ = liveY;
  }
  if (onPath != lastMotionDotOnPath_) {
    const lv_color_t guideColor = onPath ? lv_color_hex(0x72F0D0) : lv_color_hex(0xFFB44A);
    const lv_color_t haloColor = onPath ? lv_color_hex(0x86E7F0) : lv_color_hex(0xE36B6B);
    const lv_color_t dotColor = onPath ? lv_color_hex(0xE9FFFB) : lv_color_hex(0xFFE6CC);
    if (idealSeries_ != nullptr) {
      idealSeries_->color = guideColor;
    }
    if (qualityLabel_ != nullptr) {
      lv_obj_set_style_bg_color(qualityLabel_, onPath ? lv_color_hex(0x0D2729) : lv_color_hex(0x2B1711), 0);
      lv_obj_set_style_border_color(qualityLabel_, onPath ? lv_color_hex(0x1F565A) : lv_color_hex(0xA65436), 0);
      lv_obj_set_style_text_color(qualityLabel_, onPath ? lv_color_hex(0x86E7F0) : lv_color_hex(0xFFB44A), 0);
    }
    lv_obj_set_style_bg_color(motionLiveHalo_, haloColor, 0);
    lv_obj_set_style_bg_color(motionLiveDot_, dotColor, 0);
    lastMotionDotOnPath_ = onPath;
  }
}

void SmartGymTouchApp::updateIdealGuideLine(uint32_t nowMs) {
  (void)nowMs;
}

void SmartGymTouchApp::onMotionChartDraw(lv_event_t* event) {
  (void)event;
}

void SmartGymTouchApp::updateRuntime() {
  const uint32_t now = millis();
  logStateIfChanged(now);
  logHeartbeat(now);
  if (perfLastFrameMs_ != 0) {
    const uint32_t frameMs = now - perfLastFrameMs_;
    if (frameMs > (kTickIntervalMs * 3U)) {
      perfDroppedFrames_++;
    }
    if (perfAvgFrameMsQ8_ == 0) {
      perfAvgFrameMsQ8_ = frameMs << 8;
    } else {
      perfAvgFrameMsQ8_ = ((perfAvgFrameMsQ8_ * 7U) + ((frameMs << 8) * 1U)) / 8U;
    }
  }
  perfLastFrameMs_ = now;
  perfFrameCount_++;
  const bool debugOpen = currentUiScreen_ == UiScreenMode::Debug;

  if (finishRequestPending_) {
    const bool logoutNow = finishRequestLogoutNow_;
    logEvent("BTN",
             String("finish requested source=manual logoutNow=") + (logoutNow ? "1" : "0"),
             LogLevel::Normal);
    finishRequestPending_ = false;
    finishRequestLogoutNow_ = false;
    finishTraining("Ended manually.", logoutNow);
  }
  if (autoFinishRequestPending_) {
    const String reason = autoFinishReason_.isEmpty() ? String("Workout completed.") : autoFinishReason_;
    autoFinishRequestPending_ = false;
    autoFinishReason_ = "";
    finishTraining(reason, false);
  }

  processHardwareInputs(now);
  maybeAutoLogoutAfterSession(now);

  const bool restCountdownActive = isRestCountdownActive(now, nullptr);
  const bool shouldAnimateChart =
      currentUiScreen_ == UiScreenMode::Main && !debugOpen && !restCountdownActive;
  if (shouldAnimateChart) {
    if (idealPhaseLastTickMs_ == 0) {
      idealPhaseLastTickMs_ = now;
    }
    const uint32_t dtMs = now >= idealPhaseLastTickMs_ ? (now - idealPhaseLastTickMs_) : 0;
    idealPhaseLastTickMs_ = now;
    MotionGuideTemplate tpl{};
    if ((state_ == State::Training || state_ == State::Summary) && sessionMotionTemplateLatched_) {
      tpl.riseMs = sessionTplRiseMs_;
      tpl.topPauseMs = sessionTplTopPauseMs_;
      tpl.lowerMs = sessionTplLowerMs_;
      tpl.bottomPauseMs = sessionTplBottomPauseMs_;
    } else {
      const GoalRecommendation* recommendation = getRecommendation();
      tpl = buildMotionGuideTemplate(machineProfile_, recommendation);
    }
    const uint32_t cycleMs = max<uint32_t>(1200UL, motionGuideCycleMs(tpl));
    idealPhaseMs_ = (idealPhaseMs_ + dtMs) % cycleMs;
  }

  if (shouldAnimateChart && (now - uiCache_.lastChartRefreshMs >= kChartRefreshMs)) {
    uiCache_.lastChartRefreshMs = now;
    const float idealRomPercent = buildIdealMotionPercent(now);
    appendMotionGraphSample(buildDisplayedLiveRomPercent(), idealRomPercent, now);
    renderMotionChartFrame(now);
  }

  if (currentUiScreen_ == UiScreenMode::Main) {
    updateMotionLiveDot(now);
  }

  const uint32_t refreshInterval = debugOpen ? kDebugUiRefreshMs : kSlowUiRefreshMs;
  if (now - uiCache_.lastSlowUiRefreshMs >= refreshInterval) {
    uiCache_.lastSlowUiRefreshMs = now;
    refreshUi();
  }

  const SessionStage preStage = getSessionStage(now);
  const uint32_t cloudIntervalMs =
      (preStage == SessionStage::Identify || preStage == SessionStage::Idle)
          ? kCloudServiceIntervalIdentifyMs
          : (preStage == SessionStage::Summary ? kSummaryCloudServiceIntervalMs
                                                : kCloudServiceIntervalMs);
  if (now - uiCache_.lastCloudServiceMs >= cloudIntervalMs) {
    uiCache_.lastCloudServiceMs = now;
    const bool summaryVisible =
        (state_ == State::Summary || currentUiScreen_ == UiScreenMode::Summary);
    const bool cloudTransitionQuiet = now >= cloudBlockedUntilMs_;
    const bool summarySmallSyncReady =
        summaryVisible && sessionSummaryStartedMs_ != 0 &&
        (now - sessionSummaryStartedMs_) >= kSummarySmallSyncDelayMs &&
        cloudTransitionQuiet;
    const bool summaryCloudReady = !summaryVisible || summarySmallSyncReady;
    const bool fullyIdleForHeavyUpload =
        state_ == State::Idle &&
        currentUiScreen_ == UiScreenMode::Idle &&
        activeUser_ == nullptr &&
        !anonymousMode_ &&
        (now - lastUserActivityMs_) >= kHeavyUploadIdleMs;
    (void)fullyIdleForHeavyUpload;
    const SessionStage stage = getSessionStage(now);
    const bool allowCloudBackground =
        (stage == SessionStage::Idle || stage == SessionStage::Identify ||
         stage == SessionStage::Rest || stage == SessionStage::Summary);
    const bool uiIsLiveGraphScreen =
        (currentUiScreen_ == UiScreenMode::Main) &&
        (state_ == State::Training);
    const bool forceCloudNow = forceCloudSyncAfterFinish_;
    const bool forceThisTick = forceCloudNow && !summaryVisible;
    const bool waitingForCard = (stage == SessionStage::Identify);
    const bool uiQuietForCloud =
        forceThisTick || waitingForCard || summaryVisible || userLoading_ ||
        ((now - lastUserActivityMs_) >= kCloudUiQuietAfterInputMs);
    const bool uiInteractiveSurface =
        (currentUiScreen_ == UiScreenMode::Main || currentUiScreen_ == UiScreenMode::Summary ||
         currentUiScreen_ == UiScreenMode::Profile || currentUiScreen_ == UiScreenMode::Debug);
    const bool allowCloudByUi = !uiIsLiveGraphScreen && (!uiInteractiveSurface || uiQuietForCloud);
    const bool noActiveSession =
        (state_ != State::Training && state_ != State::Calibration);
    const bool finishSyncPending =
        forceCloudSyncAfterFinish_ || pendingSessionQueueEnqueue_ || pendingSessionDetailQueueEnqueue_;
    const bool shouldLogSyncGate =
        finishSyncPending &&
        (lastSyncGateDiagLogMs_ == 0 || (now - lastSyncGateDiagLogMs_) >= kSyncDiagLogCadenceMs);
    if (shouldLogSyncGate) {
      lastSyncGateDiagLogMs_ = now;
      logEvent("SYNCGATE",
               String("pending finish sync force=") + (forceCloudSyncAfterFinish_ ? "1" : "0") +
                   " pendingEnqueue=" + (pendingSessionQueueEnqueue_ ? "1" : "0") +
                   " summaryVisible=" + (summaryVisible ? "1" : "0") +
                   " summaryReady=" + (summaryCloudReady ? "1" : "0") +
                   " allowBackground=" + (allowCloudBackground ? "1" : "0") +
                   " allowUi=" + (allowCloudByUi ? "1" : "0") +
                   " uiQuiet=" + (uiQuietForCloud ? "1" : "0") +
                   " stage=" + sessionStageToText(stage),
               LogLevel::Normal);
      if (!summaryCloudReady && summaryVisible && sessionSummaryStartedMs_ != 0) {
        const uint32_t elapsedMs = now > sessionSummaryStartedMs_ ? (now - sessionSummaryStartedMs_) : 0;
        logEvent("SYNCGATE",
                 String("skipped reason=summary_delay elapsedMs=") + String(elapsedMs) +
                     " requiredMs=" + String(kSummarySmallSyncDelayMs),
                 LogLevel::Normal);
      } else if (!allowCloudBackground) {
        logEvent("SYNCGATE", "skipped reason=stage_not_allowed", LogLevel::Normal);
      } else if (!(forceCloudSyncAfterFinish_ || syncWorkerForce_ || noActiveSession ||
                   (allowCloudByUi && uiQuietForCloud))) {
        logEvent("SYNCGATE", "skipped reason=ui_not_quiet", LogLevel::Normal);
      }
    }
    if (cloudEnabled_) {
      const bool forceRequested = syncWorkerForce_;
      const bool allowRunBySyncPressure = finishSyncPending || (pendingScanReconcile_ && userLoading_);
      if (summaryCloudReady &&
          allowCloudBackground &&
          ((forceThisTick || forceRequested) || noActiveSession || allowRunBySyncPressure ||
           (allowCloudByUi && uiQuietForCloud))) {
        if (finishSyncPending) {
          logEvent("SYNCGATE", "running reason=finish_pending", LogLevel::Normal);
        }
        if (kUseCloudSyncWorker) {
          requestCloudSync(forceThisTick || forceRequested);
        } else {
          updateCloudSync(now, forceThisTick || forceRequested);
        }
      }
      // Keep cloud fetch/merge work out of direct interactive UI handlers.
      // Summary should remain responsive while queue flush proceeds on the
      // scheduled cloud service cadence.
    } else {
      timeService_.update(false);
      if (pendingScanReconcile_ && userLoading_ && activeUser_ != nullptr) {
        refreshActiveCalibration();
        configureRepDetectorThresholds();
        logEvent("USER_SYNC", "calibration fallback reason=cloud_disabled", LogLevel::Normal);
        logEvent("USER_SYNC",
                 String("recommendation resolved kg=") +
                     String(resolvedRecommendation_.hasRecommendation ? resolvedRecommendation_.kg : 0.0f, 1) +
                     " source=" +
                     (resolvedRecommendation_.hasRecommendation ? resolvedRecommendation_.source : String("fallback")),
                 LogLevel::Normal);
        userLoading_ = false;
        hideUserLoadingPopup(activeUser_->rfidUid, "fallback");
        logEvent("USER_SYNC",
                 String("complete uid=") + activeUser_->rfidUid + " result=fallback",
                 LogLevel::Normal);
        showUiScreen(UiScreenMode::Main);
        refreshUi();
      }
      pendingScanReconcile_ = false;
      pendingScanReconcileEarliestMs_ = 0;
      pendingScanReconcileStartedMs_ = 0;
      pendingScanReconcileStep_ = 0;
    }
  }
}

void SmartGymTouchApp::processHardwareInputs(uint32_t nowMs) {
  if (hardwareRfidEnabled_) {
    const bool wasReady = rfidInitialized_;
    rfidInitialized_ = rfidService_.isReady();
    if (!wasReady && rfidInitialized_) {
      logEvent("RFID", String("recovered version=0x") + String(rfidService_.readerVersion(), HEX));
    }
    String uid;
    if (rfidService_.readCard(uid)) {
      idleStartupLock_ = false;
      markUserActivity(nowMs);
      if (state_ == State::Calibration) {
        logEvent("CAL",
                 String("interaction blocked reason=calibration_active kind=rfid uid=") + uid,
                 LogLevel::Normal);
        setStatusMessage("Calibration active. Scan after calibration is complete.");
        refreshUi();
        rfidInitialized_ = rfidService_.isReady();
        return;
      }
      const char* busyReason = nullptr;
      uint16_t busyQueueCount = 0;
      if (isSessionUploadBusy(nowMs, &busyReason, &busyQueueCount)) {
        logEvent("SYNC",
                 String("upload busy reason=") + (busyReason != nullptr ? busyReason : "unknown") +
                     " queueCount=" + String(busyQueueCount) +
                     " phase=" + String(static_cast<unsigned>(pendingWebDetailPhase_)),
                 LogLevel::Normal);
        logEvent("RFID",
                 String("scan ignored reason=upload_active uid=") + uid,
                 LogLevel::Normal);
        setStatusMessage("Syncing session. Scan again in a moment.");
        refreshUi();
        rfidInitialized_ = rfidService_.isReady();
        return;
      }
      if (currentUiScreen_ == UiScreenMode::Debug) {
        lastScannedUid_ = uid;
        setStatusMessage("RFID read in service. Exit service to load user.");
      } else {
        if (state_ == State::Training && activeUser_ != nullptr &&
            !activeUser_->rfidUid.equalsIgnoreCase(uid)) {
          openUserSwitchPrompt(uid);
          rfidInitialized_ = rfidService_.isReady();
          return;
        }
        activateUserByUid(uid, true);
      }
    }
    rfidInitialized_ = rfidService_.isReady();
  }

  if (sensorSimulationEnabled_) {
    updateAutoMotion();
    liveRomPercent_ = simulatedRomPercent_;
  } else if (hardwareSensorEnabled_ && sensorManager_.update(nowMs)) {
    lastSensorReading_ = sensorManager_.getReading();
    liveRomPercent_ = lastSensorReading_.romPercent;
    simulatedRomPercent_ = lastSensorReading_.romPercent;
    syncRomDebugSliderIfPresent();
    if (!firstSensorBootMemLogged_) {
      firstSensorBootMemLogged_ = true;
      logBootMemPoint("after_first_sensor_update");
    }
  }

  updateFromMotionInput(nowMs);
}

void SmartGymTouchApp::updateFromMotionInput(uint32_t nowMs) {
  if (lastTickMs_ == 0) {
    lastTickMs_ = nowMs;
    lastSimulatedRomPercent_ = liveRomPercent_;
    return;
  }

  const uint32_t deltaMs = nowMs > lastTickMs_ ? nowMs - lastTickMs_ : 1;
  const float motionDelta = fabsf(liveRomPercent_ - lastSimulatedRomPercent_);
  const float rawVelocityPctPerSec =
      ((liveRomPercent_ - lastSimulatedRomPercent_) * 1000.0f) / static_cast<float>(deltaMs);
  if (velocityStillLatched_) {
    if (motionDelta >= kVelocityStillDeltaExitPct ||
        fabsf(rawVelocityPctPerSec) >= kVelocityStillSpeedExitPctPerSec) {
      velocityStillLatched_ = false;
    }
  } else if (motionDelta <= kVelocityStillDeltaEnterPct &&
             fabsf(rawVelocityPctPerSec) <= kVelocityStillSpeedEnterPctPerSec) {
    velocityStillLatched_ = true;
  }

  if (velocityStillLatched_) {
    filteredVelocityPctPerSec_ = 0.0f;
    // Track idle encoder noise floor for diagnostics and calibration quality.
    if (encoderNoiseFloorPct_ <= 0.001f) {
      encoderNoiseFloorPct_ = motionDelta;
    } else {
      encoderNoiseFloorPct_ = (encoderNoiseFloorPct_ * 0.95f) + (motionDelta * 0.05f);
    }
  } else {
    // Fast smoothing to reduce encoder noise flicker while keeping motion responsive.
    filteredVelocityPctPerSec_ += (rawVelocityPctPerSec - filteredVelocityPctPerSec_) * 0.40f;
    if (fabsf(filteredVelocityPctPerSec_) < kVelocityDisplayDeadbandPctPerSec) {
      filteredVelocityPctPerSec_ = 0.0f;
    }
  }
  simulatedVelocityPctPerSec_ = filteredVelocityPctPerSec_;
  lastTickMs_ = nowMs;
  lastSimulatedRomPercent_ = liveRomPercent_;

  const bool realMotion = motionDelta >= 3.5f || fabsf(rawVelocityPctPerSec) >= 8.5f;
  const bool waitingForCard = (state_ == State::Idle && activeUser_ == nullptr && !anonymousMode_);
  if (currentUiScreen_ == UiScreenMode::Idle) {
    if (!waitingForCard && realMotion) {
      if (idleWakeCandidateMs_ == 0) {
        idleWakeCandidateMs_ = nowMs;
      } else if ((nowMs - idleWakeCandidateMs_) >= 900UL) {
        idleWakeCandidateMs_ = 0;
        wakeIdleOverlay(nowMs, "Motion detected. Screen active.");
      }
    } else {
      idleWakeCandidateMs_ = 0;
    }
  } else if (!waitingForCard && shouldShowIdleOverlay(nowMs) && realMotion) {
    wakeIdleOverlay(nowMs, "Motion detected. Screen active.");
  } else if (realMotion) {
    markUserActivity(nowMs);
  }

  float repRawRom = 0.0f;
  const char* repRomSource = nullptr;
  const float activeRepRom = getActiveRomPercent(&repRawRom, &repRomSource);
  const RepMetrics rep = repDetector_.update(activeRepRom, nowMs);
  if (rep.completed && (state_ == State::Calibration || state_ == State::Training)) {
    logEvent("ROM_MAP",
             String("source=") + (repRomSource != nullptr ? repRomSource : "machine_default") +
                 " raw=" + String(repRawRom, 1) +
                 " normalized=" + String(activeRepRom, 1),
             LogLevel::Normal);
    logEvent("REP_ROM",
             String("input source=") + (repRomSource != nullptr ? repRomSource : "machine_default") +
                 " rom=" + String(activeRepRom, 1) +
                 " raw=" + String(repRawRom, 1),
             LogLevel::Normal);
    handleRep(rep, nowMs);
  }

  // Auto-close a set after a long true pause only if user already produced
  // meaningful work (3+ accepted reps) and we are not in rest countdown.
  if (state_ == State::Training && currentSetRepCount_ >= 3 && lastAcceptedRepAtMs_ != 0 &&
      setStartedAtMs_ != 0 && (nowMs - setStartedAtMs_) >= 2500UL &&
      !isRestCountdownActive(nowMs, nullptr)) {
    const bool stillNow = fabsf(rawVelocityPctPerSec) <= kSetPauseStillVelPctPerSec &&
                          motionDelta <= kSetPauseStillDeltaPct;
    if (stillNow) {
      if (setPauseCandidateMs_ == 0) {
        setPauseCandidateMs_ = nowMs;
      }
      const bool stablePause = (nowMs - setPauseCandidateMs_) >= kSetPauseStableMs;
      const bool longSinceRep = (nowMs - lastAcceptedRepAtMs_) >= kSetPauseCloseMs;
      if (stablePause && longSinceRep) {
        const GoalRecommendation* recommendation = getRecommendation();
        const uint8_t targetSets = activeSessionTargetSets_ > 0
                                       ? activeSessionTargetSets_
                                       : (recommendation != nullptr ? recommendation->targetSets : 3);
        const uint16_t restSeconds = activeSessionRestSeconds_ > 0
                                         ? activeSessionRestSeconds_
                                         : (recommendation != nullptr ? recommendation->restSeconds : 45);
        const uint8_t activeSetNumber = completedSets_ + 1;
        const uint8_t repsDoneThisSet = currentSetRepCount_;
        sessionRecorder_.completeSet(activeSetNumber, repsDoneThisSet, restSeconds, nowMs);
        completedSets_++;
        logEvent("SESSION",
                 String("set complete set=") + String(activeSetNumber) +
                     " completedSets=" + String(completedSets_) +
                     " targetSets=" + String(targetSets) +
                     " source=pause_detect",
                 LogLevel::Normal);
        currentSetRepCount_ = 0;
        lastSetCompletedMs_ = nowMs;
        sessionRecorder_.beginRest(activeSetNumber, nowMs);
        setPauseCandidateMs_ = 0;
        setStatusMessage("Set auto-ended after pause.");
    if (completedSets_ >= targetSets) {
      if (!autoFinishRequestPending_) {
        autoFinishRequestPending_ = true;
        autoFinishReason_ = "All target sets completed.";
        logEvent("SESSION",
                 String("auto finish requested reason=") + autoFinishReason_ +
                     " completedSets=" + String(completedSets_) +
                     " targetSets=" + String(targetSets),
                 LogLevel::Normal);
      }
      return;
    }
        refreshUi();
      }
    } else {
      setPauseCandidateMs_ = 0;
    }
  } else {
    setPauseCandidateMs_ = 0;
  }
}

void SmartGymTouchApp::updateAutoMotion() {
  const uint32_t now = millis();

  if (!autoMotionEnabled_) {
    uiCache_.lastAutoMotionMs = now;
    return;
  }

  if (uiCache_.lastAutoMotionMs == 0) {
    uiCache_.lastAutoMotionMs = now;
    return;
  }

  const uint32_t dtMs = now > uiCache_.lastAutoMotionMs ? now - uiCache_.lastAutoMotionMs : 1;
  uiCache_.lastAutoMotionMs = now;
  const float delta = kAutoMotionSpeedPctPerSec * (static_cast<float>(dtMs) / 1000.0f);

  if (autoMotionPhase_ == AutoMotionPhase::Up) {
    simulatedRomPercent_ = min(100.0f, simulatedRomPercent_ + delta);
    if (simulatedRomPercent_ >= 100.0f) {
      simulatedRomPercent_ = 100.0f;
      autoMotionPhase_ = AutoMotionPhase::Down;
    }
  } else if (autoMotionPhase_ == AutoMotionPhase::Down) {
    simulatedRomPercent_ = max(0.0f, simulatedRomPercent_ - delta);
    if (simulatedRomPercent_ <= 0.0f) {
      simulatedRomPercent_ = 0.0f;
      if (autoRepContinuous_) {
        autoMotionPhase_ = AutoMotionPhase::Up;
      } else if (autoRepStopAfterCycle_) {
        autoMotionEnabled_ = false;
        autoMotionPhase_ = AutoMotionPhase::Idle;
        autoRepStopAfterCycle_ = false;
      } else {
        autoMotionEnabled_ = false;
        autoMotionPhase_ = AutoMotionPhase::Idle;
      }
    }
  }

  liveRomPercent_ = simulatedRomPercent_;
  syncRomDebugSliderIfPresent();
}

void SmartGymTouchApp::syncRomDebugSliderIfPresent() {
  if (romSlider_ == nullptr) {
    return;
  }
  lv_slider_set_value(romSlider_, static_cast<int16_t>(simulatedRomPercent_), LV_ANIM_OFF);
}

void SmartGymTouchApp::startSyncWorker() {
  static bool loggedMode = false;
  if (!kUseCloudSyncWorker) {
    if (!loggedMode) {
      loggedMode = true;
      Serial.println("[Cloud] sync mode: single-context");
      Serial.println("[Cloud] worker disabled");
    }
    return;
  }
  if (!cloudEnabled_ || syncTaskHandle_ != nullptr) {
    return;
  }
  syncWorkerStop_ = false;
  constexpr uint32_t kSyncTaskStackWords = 20480;
  BaseType_t created = xTaskCreatePinnedToCore(syncTaskEntry,
                                                "sg_sync",
                                                kSyncTaskStackWords,
                                                this,
                                                1,
                                                &syncTaskHandle_,
                                                0);
  if (created != pdPASS) {
    syncTaskHandle_ = nullptr;
    logEvent("SYNC", "failed to start sync worker task", LogLevel::Normal);
  }
}

void SmartGymTouchApp::startUploadTransportWorker() {
  static bool loggedMode = false;
  if (!kUseUploadTransportWorker) {
    return;
  }
  if (!loggedMode) {
    loggedMode = true;
    Serial.println("[Cloud] upload transport worker enabled");
  }
  if (!cloudEnabled_ || uploadTransportTaskHandle_ != nullptr) {
    return;
  }
  uploadTransportWorkerStop_ = false;
  uploadTransportWorkerBusy_ = false;
  uploadTransportLastIdleLogMs_ = 0;
  uploadTransportLastLowHeapLogMs_ = 0;
  constexpr uint32_t kTransportTaskStackBytes = 16384;
  constexpr BaseType_t kTransportTaskPriority = 1;
  constexpr BaseType_t kTransportTaskCore = 0;
  BaseType_t created = xTaskCreatePinnedToCore(uploadTransportTaskEntry,
                                                "sg_upload",
                                                kTransportTaskStackBytes,
                                                this,
                                                kTransportTaskPriority,
                                                &uploadTransportTaskHandle_,
                                                kTransportTaskCore);
  if (created != pdPASS) {
    uploadTransportTaskHandle_ = nullptr;
    Serial.println("[Cloud] upload transport worker failed to start");
    return;
  }
  Serial.printf("[Cloud] upload transport worker started core=%d stackBytes=%u\n",
                static_cast<int>(kTransportTaskCore),
                static_cast<unsigned>(kTransportTaskStackBytes));
}

void SmartGymTouchApp::requestCloudSync(bool force) {
  if (!cloudEnabled_) {
    return;
  }
  if (force) {
    syncWorkerForce_ = true;
  }
}

void SmartGymTouchApp::syncTaskEntry(void* arg) {
  auto* app = static_cast<SmartGymTouchApp*>(arg);
  while (app != nullptr && !app->syncWorkerStop_) {
    const uint32_t nowMs = millis();
    const SessionStage stage = app->getSessionStage(nowMs);
    const uint32_t intervalMs =
        stage == SessionStage::Summary ? kSummaryCloudServiceIntervalMs :
        (stage == SessionStage::Identify || stage == SessionStage::Idle ? kCloudServiceIntervalIdentifyMs
                                                                         : kCloudServiceIntervalMs);
    const bool force = app->syncWorkerForce_;
    const bool due = force || app->syncWorkerLastServiceMs_ == 0 ||
                     (nowMs - app->syncWorkerLastServiceMs_) >= intervalMs;
    if (due) {
      app->syncWorkerForce_ = false;
      app->syncWorkerLastServiceMs_ = nowMs;
      app->syncWorkerBusy_ = true;
      app->updateCloudSyncWorker(nowMs, force);
      app->syncWorkerBusy_ = false;
    }
    vTaskDelay(pdMS_TO_TICKS(120));
  }
  if (app != nullptr) {
    app->syncWorkerBusy_ = false;
    app->syncTaskHandle_ = nullptr;
  }
  vTaskDelete(nullptr);
}

void SmartGymTouchApp::uploadTransportTaskEntry(void* arg) {
  auto* app = static_cast<SmartGymTouchApp*>(arg);
  while (app != nullptr && !app->uploadTransportWorkerStop_) {
    const uint32_t nowMs = millis();
    if (!app->cloudEnabled_ || !app->firebaseService_.isWifiConnected()) {
      vTaskDelay(pdMS_TO_TICKS(400));
      continue;
    }

    const uint16_t queueCount = app->localPersistenceStore_.getPendingUploadCount();
    if (queueCount == 0) {
      if (app->uploadTransportLastIdleLogMs_ == 0 ||
          (nowMs - app->uploadTransportLastIdleLogMs_) >= 15000UL) {
        app->uploadTransportLastIdleLogMs_ = nowMs;
        Serial.println("[SYNC][worker] idle queue=0");
      }
      vTaskDelay(pdMS_TO_TICKS(300));
      continue;
    }

    CloudLockGuard cloudLock(app->cloudMutex_, 0);
    if (!cloudLock) {
      vTaskDelay(pdMS_TO_TICKS(80));
      continue;
    }

    app->uploadTransportWorkerBusy_ = true;
    app->retryPendingUploads(nowMs, false);
    app->uploadTransportWorkerBusy_ = false;
    vTaskDelay(pdMS_TO_TICKS(150));
  }
  if (app != nullptr) {
    app->uploadTransportWorkerBusy_ = false;
    app->uploadTransportTaskHandle_ = nullptr;
  }
  vTaskDelete(nullptr);
}

void SmartGymTouchApp::updateCloudSync(uint32_t nowMs, bool forcePendingUploads) {
  if (!cloudEnabled_) {
    return;
  }
  if (kUseCloudSyncWorker) {
    requestCloudSync(forcePendingUploads);
    return;
  }
  const bool forced = forcePendingUploads || syncWorkerForce_;
  syncWorkerForce_ = false;
  updateCloudSyncWorker(nowMs, forced);
}

void SmartGymTouchApp::resetPendingWebDetailUpload() {
  pendingWebDetailUpload_ = false;
  pendingWebDetailWeekKey_.remove(0);
  pendingWebDetailDayKey_.remove(0);
  pendingWebDetailSessionPath_.remove(0);
  pendingWebDetailPhase_ = 0;
  pendingWebDetailNextSet_ = 1;
  pendingWebDetailNextRep_ = 1;
  pendingWebDetailSplitSet_ = false;
  pendingWebDetailSetSummaryQueued_ = false;
  pendingWebRootUploaded_ = false;
  pendingWebRootPath_.remove(0);
  pendingWebCoreBundleRequired_ = false;
  pendingWebCoreBundleUploaded_ = false;
  pendingWebCoreBundlePath_.remove(0);
  pendingWebCoreBundlePayloadBytes_ = 0;
  pendingWebDetailPauseLogMs_ = 0;
  pendingWebDetailPauseQueueCount_ = 0xFFFF;
  pendingWebDetailDeferSet_ = 0;
  pendingWebDetailDeferQueueCount_ = 0xFFFF;
  pendingWebDetailDeferLogMs_ = 0;
  summaryUploadStageText_ = "";
}

void SmartGymTouchApp::pumpPendingWebDetailUpload() {
  if (!pendingWebDetailUpload_) {
    return;
  }
  const uint32_t nowMs = millis();
  if (pendingSessionQueueRecord_.sessionId.isEmpty()) {
    logEvent("SYNC", "detail pump paused reason=missing_session_record", LogLevel::Normal);
    resetPendingWebDetailUpload();
    return;
  }

  const uint16_t queueCountStart = localPersistenceStore_.getPendingUploadCount();
  if (pendingWebDetailPhase_ <= 2 && queueCountStart > 0) {
    const bool shouldLogPause =
        pendingWebDetailPauseQueueCount_ != queueCountStart ||
        pendingWebDetailPauseLogMs_ == 0 ||
        (nowMs - pendingWebDetailPauseLogMs_) >= kWebDetailWaitingLogCadenceMs;
    if (shouldLogPause) {
      pendingWebDetailPauseLogMs_ = nowMs;
      pendingWebDetailPauseQueueCount_ = queueCountStart;
      logEvent("SYNC",
               String("detail pump paused reason=mandatory_queue_not_drained queueCount=") +
                   String(queueCountStart),
               LogLevel::Normal);
      if (pendingWebDetailPhase_ == 1) {
        if (pendingWebCoreBundleRequired_ && !pendingWebCoreBundleUploaded_) {
          logEvent("SYNC",
                   String("core bundle upload pending queueCount=") + String(queueCountStart),
                   LogLevel::Normal);
        }
        logEvent("SYNC",
                 String("webapp upload phase=details waiting queueCount=") + String(queueCountStart),
                 LogLevel::Normal);
      } else if (pendingWebDetailPhase_ == 2) {
        logEvent("SYNC",
                 String("webapp upload phase=repSets waiting queueCount=") + String(queueCountStart),
                 LogLevel::Normal);
      }
    }
    return;
  }
  if (pendingWebDetailPhase_ == 3 && queueCountStart > 0) {
    const bool shouldLogWait =
        pendingWebDetailPauseQueueCount_ != queueCountStart ||
        pendingWebDetailPauseLogMs_ == 0 ||
        (nowMs - pendingWebDetailPauseLogMs_) >= kWebDetailWaitingLogCadenceMs;
    if (shouldLogWait) {
      pendingWebDetailPauseLogMs_ = nowMs;
      pendingWebDetailPauseQueueCount_ = queueCountStart;
      logEvent("SYNC",
               String("webapp upload phase=repSets waiting queueCount=") + String(queueCountStart),
               LogLevel::Normal);
    }
    return;
  }

  const SessionHistoryRecord& record = pendingSessionQueueRecord_;
  struct SetUploadSummary {
    bool present = false;
    uint8_t setNumber = 0;
    uint16_t repCount = 0;
    uint16_t validReps = 0;
    uint16_t invalidReps = 0;
    uint16_t fastEccentricWarnings = 0;
    float avgRomPercent = 0.0f;
    float avgConcentricTimeMs = 0.0f;
    float avgPeakVelocityPctPerSec = 0.0f;
    float avgPeakEccentricVelocityPctPerSec = 0.0f;
    float selectedWeightKgStart = 0.0f;
    float selectedWeightKgEnd = 0.0f;
    uint8_t targetRepsUsed = 0;
    uint8_t targetRepsMin = 0;
    uint8_t targetRepsMax = 0;
    uint16_t plannedRestSeconds = 0;
    uint16_t actualRestSeconds = 0;
    uint32_t startOffsetMs = 0;
    uint32_t endOffsetMs = 0;
    bool weightChangedDuringSet = false;
  };

  SetUploadSummary setSummaries[SessionHistoryRecord::kMaxSets];
  for (uint8_t setNumber = 1; setNumber <= SessionHistoryRecord::kMaxSets; ++setNumber) {
    SetUploadSummary& summary = setSummaries[setNumber - 1];
    const SetHistoryRecord& setRecord = record.sets[setNumber - 1];
    summary.setNumber = setNumber;
    summary.validReps = setRecord.validReps;
    summary.invalidReps = setRecord.invalidReps;
    summary.fastEccentricWarnings = setRecord.fastEccentricWarnings;
    summary.avgRomPercent = setRecord.avgRomPercent;
    summary.avgConcentricTimeMs = setRecord.avgConcentricTimeMs;
    summary.avgPeakVelocityPctPerSec = setRecord.avgPeakVelocityPctPerSec;
    summary.avgPeakEccentricVelocityPctPerSec = setRecord.avgPeakEccentricVelocityPctPerSec;
    summary.targetRepsUsed = setRecord.targetRepsUsed;
    summary.targetRepsMin = record.targetRepsMin;
    summary.targetRepsMax = record.targetRepsMax;
    summary.plannedRestSeconds = setRecord.plannedRestSeconds;
    summary.actualRestSeconds = setRecord.actualRestSeconds;
    summary.startOffsetMs = setRecord.startOffsetMs;
    summary.endOffsetMs = setRecord.endOffsetMs;

    bool hasWeight = false;
    for (uint16_t repIndex = 0; repIndex < record.repCount; ++repIndex) {
      const RepHistoryRecord& rep = record.reps[repIndex];
      if (rep.setNumber != setNumber) {
        continue;
      }
      summary.present = true;
      summary.repCount++;
      if (!hasWeight) {
        summary.selectedWeightKgStart = rep.selectedWeightKg;
        summary.selectedWeightKgEnd = rep.selectedWeightKg;
        hasWeight = true;
      } else {
        summary.selectedWeightKgEnd = rep.selectedWeightKg;
        if (fabsf(summary.selectedWeightKgEnd - summary.selectedWeightKgStart) > 0.05f) {
          summary.weightChangedDuringSet = true;
        }
      }
    }
    if (!summary.present) {
      summary.present = summary.validReps > 0 || summary.invalidReps > 0;
    }
    if (!hasWeight) {
      summary.selectedWeightKgStart = record.selectedWeightKg;
      summary.selectedWeightKgEnd = record.selectedWeightKg;
    }
  }

  const auto buildSetSummaryJson = [&](const SetUploadSummary& summary) -> String {
    String json = "{";
    json += "\"setNumber\":" + String(summary.setNumber) + ",";
    json += "\"repCount\":" + String(summary.repCount) + ",";
    json += "\"validReps\":" + String(summary.validReps) + ",";
    json += "\"invalidReps\":" + String(summary.invalidReps) + ",";
    json += "\"fastEccentricWarnings\":" + String(summary.fastEccentricWarnings) + ",";
    json += "\"avgRomPercent\":" + String(summary.avgRomPercent, 2) + ",";
    json += "\"avgConcentricTimeMs\":" + String(summary.avgConcentricTimeMs, 2) + ",";
    json += "\"avgPeakVelocityPctPerSec\":" + String(summary.avgPeakVelocityPctPerSec, 2) + ",";
    json += "\"avgPeakEccentricVelocityPctPerSec\":" + String(summary.avgPeakEccentricVelocityPctPerSec, 2) + ",";
    json += "\"selectedWeightKgStart\":" + String(summary.selectedWeightKgStart, 2) + ",";
    json += "\"selectedWeightKgEnd\":" + String(summary.selectedWeightKgEnd, 2) + ",";
    json += "\"targetRepsUsed\":" + String(summary.targetRepsUsed) + ",";
    json += "\"targetRepsMin\":" + String(summary.targetRepsMin) + ",";
    json += "\"targetRepsMax\":" + String(summary.targetRepsMax) + ",";
    json += "\"plannedRestSeconds\":" + String(summary.plannedRestSeconds) + ",";
    json += "\"actualRestSeconds\":" + String(summary.actualRestSeconds) + ",";
    json += "\"startOffsetMs\":" + String(summary.startOffsetMs) + ",";
    json += "\"endOffsetMs\":" + String(summary.endOffsetMs) + ",";
    json += "\"weightChangedDuringSet\":" + String(summary.weightChangedDuringSet ? "true" : "false");
    json += "}";
    return json;
  };

  const auto buildRepJson = [&](const RepHistoryRecord& rep, uint16_t index, uint8_t setNumber) -> String {
    String repJson = "{";
    repJson += "\"index\":" + String(index) + ",";
    repJson += "\"setNumber\":" + String(setNumber) + ",";
    repJson += "\"repNumberInSet\":" + String(rep.repNumberInSet) + ",";
    repJson += "\"valid\":" + String(rep.valid ? "true" : "false") + ",";
    repJson += "\"warningFastEccentric\":" + String(rep.warningFastEccentric ? "true" : "false") + ",";
    repJson += "\"selectedWeightKg\":" + String(rep.selectedWeightKg, 2) + ",";
    repJson += "\"romPercent\":" + String(rep.romPercent, 2) + ",";
    repJson += "\"durationMs\":" + String(rep.durationMs) + ",";
    repJson += "\"concentricTimeMs\":" + String(rep.concentricTimeMs) + ",";
    repJson += "\"peakVelocityPctPerSec\":" + String(rep.peakVelocityPctPerSec, 2) + ",";
    repJson += "\"peakEccentricVelocityPctPerSec\":" + String(rep.peakEccentricVelocityPctPerSec, 2) + ",";
    repJson += "\"invalidFlags\":" + String(rep.invalidFlags) + ",";
    repJson += "\"offsetMs\":" + String(rep.offsetMs);
    repJson += "}";
    return repJson;
  };

  uint16_t queuedPutItems = 0;
  uint16_t queuedPatchItems = 0;
  uint16_t estimatedRequestsSaved = 0;
  const uint32_t enqueueBatchStartMs = millis();
  const uint16_t enqueueBatchQueueBefore = queueCountStart;
  uint16_t enqueueBatchAdded = 0;
  uint16_t enqueueQueueCount = queueCountStart;
  if (kSyncRequestTimingTrace) {
    logEvent("SYNC",
             String("enqueue batch begin phase=") + String(pendingWebDetailPhase_) +
                 " queueBefore=" + String(enqueueBatchQueueBefore) +
                 " heap=" + String(ESP.getFreeHeap()),
             LogLevel::Normal);
  }
  const bool firebaseClientUploadMode = kUseFirebaseClientQueuedTransport;
  uint16_t detailPayloadHardMaxBytes =
      firebaseClientUploadMode ? kFirebaseClientPatchPayloadHardMaxBytes : kNvsUploadMaxPayloadBytes;
  uint16_t detailPatchTargetBytes =
      firebaseClientUploadMode ? kFirebaseClientPatchPayloadTargetBytes : kWebAppPatchPayloadTargetBytes;
  uint16_t repSetsPatchTargetBytes =
      firebaseClientUploadMode ? kFirebaseClientRepSetsSplitPatchTargetBytes : kRepSetsPatchPayloadTargetBytes;
  uint16_t repSetsPatchHardMaxBytes =
      firebaseClientUploadMode ? kFirebaseClientRepSetsSplitPatchHardMaxBytes : detailPayloadHardMaxBytes;
  uint8_t repSetsMaxRepsPerPatch =
      firebaseClientUploadMode ? kFirebaseClientRepSetsSplitMaxRepsPerPatch : 0;
  uint16_t representativeRepsMaxBytes =
      firebaseClientUploadMode ? kFirebaseClientRepresentativeRepsMaxBytes : kWebAppPatchPayloadTargetBytes;
  uint16_t setDetailsSinglePatchMaxBytes =
      firebaseClientUploadMode ? kFirebaseClientSetDetailsSinglePatchMaxBytes : kWebAppPatchPayloadTargetBytes;
  const uint16_t repSetSinglePayloadMaxBytes =
      firebaseClientUploadMode ? kFirebaseClientRepSetSinglePayloadMaxBytes : kNvsUploadMaxPayloadBytes;
  const uint16_t repSetFullHardMaxBytes =
      firebaseClientUploadMode && kUseFirebaseClientLargeFullRepSetUpload
          ? kFirebaseClientFullRepSetHardMaxBytes
          : repSetSinglePayloadMaxBytes;
  uint32_t uploadInternalHeap = internalFree8BitHeap();
  uint32_t uploadLargestBlock = internalLargestFree8BitBlock();
  UploadMemoryMode uploadMemoryMode = getUploadMemoryMode(uploadInternalHeap, uploadLargestBlock);
  if (firebaseClientUploadMode) {
    if (uploadMemoryMode == UploadMemoryMode::Constrained) {
      detailPayloadHardMaxBytes = min<uint16_t>(detailPayloadHardMaxBytes, 1100U);
      detailPatchTargetBytes = min<uint16_t>(detailPatchTargetBytes, 1000U);
      representativeRepsMaxBytes = min<uint16_t>(representativeRepsMaxBytes, 1100U);
      setDetailsSinglePatchMaxBytes = min<uint16_t>(setDetailsSinglePatchMaxBytes, 1100U);
      repSetsPatchTargetBytes = kFirebaseClientRepSetsSplitPatchTargetBytesConstrained;
      repSetsPatchHardMaxBytes = kFirebaseClientRepSetsSplitPatchHardMaxBytesConstrained;
      repSetsMaxRepsPerPatch = kFirebaseClientRepSetsSplitMaxRepsPerPatchConstrained;
    } else if (uploadMemoryMode == UploadMemoryMode::Critical) {
      detailPayloadHardMaxBytes = min<uint16_t>(detailPayloadHardMaxBytes, 700U);
      detailPatchTargetBytes = min<uint16_t>(detailPatchTargetBytes, 650U);
      representativeRepsMaxBytes = min<uint16_t>(representativeRepsMaxBytes, 700U);
      setDetailsSinglePatchMaxBytes = min<uint16_t>(setDetailsSinglePatchMaxBytes, 700U);
      repSetsPatchTargetBytes = kFirebaseClientRepSetsSplitPatchTargetBytesCritical;
      repSetsPatchHardMaxBytes = kFirebaseClientRepSetsSplitPatchHardMaxBytesCritical;
      repSetsMaxRepsPerPatch = kFirebaseClientRepSetsSplitMaxRepsPerPatchCritical;
    }
    logEvent("SYNC",
             String("upload memory mode=") + uploadMemoryModeText(uploadMemoryMode) +
                 " internalHeap=" + String(uploadInternalHeap) +
                 " largestBlock=" + String(uploadLargestBlock),
             LogLevel::Normal);
    if (uploadMemoryMode != UploadMemoryMode::Normal) {
      logEvent("SYNC",
               String("low-memory split limits targetBytes=") +
                   String(repSetsPatchTargetBytes) +
                   " hardMaxBytes=" + String(repSetsPatchHardMaxBytes) +
                   " maxRepsPerPatch=" + String(repSetsMaxRepsPerPatch),
               LogLevel::Normal);
    }
    if (uploadMemoryMode != UploadMemoryMode::Normal && !lowMemoryUploadUiFreed_) {
      freeOptionalUiForMemory("low_memory_upload", true);
      lowMemoryUploadUiFreed_ = true;
    }
  }
  if (pendingWebDetailPhase_ == 1) {
    const bool rootUploaded = pendingWebRootUploaded_;
    const bool coreBundleUploaded = !pendingWebCoreBundleRequired_ || pendingWebCoreBundleUploaded_;
    logEvent("SYNC",
             String("core phase complete rootUploaded=") + String(rootUploaded ? 1 : 0) +
                 " coreBundleUploaded=" + String(coreBundleUploaded ? 1 : 0),
             LogLevel::Normal);
    if (!coreBundleUploaded) {
      logEvent("SYNC", "core phase blocked reason=core_bundle_not_uploaded", LogLevel::Normal);
      return;
    }
  }
  if (pendingWebDetailPhase_ == 1) {
    logEvent("SYNC",
             String("upload limits mode=") + (firebaseClientUploadMode ? "firebaseclient" : "legacy") +
                 " repSetMax=" + String(repSetSinglePayloadMaxBytes) +
                 " patchTarget=" + String(detailPatchTargetBytes),
             LogLevel::Normal);
  }

  const auto hasFieldSplitQueueCapacity = [&](uint16_t needed) -> bool {
    if (static_cast<uint16_t>(enqueueQueueCount + needed) <= kMaxQueueCountAfterWebAppDetails) {
      return true;
    }
    logEvent("SYNC",
             String("field split paused reason=queue_capacity queueCount=") +
                 String(enqueueQueueCount) +
                 " needed=" + String(needed),
             LogLevel::Normal);
    return false;
  };

  const auto enqueueChecked = [&](const String& path,
                                  const String& payload,
                                  const char* label,
                                  const String& method = "PUT") -> bool {
    if (enqueueQueueCount >= kMaxQueueCountAfterWebAppDetails) {
      logEvent("SYNC",
               String("field split paused reason=queue_capacity queueCount=") +
                   String(enqueueQueueCount) +
                   " needed=1",
               LogLevel::Normal);
      return false;
    }
    if (payload.length() > detailPayloadHardMaxBytes) {
      logEvent("SYNC",
               String("queue failed ") + label + " path=" + path +
                   " bytes=" + String(payload.length()),
               LogLevel::Normal);
      return false;
    }
    if (!localPersistenceStore_.enqueueUpload(path, payload, method)) {
      logEvent("SYNC",
               String("queue failed ") + label + " path=" + path +
                   " bytes=" + String(payload.length()),
               LogLevel::Normal);
      return false;
    }
    if (method.equalsIgnoreCase("PATCH")) {
      queuedPatchItems++;
    } else {
      queuedPutItems++;
    }
    enqueueBatchAdded++;
    enqueueQueueCount++;
    return true;
  };

  if (pendingWebDetailPhase_ == 1) {
    auto buildRepresentativeRepJson = [&](const RepHistoryRecord& rep) -> String {
      String repJson = "{";
      repJson += "\"setNumber\":" + String(rep.setNumber) + ",";
      repJson += "\"repNumberInSet\":" + String(rep.repNumberInSet) + ",";
      repJson += "\"romPercent\":" + String(rep.romPercent, 2) + ",";
      repJson += "\"selectedWeightKg\":" + String(rep.selectedWeightKg, 2) + ",";
      repJson += "\"durationMs\":" + String(rep.durationMs) + ",";
      repJson += "\"concentricTimeMs\":" + String(rep.concentricTimeMs) + ",";
      repJson += "\"peakVelocityPctPerSec\":" + String(rep.peakVelocityPctPerSec, 2) + ",";
      repJson += "\"peakEccentricVelocityPctPerSec\":" + String(rep.peakEccentricVelocityPctPerSec, 2);
      repJson += "}";
      return repJson;
    };

    bool hasValidRep = false;
    RepHistoryRecord bestRomRep;
    RepHistoryRecord bestVelocityRep;
    RepHistoryRecord firstValidRep;
    RepHistoryRecord lastValidRep;
    for (uint16_t repIndex = 0; repIndex < record.repCount; ++repIndex) {
      const RepHistoryRecord& rep = record.reps[repIndex];
      if (!rep.valid) {
        continue;
      }
      if (!hasValidRep) {
        hasValidRep = true;
        bestRomRep = rep;
        bestVelocityRep = rep;
        firstValidRep = rep;
        lastValidRep = rep;
      } else {
        if (rep.romPercent > bestRomRep.romPercent) {
          bestRomRep = rep;
        }
        if (rep.peakVelocityPctPerSec > bestVelocityRep.peakVelocityPctPerSec) {
          bestVelocityRep = rep;
        }
        if (rep.offsetMs < firstValidRep.offsetMs) {
          firstValidRep = rep;
        }
        if (rep.offsetMs >= lastValidRep.offsetMs) {
          lastValidRep = rep;
        }
      }
    }
    String representativeRepsJson = "{}";
    if (hasValidRep) {
      representativeRepsJson = "{";
      representativeRepsJson += "\"bestRom\":" + buildRepresentativeRepJson(bestRomRep) + ",";
      representativeRepsJson += "\"bestVelocity\":" + buildRepresentativeRepJson(bestVelocityRep) + ",";
      representativeRepsJson += "\"firstValid\":" + buildRepresentativeRepJson(firstValidRep) + ",";
      representativeRepsJson += "\"lastValid\":" + buildRepresentativeRepJson(lastValidRep);
      representativeRepsJson += "}";
    }
    logEvent("SYNC",
             String("representativeReps payloadBytes=") + String(representativeRepsJson.length()),
             LogLevel::Normal);
    uint8_t presentSetCount = 0;
    for (uint8_t setNumber = 1; setNumber <= SessionHistoryRecord::kMaxSets; ++setNumber) {
      if (setSummaries[setNumber - 1].present) {
        presentSetCount++;
      }
    }
    const String setDetailsIdem = "setdetails_" + record.sessionId;
    String setDetailsJson = "{";
    setDetailsJson += "\"schemaVersion\":2,";
    setDetailsJson += "\"idempotencyKey\":\"" + jsonEscape(setDetailsIdem) + "\",";
    setDetailsJson += "\"sessionId\":\"" + jsonEscape(record.sessionId) + "\",";
    setDetailsJson += "\"setCount\":" + String(presentSetCount) + ",";
    setDetailsJson += "\"sets\":{";
    bool firstSet = true;
    for (uint8_t setNumber = 1; setNumber <= SessionHistoryRecord::kMaxSets; ++setNumber) {
      const SetUploadSummary& summary = setSummaries[setNumber - 1];
      if (!summary.present) {
        continue;
      }
      if (!firstSet) {
        setDetailsJson += ",";
      }
      firstSet = false;
      setDetailsJson += "\"set" + String(setNumber) + "\":" + buildSetSummaryJson(summary);
    }
    setDetailsJson += "}}";
    logEvent("SYNC", String("setDetails payloadBytes=") + String(setDetailsJson.length()), LogLevel::Normal);
    bool detailsBundleQueued = false;
    if (firebaseClientUploadMode && kUseFirebaseClientDetailsBundle) {
      String detailsBundlePayload = "{";
      detailsBundlePayload += "\"representativeReps\":" + representativeRepsJson + ",";
      detailsBundlePayload += "\"setDetails\":" + setDetailsJson;
      detailsBundlePayload += "}";
      logEvent("SYNC",
               String("details bundle mode=firebaseclient payloadBytes=") + String(detailsBundlePayload.length()),
               LogLevel::Normal);
      if (detailsBundlePayload.length() > kFirebaseClientPatchPayloadHardMaxBytes) {
        logEvent("SYNC",
                 String("details bundle fallback reason=payload_too_large payloadBytes=") +
                     String(detailsBundlePayload.length()),
                 LogLevel::Normal);
      } else {
        if (uploadMemoryMode != UploadMemoryMode::Normal && detailsBundlePayload.length() > 1200U) {
          logEvent("SYNC",
                   String("details bundle fallback reason=heap_guard internalHeap=") +
                       String(uploadInternalHeap) +
                       " largestBlock=" + String(uploadLargestBlock) +
                       " mode=" + uploadMemoryModeText(uploadMemoryMode),
                   LogLevel::Normal);
        } else {
        uint32_t minInternalHeap = 43000UL;
        uint32_t minLargestBlock = 22000UL;
        if (detailsBundlePayload.length() <= 1200U) {
          minInternalHeap = 43000UL;
          minLargestBlock = 22000UL;
        } else if (detailsBundlePayload.length() <= 2200U) {
          minInternalHeap = 50000UL;
          minLargestBlock = 24000UL;
        } else if (detailsBundlePayload.length() <= 3000U) {
          minInternalHeap = 52000UL;
          minLargestBlock = 26000UL;
        } else {
          minInternalHeap = 56000UL;
          minLargestBlock = 34000UL;
        }
        const uint32_t actualInternalHeap = internalFree8BitHeap();
        const uint32_t actualLargestBlock = internalLargestFree8BitBlock();
        if (actualInternalHeap < minInternalHeap || actualLargestBlock < minLargestBlock) {
          logEvent("SYNC",
                   String("details bundle fallback reason=heap_guard internalHeap=") + String(actualInternalHeap) +
                       " largestBlock=" + String(actualLargestBlock) +
                       " minInternalHeap=" + String(minInternalHeap) +
                       " minLargestBlock=" + String(minLargestBlock),
                   LogLevel::Normal);
        } else {
          detailsBundleQueued = enqueueChecked(pendingWebDetailSessionPath_,
                                               detailsBundlePayload,
                                               "detailsBundle",
                                               "PATCH");
          if (!detailsBundleQueued) {
            return;
          }
          logEvent("SYNC",
                   String("details bundle queued path=") + pendingWebDetailSessionPath_,
                   LogLevel::Normal);
        }
        }
      }
    }

    if (!detailsBundleQueued) {
      if (representativeRepsJson.length() <= representativeRepsMaxBytes &&
          representativeRepsJson.length() <= detailPayloadHardMaxBytes) {
        if (!enqueueChecked(pendingWebDetailSessionPath_ + "/representativeReps",
                            representativeRepsJson,
                            "representativeReps",
                            "PATCH")) {
          return;
        }
      } else {
        const String repBasePath = pendingWebDetailSessionPath_ + "/representativeReps";
        struct RepChild {
          const char* key;
          bool hasValue;
          RepHistoryRecord value;
        };
        const RepChild children[] = {
            {"bestRom", hasValidRep, bestRomRep},
            {"bestVelocity", hasValidRep, bestVelocityRep},
            {"firstValid", hasValidRep, firstValidRep},
            {"lastValid", hasValidRep, lastValidRep},
        };
        if (!hasFieldSplitQueueCapacity(4)) {
          return;
        }
        size_t maxChildPayload = 0;
        for (const RepChild& child : children) {
          const String childPayload = child.hasValue ? buildRepresentativeRepJson(child.value) : String("null");
          if (childPayload.length() > maxChildPayload) {
            maxChildPayload = childPayload.length();
          }
          logEvent("SYNC",
                   String("representativeReps split child=") + child.key +
                       " payloadBytes=" + String(childPayload.length()) +
                       " heap=" + String(ESP.getFreeHeap()),
                   LogLevel::Normal);
          if (!enqueueChecked(repBasePath + "/" + String(child.key), childPayload, "representativeRepsChild")) {
            return;
          }
        }
        logEvent("SYNC",
                 String("representativeReps split maxChildPayloadBytes=") + String(maxChildPayload),
                 LogLevel::Normal);
      }

      if (setDetailsJson.length() <= setDetailsSinglePatchMaxBytes &&
          setDetailsJson.length() <= detailPayloadHardMaxBytes) {
        logEvent("SYNC",
                 String("setDetails single upload allowed mode=") +
                     (firebaseClientUploadMode ? "firebaseclient" : "legacy") +
                     " payloadBytes=" + String(setDetailsJson.length()),
                 LogLevel::Normal);
        if (!enqueueChecked(pendingWebDetailSessionPath_ + "/setDetails",
                            setDetailsJson,
                            "setDetails",
                            "PATCH")) {
          return;
        }
      } else {
      const String setDetailsBasePath = pendingWebDetailSessionPath_ + "/setDetails";
      if (!hasFieldSplitQueueCapacity(static_cast<uint16_t>(presentSetCount + 1U))) {
        return;
      }
      String metadataPatch = "{";
      metadataPatch += "\"schemaVersion\":2,";
      metadataPatch += "\"sessionId\":\"" + jsonEscape(record.sessionId) + "\",";
      metadataPatch += "\"idempotencyKey\":\"" + jsonEscape(setDetailsIdem) + "\",";
      metadataPatch += "\"setCount\":" + String(presentSetCount);
      metadataPatch += "}";

      size_t maxSetDetailsChildPayload = 0;
      const auto enqueueSetDetailsPatch = [&](const String& childPath,
                                              const String& childPayload,
                                              const char* childLabel) -> bool {
        if (childPayload.length() > maxSetDetailsChildPayload) {
          maxSetDetailsChildPayload = childPayload.length();
        }
        logEvent("SYNC",
                 String("setDetails split child=") + childLabel +
                     " payloadBytes=" + String(childPayload.length()),
                 LogLevel::Normal);
        return enqueueChecked(childPath, childPayload, "setDetailsSplit", "PATCH");
      };

      logEvent("SYNC",
               String("setDetails PATCH metadata payloadBytes=") + String(metadataPatch.length()),
               LogLevel::Normal);
      if (!enqueueSetDetailsPatch(setDetailsBasePath, metadataPatch, "metadata")) {
        return;
      }
      estimatedRequestsSaved += 3;

      String setsPatch = "{";
      uint8_t setsInCurrentPatch = 0;
      for (uint8_t setNumber = 1; setNumber <= SessionHistoryRecord::kMaxSets; ++setNumber) {
        const SetUploadSummary& summary = setSummaries[setNumber - 1];
        if (!summary.present) {
          continue;
        }
        const String setPayload = buildSetSummaryJson(summary);
        const String fieldEntry = String("\"sets/set") + String(setNumber) + "\":" + setPayload;
        const size_t projectedBytes = setsPatch.length() + (setsInCurrentPatch > 0 ? 1 : 0) + fieldEntry.length() + 1;
        if (setsInCurrentPatch > 0 && projectedBytes > detailPatchTargetBytes) {
          setsPatch += "}";
          logEvent("SYNC",
                   String("setDetails PATCH sets payloadBytes=") + String(setsPatch.length()),
                   LogLevel::Normal);
          if (!enqueueSetDetailsPatch(setDetailsBasePath, setsPatch, "sets")) {
            return;
          }
          if (setsInCurrentPatch > 1) {
            estimatedRequestsSaved += static_cast<uint16_t>(setsInCurrentPatch - 1);
          }
          setsPatch = "{";
          setsInCurrentPatch = 0;
        }
        logEvent("SYNC",
                 String("setDetails split set=") + String(setNumber) +
                     " payloadBytes=" + String(setPayload.length()),
                 LogLevel::Normal);
        if (setsInCurrentPatch > 0) {
          setsPatch += ",";
        }
        setsPatch += fieldEntry;
        setsInCurrentPatch++;
      }
      if (setsInCurrentPatch > 0) {
        setsPatch += "}";
        logEvent("SYNC",
                 String("setDetails PATCH sets payloadBytes=") + String(setsPatch.length()),
                 LogLevel::Normal);
        if (!enqueueSetDetailsPatch(setDetailsBasePath, setsPatch, "sets")) {
          return;
        }
        if (setsInCurrentPatch > 1) {
          estimatedRequestsSaved += static_cast<uint16_t>(setsInCurrentPatch - 1);
        }
      }

      logEvent("SYNC",
               String("setDetails split maxChildPayloadBytes=") + String(maxSetDetailsChildPayload),
               LogLevel::Normal);
      }
    }

    pendingWebDetailPhase_ = 2;
    logEvent("SYNC", "webapp upload phase=details queued representativeReps/setDetails", LogLevel::Normal);
    const uint32_t enqueueBatchDurationMs = millis() - enqueueBatchStartMs;
    if (kSyncRequestTimingTrace) {
      logEvent("SYNC",
               String("enqueue batch end phase=") + String(pendingWebDetailPhase_) +
                   " added=" + String(enqueueBatchAdded) +
                   " durationMs=" + String(enqueueBatchDurationMs) +
                   " queueAfter=" + String(localPersistenceStore_.getPendingUploadCount()) +
                   " heap=" + String(ESP.getFreeHeap()),
               LogLevel::Normal);
    }
    if (enqueueBatchAdded >= kWebDetailBatchPerTick ||
        enqueueBatchDurationMs >= kWebDetailEnqueueTimeBudgetMs) {
      uploadYieldAfterBatchEnqueue_ = true;
    }
    return;
  }

  if (pendingWebDetailPhase_ == 2) {
    pendingWebDetailPhase_ = 3;
    logEvent("SYNC", "batching enabled method=PATCH", LogLevel::Normal);
    logEvent("SYNC", "webapp upload phase=repSets waiting queueCount=0", LogLevel::Normal);
  }

  uint8_t queuedOptional = 0;
  uint16_t queueCount = queueCountStart;
  while (pendingWebDetailUpload_ && queuedOptional < kWebDetailBatchPerTick) {
    uint8_t setNumber = pendingWebDetailNextSet_;
    while (setNumber <= SessionHistoryRecord::kMaxSets && !setSummaries[setNumber - 1].present) {
      ++setNumber;
    }
    if (setNumber > SessionHistoryRecord::kMaxSets) {
      const uint16_t pendingCount = localPersistenceStore_.getPendingUploadCount();
      if (pendingCount > 0) {
        const bool shouldLogWait =
            pendingWebDetailPauseQueueCount_ != pendingCount ||
            pendingWebDetailPauseLogMs_ == 0 ||
            (nowMs - pendingWebDetailPauseLogMs_) >= kWebDetailWaitingLogCadenceMs;
        if (shouldLogWait) {
          pendingWebDetailPauseLogMs_ = nowMs;
          pendingWebDetailPauseQueueCount_ = pendingCount;
          logEvent("SYNC",
                   String("webapp upload phase=repSets waiting queueCount=") + String(pendingCount),
                   LogLevel::Normal);
        }
        return;
      }
      lastWebUploadPhaseCompleteMs_ = nowMs;
      logEvent("SYNC",
               String("webapp upload phase=complete sessionId=") + pendingSessionQueueRecord_.sessionId,
               LogLevel::Normal);
      const String completedSessionId = pendingSessionQueueRecord_.sessionId;
      resetPendingWebDetailUpload();
      clearSessionHistoryRecordInPlace(pendingSessionQueueRecord_);
      const uint16_t queueAfterComplete = localPersistenceStore_.getPendingUploadCount();
      if (!pendingSessionQueueEnqueue_ &&
          !pendingSessionDetailQueueEnqueue_ &&
          !pendingWebDetailUpload_ &&
          pendingWebDetailPhase_ == 0 &&
          queueAfterComplete == 0) {
        forceCloudSyncAfterFinish_ = false;
        syncWorkerForce_ = false;
        logEvent("SYNC",
                 String("finish sync complete id=") + completedSessionId +
                     " clearing_force=1 queueCount=0",
                 LogLevel::Normal);
      }
      break;
    }

    if (queueCount >= kMaxQueueCountAfterWebAppDetails) {
      const bool shouldLogDefer =
          pendingWebDetailDeferSet_ != setNumber ||
          pendingWebDetailDeferQueueCount_ != queueCount ||
          pendingWebDetailDeferLogMs_ == 0 ||
          (nowMs - pendingWebDetailDeferLogMs_) >= kWebDetailWaitingLogCadenceMs;
      if (shouldLogDefer) {
        pendingWebDetailDeferSet_ = setNumber;
        pendingWebDetailDeferQueueCount_ = queueCount;
        pendingWebDetailDeferLogMs_ = nowMs;
        logEvent("SYNC",
                 String("defer repSet set=") + String(setNumber) +
                     " reason=queue_budget count=" + String(queueCount),
                 LogLevel::Normal);
      }
      return;
    }

    SetUploadSummary& summary = setSummaries[setNumber - 1];
    const String repSetBasePath = pendingWebDetailSessionPath_ + "/repSets/set" + String(setNumber);
    if (!pendingWebDetailSplitSet_) {
      String repsArray = "[";
      bool firstRep = true;
      uint16_t repCountInSet = 0;
      for (uint16_t repIndex = 0; repIndex < record.repCount; ++repIndex) {
        const RepHistoryRecord& rep = record.reps[repIndex];
        if (rep.setNumber != setNumber) {
          continue;
        }
        repCountInSet++;
        if (!firstRep) {
          repsArray += ",";
        }
        firstRep = false;
        repsArray += buildRepJson(rep, repIndex + 1, setNumber);
      }
      repsArray += "]";
      String repSetPayload = "{";
      repSetPayload += "\"schemaVersion\":2,";
      repSetPayload += "\"idempotencyKey\":\"" + jsonEscape("repset_" + record.sessionId + "_s" + String(setNumber)) + "\",";
      repSetPayload += "\"sessionId\":\"" + jsonEscape(record.sessionId) + "\",";
      repSetPayload += "\"setNumber\":" + String(setNumber) + ",";
      repSetPayload += "\"count\":" + String(repCountInSet) + ",";
      repSetPayload += "\"setSummary\":" + buildSetSummaryJson(summary) + ",";
      repSetPayload += "\"reps\":" + repsArray;
      repSetPayload += "}";
      logEvent("SYNC",
               String("repSet set=") + String(setNumber) + " payloadBytes=" + String(repSetPayload.length()),
               LogLevel::Normal);
      if (repSetPayload.length() <= repSetFullHardMaxBytes &&
          (!firebaseClientUploadMode || kUseFirebaseClientFullRepSetUpload)) {
        bool allowFullRepSetUpload = true;
        if (firebaseClientUploadMode &&
            uploadMemoryMode != UploadMemoryMode::Normal &&
            repSetPayload.length() > 1200U) {
          allowFullRepSetUpload = false;
          logEvent("SYNC",
                   String("repSet full upload fallback reason=heap_guard set=") + String(setNumber) +
                       " payloadBytes=" + String(repSetPayload.length()) +
                       " internalHeap=" + String(uploadInternalHeap) +
                       " largestBlock=" + String(uploadLargestBlock) +
                       " mode=" + uploadMemoryModeText(uploadMemoryMode),
                   LogLevel::Normal);
        }
        if (firebaseClientUploadMode) {
          uint32_t minInternalHeap = 43000UL;
          uint32_t minLargestBlock = 22000UL;
          if (repSetPayload.length() <= 1200U) {
            minInternalHeap = 43000UL;
            minLargestBlock = 22000UL;
          } else if (repSetPayload.length() <= 2200U) {
            minInternalHeap = 50000UL;
            minLargestBlock = 24000UL;
          } else if (repSetPayload.length() <= 3000U) {
            minInternalHeap = 52000UL;
            minLargestBlock = 26000UL;
          } else if (repSetPayload.length() <= 6200U) {
            minInternalHeap = 56000UL;
            minLargestBlock = 30000UL;
            // If queue rewrite is unsafe, only enqueue large full sets with
            // stronger margin so we do not leave stuck internal-heap items.
            minInternalHeap = max<uint32_t>(minInternalHeap, 58000UL);
            minLargestBlock = max<uint32_t>(minLargestBlock, 30000UL);
          } else {
            allowFullRepSetUpload = false;
          }
          const uint32_t enqueueInternalHeap = internalFree8BitHeap();
          const uint32_t enqueueLargestBlock = internalLargestFree8BitBlock();
          if (!allowFullRepSetUpload ||
              enqueueInternalHeap < minInternalHeap ||
              enqueueLargestBlock < minLargestBlock) {
            allowFullRepSetUpload = false;
            if (repSetPayload.length() > 3000U) {
              logEvent("SYNC",
                       String("repSet large full upload skipped reason=heap_guard set=") +
                           String(setNumber) +
                           " payloadBytes=" + String(repSetPayload.length()) +
                           " internalHeap=" + String(enqueueInternalHeap) +
                           " largestBlock=" + String(enqueueLargestBlock) +
                           " minInternalHeap=" + String(minInternalHeap) +
                           " minLargestBlock=" + String(minLargestBlock),
                       LogLevel::Normal);
            } else {
              logEvent("SYNC",
                       String("repSet full upload fallback reason=heap_guard set=") +
                           String(setNumber) +
                           " payloadBytes=" + String(repSetPayload.length()) +
                           " internalHeap=" + String(enqueueInternalHeap) +
                           " largestBlock=" + String(enqueueLargestBlock) +
                           " minInternalHeap=" + String(minInternalHeap) +
                           " minLargestBlock=" + String(minLargestBlock),
                       LogLevel::Normal);
            }
          }
        }
        if (allowFullRepSetUpload && firebaseClientUploadMode) {
          if (repSetPayload.length() > 3000U) {
            logEvent("SYNC",
                     String("repSet large full upload allowed mode=firebaseclient set=") +
                         String(setNumber) +
                         " payloadBytes=" + String(repSetPayload.length()),
                     LogLevel::Normal);
          } else {
            logEvent("SYNC",
                     String("repSet full upload allowed mode=firebaseclient set=") + String(setNumber) +
                         " payloadBytes=" + String(repSetPayload.length()),
                     LogLevel::Normal);
          }
        }
        if (allowFullRepSetUpload) {
          if (!enqueueChecked(repSetBasePath, repSetPayload, "repSet", "PUT")) {
            return;
          }
          if (firebaseClientUploadMode) {
            if (repSetPayload.length() > 3000U) {
              logEvent("SYNC",
                       String("repSet large full upload queued set=") + String(setNumber) +
                           " payloadBytes=" + String(repSetPayload.length()),
                       LogLevel::Normal);
            } else {
              logEvent("SYNC",
                       String("repSet full upload queued set=") + String(setNumber) +
                           " payloadBytes=" + String(repSetPayload.length()),
                       LogLevel::Normal);
            }
          }
          queuedOptional++;
          queueCount++;
          pendingWebDetailNextSet_ = setNumber + 1;
          pendingWebDetailNextRep_ = 1;
        } else {
          logEvent("SYNC",
                   String("repSet split mode=firebaseclient set=") + String(setNumber) +
                       " reason=full_upload_not_safe",
                   LogLevel::Normal);
          logEvent("SYNC",
                   String("repSet split limits mode=firebaseclient targetBytes=") +
                       String(repSetsPatchTargetBytes) +
                       " hardMaxBytes=" + String(repSetsPatchHardMaxBytes) +
                       " maxRepsPerPatch=" + String(repSetsMaxRepsPerPatch),
                   LogLevel::Normal);
          pendingWebDetailSplitSet_ = true;
          pendingWebDetailSetSummaryQueued_ = false;
          pendingWebDetailNextRep_ = 1;
        }
      } else {
        if (repSetPayload.length() > repSetFullHardMaxBytes) {
          if (firebaseClientUploadMode && repSetPayload.length() > 3000U) {
            logEvent("SYNC",
                     String("repSet large full upload skipped reason=payload_too_large set=") +
                         String(setNumber) +
                         " payloadBytes=" + String(repSetPayload.length()),
                     LogLevel::Normal);
          } else {
            logEvent("SYNC",
                     String("repSet full upload fallback reason=payload_too_large set=") + String(setNumber) +
                         " payloadBytes=" + String(repSetPayload.length()),
                     LogLevel::Normal);
          }
        }
        logEvent("SYNC",
                 String("repSet split mode=") + (firebaseClientUploadMode ? "firebaseclient" : "legacy") +
                     " set=" + String(setNumber) +
                     " reason=too_large_for_single_payload",
                 LogLevel::Normal);
        if (firebaseClientUploadMode) {
          logEvent("SYNC",
                   String("repSet split limits mode=firebaseclient targetBytes=") +
                       String(repSetsPatchTargetBytes) +
                       " hardMaxBytes=" + String(repSetsPatchHardMaxBytes) +
                       " maxRepsPerPatch=" + String(repSetsMaxRepsPerPatch),
                   LogLevel::Normal);
        }
        pendingWebDetailSplitSet_ = true;
        pendingWebDetailSetSummaryQueued_ = false;
        pendingWebDetailNextRep_ = 1;
      }
    }

    if (!pendingWebDetailSplitSet_) {
      continue;
    }

    if (!pendingWebDetailSetSummaryQueued_) {
      if (queueCount >= kMaxQueueCountAfterWebAppDetails) {
        return;
      }
      const String setSummaryPayload = buildSetSummaryJson(summary);
      String setHeaderPayload = "{";
      setHeaderPayload += "\"schemaVersion\":2,";
      setHeaderPayload += "\"sessionId\":\"" + jsonEscape(record.sessionId) + "\",";
      setHeaderPayload += "\"setNumber\":" + String(setNumber) + ",";
      setHeaderPayload += "\"count\":" + String(summary.repCount) + ",";
      setHeaderPayload += "\"idempotencyKey\":\"" + jsonEscape("repset_" + record.sessionId + "_s" + String(setNumber)) + "\",";
      setHeaderPayload += "\"setSummary\":" + setSummaryPayload;
      setHeaderPayload += "}";
      if (setHeaderPayload.length() > detailPayloadHardMaxBytes) {
        pendingWebDetailSplitSet_ = false;
        pendingWebDetailSetSummaryQueued_ = false;
        pendingWebDetailNextSet_ = setNumber + 1;
        pendingWebDetailNextRep_ = 1;
        continue;
      }
      if (!enqueueChecked(repSetBasePath, setHeaderPayload, "repSetHeader", "PUT")) {
        return;
      }
      queuedOptional++;
      queueCount++;
      pendingWebDetailSetSummaryQueued_ = true;
      if (queuedOptional >= kWebDetailBatchPerTick) {
        break;
      }
    }

    uint16_t repNumberInSet = 0;
    bool queuedAnyRep = false;
    String repsPatch = "{";
    uint16_t batchFirstRepZeroBased = 0;
    uint16_t batchCount = 0;
    bool batchHasItems = false;

    const auto flushRepsPatch = [&]() -> bool {
      if (!batchHasItems) {
        return true;
      }
      repsPatch += "}";
      if (firebaseClientUploadMode && repsPatch.length() > repSetsPatchHardMaxBytes) {
        logEvent("SYNC",
                 String("repSet split payload hard max exceeded set=") + String(setNumber) +
                     " payloadBytes=" + String(repsPatch.length()) +
                     " hardMaxBytes=" + String(repSetsPatchHardMaxBytes),
                 LogLevel::Normal);
        return false;
      }
      logEvent("SYNC",
               String("repSet set=") + String(setNumber) +
                   " reps PATCH batch first=" + String(batchFirstRepZeroBased) +
                   " count=" + String(batchCount) +
                   " payloadBytes=" + String(repsPatch.length()),
               LogLevel::Normal);
      if (!enqueueChecked(repSetBasePath, repsPatch, "repSetRepsPatch", "PATCH")) {
        return false;
      }
      queuedOptional++;
      queueCount++;
      if (batchCount > 1) {
        estimatedRequestsSaved += static_cast<uint16_t>(batchCount - 1);
      }
      repsPatch = "{";
      batchFirstRepZeroBased = 0;
      batchCount = 0;
      batchHasItems = false;
      return true;
    };

    for (uint16_t repIndex = 0; repIndex < record.repCount; ++repIndex) {
      const RepHistoryRecord& rep = record.reps[repIndex];
      if (rep.setNumber != setNumber) {
        continue;
      }
      repNumberInSet++;
      if (repNumberInSet < pendingWebDetailNextRep_) {
        continue;
      }
      if (queueCount >= kMaxQueueCountAfterWebAppDetails) {
        if (!flushRepsPatch()) {
          return;
        }
        return;
      }
      if (firebaseClientUploadMode &&
          batchHasItems &&
          repSetsMaxRepsPerPatch > 0 &&
          batchCount >= repSetsMaxRepsPerPatch) {
        if (!flushRepsPatch()) {
          return;
        }
        if (queuedOptional >= kWebDetailBatchPerTick || queueCount >= kMaxQueueCountAfterWebAppDetails) {
          return;
        }
      }
      const String repPayload = buildRepJson(rep, repIndex + 1, setNumber);
      if (repPayload.length() > detailPayloadHardMaxBytes) {
        if (!flushRepsPatch()) {
          return;
        }
        pendingWebDetailNextRep_ = repNumberInSet + 1;
        continue;
      }
      const uint16_t zeroBasedIndex = static_cast<uint16_t>(repNumberInSet - 1);
      const String fieldEntry = String("\"reps/") + String(zeroBasedIndex) + "\":" + repPayload;
      const size_t projectedBytes = repsPatch.length() + (batchHasItems ? 1 : 0) + fieldEntry.length() + 1;
      if (batchHasItems &&
          (projectedBytes > repSetsPatchTargetBytes ||
           (firebaseClientUploadMode && projectedBytes > repSetsPatchHardMaxBytes))) {
        logEvent("SYNC",
                 String("repSet batch shrink reason=payload_too_large set=") + String(setNumber) +
                     " oldCount=" + String(batchCount + 1) +
                     " newCount=" + String(batchCount) +
                     " payloadBytes=" + String(projectedBytes),
                 LogLevel::Normal);
        if (!flushRepsPatch()) {
          return;
        }
        if (queuedOptional >= kWebDetailBatchPerTick || queueCount >= kMaxQueueCountAfterWebAppDetails) {
          return;
        }
      }

      if (!batchHasItems) {
        batchFirstRepZeroBased = zeroBasedIndex;
      } else {
        repsPatch += ",";
      }
      repsPatch += fieldEntry;
      batchHasItems = true;
      batchCount++;
      queuedAnyRep = true;
      pendingWebDetailNextRep_ = repNumberInSet + 1;

      if (repsPatch.length() >= (repSetsPatchTargetBytes - 80) ||
          (firebaseClientUploadMode &&
           (repsPatch.length() >= (repSetsPatchHardMaxBytes - 80) ||
            (repSetsMaxRepsPerPatch > 0 && batchCount >= repSetsMaxRepsPerPatch)))) {
        if (!flushRepsPatch()) {
          return;
        }
        if (queuedOptional >= kWebDetailBatchPerTick || queueCount >= kMaxQueueCountAfterWebAppDetails) {
          break;
        }
      }
    }
    if (batchHasItems) {
      if (!flushRepsPatch()) {
        return;
      }
    }

    if (pendingWebDetailNextRep_ > summary.repCount || (!queuedAnyRep && pendingWebDetailNextRep_ == 1)) {
      pendingWebDetailSplitSet_ = false;
      pendingWebDetailSetSummaryQueued_ = false;
      pendingWebDetailNextSet_ = setNumber + 1;
      pendingWebDetailNextRep_ = 1;
      pendingWebDetailDeferSet_ = 0;
      pendingWebDetailDeferQueueCount_ = 0xFFFF;
      pendingWebDetailDeferLogMs_ = 0;
    }
  }
  if (queuedPutItems > 0 || queuedPatchItems > 0) {
    const uint32_t enqueueBatchDurationMs = millis() - enqueueBatchStartMs;
    if (kSyncRequestTimingTrace) {
      logEvent("SYNC",
               String("enqueue batch end phase=") + String(pendingWebDetailPhase_) +
                   " added=" + String(enqueueBatchAdded) +
                   " durationMs=" + String(enqueueBatchDurationMs) +
                   " queueAfter=" + String(localPersistenceStore_.getPendingUploadCount()) +
                   " heap=" + String(ESP.getFreeHeap()),
               LogLevel::Normal);
    }
    if (enqueueBatchAdded >= kWebDetailBatchPerTick ||
        enqueueBatchDurationMs >= kWebDetailEnqueueTimeBudgetMs) {
      uploadYieldAfterBatchEnqueue_ = true;
    }
    logEvent("SYNC",
             String("upload batching summary putItems=") + String(queuedPutItems) +
                 " patchItems=" + String(queuedPatchItems) +
                 " estimatedRequestsSaved=" + String(estimatedRequestsSaved),
             LogLevel::Normal);
  }
}

void SmartGymTouchApp::updateCloudSyncWorker(uint32_t nowMs, bool forcePendingUploads) {
  const bool pendingFinishSync =
      forceCloudSyncAfterFinish_ || pendingSessionQueueEnqueue_ || pendingSessionDetailQueueEnqueue_;
  if (pendingFinishSync) {
    logEvent("SYNC",
             String("worker enter forced=") + (forcePendingUploads ? "1" : "0") +
                 " wifi=" + (firebaseService_.isWifiConnected() ? "1" : "0") +
                 " pendingSessionQueueEnqueue=" + (pendingSessionQueueEnqueue_ ? "1" : "0") +
                 " queueCount=" + String(localPersistenceStore_.getPendingUploadCount()) +
                 " state=" + stateToText() +
                 " screen=" + uiScreenToText(currentUiScreen_),
             LogLevel::Normal);
  }
  CloudLockGuard cloudLock(cloudMutex_, forcePendingUploads ? 250 : 0);
  if (!cloudLock) {
    if (pendingFinishSync &&
        (lastSyncWorkerDiagLogMs_ == 0 || (nowMs - lastSyncWorkerDiagLogMs_) >= kSyncDiagLogCadenceMs)) {
      lastSyncWorkerDiagLogMs_ = nowMs;
      logEvent("SYNC", "worker skipped reason=lock_busy", LogLevel::Normal);
    }
    return;
  }

  const uint32_t sliceStartMs = millis();
  const auto timedOut = [&]() -> bool {
    return (millis() - sliceStartMs) >= kCloudSliceBudgetMs;
  };

  firebaseService_.update();
  if (timedOut()) {
    return;
  }
  const bool wifiConnectedNow = firebaseService_.isWifiConnected();
  const bool wifiReconnected = (!lastCloudConnected_ && wifiConnectedNow);
  lastCloudConnected_ = wifiConnectedNow;
  const SessionStage stage = getSessionStage(nowMs);
  if (wifiReconnected) {
    // Force an immediate retry cycle after link recovery.
    lastUploadRetryMs_ = 0;
    lastDeviceHeartbeatMs_ = 0;
    logEvent("SYNC", "wifi reconnected; retry window reset", LogLevel::Normal);
  }

  // Reconcile user/calibration after card load on worker context only.
  if (bootMachineRestorePending_ && wifiConnectedNow &&
      (stage == SessionStage::Idle || stage == SessionStage::Identify)) {
    if (shouldSuppressOptionalCloudRead("boot_restore", nowMs)) {
      return;
    }
    if (refreshDeviceCalibrationFromCloud()) {
      bootMachineRestorePending_ = false;
    }
  }

  // Reconcile user/calibration after card load on worker context only.
  if (pendingScanReconcile_ && activeUser_ != nullptr &&
      pendingScanReconcileStartedMs_ != 0 &&
      (nowMs - pendingScanReconcileStartedMs_) >= kUserSyncTimeoutMs) {
    logEvent("USER_SYNC", "calibration fallback reason=timeout", LogLevel::Normal);
    refreshActiveCalibration();
    configureRepDetectorThresholds();
    const String source = resolvedRecommendation_.hasRecommendation
                              ? resolvedRecommendation_.source
                              : String("fallback");
    logEvent("USER_SYNC",
             String("recommendation resolved kg=") +
                 String(resolvedRecommendation_.hasRecommendation ? resolvedRecommendation_.kg : 0.0f, 1) +
                 " source=" + source,
             LogLevel::Normal);
    pendingScanReconcile_ = false;
    pendingScanReconcileEarliestMs_ = 0;
    pendingScanReconcileStartedMs_ = 0;
    pendingScanReconcileStep_ = 0;
    if (userLoading_) {
      userLoading_ = false;
      hideUserLoadingPopup(activeUser_->rfidUid, "fallback");
      logEvent("USER_SYNC",
               String("complete uid=") + activeUser_->rfidUid + " result=fallback",
               LogLevel::Normal);
      showUiScreen(UiScreenMode::Main);
      refreshUi();
    }
  }

  if (pendingScanReconcile_ && activeUser_ != nullptr &&
      nowMs >= pendingScanReconcileEarliestMs_ &&
      (stage == SessionStage::Identify || stage == SessionStage::Idle) &&
      localPersistenceStore_.getPendingUploadCount() == 0) {
    if (shouldSuppressOptionalCloudRead("scan_reconcile", nowMs)) {
      return;
    }
    if (reconcileCloudStateForScan()) {
      pendingScanReconcile_ = false;
      pendingScanReconcileEarliestMs_ = 0;
      pendingScanReconcileStartedMs_ = 0;
      pendingScanReconcileStep_ = 0;
      if (userLoading_) {
        userLoading_ = false;
        hideUserLoadingPopup(activeUser_->rfidUid, "sync_complete");
        logEvent("USER_SYNC",
                 String("complete uid=") + activeUser_->rfidUid + " result=ok",
                 LogLevel::Normal);
        showUiScreen(UiScreenMode::Main);
        refreshUi();
      }
    }
  }

  if (wifiConnectedNow && pendingCanonicalSessionSync_) {
    // Canonical push is heavy (multi-request) and can stall UI on this device.
    // We rely on queued path uploads for responsiveness and clear this marker.
    pendingCanonicalSessionSync_ = false;
    pendingCanonicalSessionRecord_ = SessionHistoryRecord{};
    logEvent("SYNC", "canonical push deferred; using queued incremental uploads", LogLevel::Normal);
  }

  if (!forcePendingUploads && nowMs < kCloudBootGraceMs) {
    if (pendingFinishSync &&
        (lastSyncWorkerDiagLogMs_ == 0 || (nowMs - lastSyncWorkerDiagLogMs_) >= kSyncDiagLogCadenceMs)) {
      lastSyncWorkerDiagLogMs_ = nowMs;
      logEvent("SYNC", "worker skipped reason=boot_grace", LogLevel::Normal);
    }
    return;
  }
  const bool summaryVisible = state_ == State::Summary || currentUiScreen_ == UiScreenMode::Summary;
  const bool summaryQueueReady =
      !summaryVisible || (sessionSummaryStartedMs_ != 0 &&
                          (nowMs - sessionSummaryStartedMs_) >= kSummarySmallSyncDelayMs);
  if (pendingSessionQueueEnqueue_) {
    const uint32_t elapsedMs =
        (sessionSummaryStartedMs_ != 0 && nowMs >= sessionSummaryStartedMs_)
            ? (nowMs - sessionSummaryStartedMs_)
            : 0;
    logEvent("SYNC",
             String("pending session enqueue check summaryVisible=") + (summaryVisible ? "1" : "0") +
                 " summaryQueueReady=" + (summaryQueueReady ? "1" : "0") +
                 " elapsedMs=" + String(elapsedMs),
             LogLevel::Normal);
    if (!summaryQueueReady) {
      logEvent("SYNCGATE",
               String("skipped reason=summary_delay elapsedMs=") + String(elapsedMs) +
                   " requiredMs=" + String(kSummarySmallSyncDelayMs),
               LogLevel::Normal);
    }
  }
  if (pendingSessionQueueEnqueue_ && summaryQueueReady) {
    freeOptionalUiForMemory("session_upload", true);
    lastUploadRetryMs_ = 0;
    logEvent("SYNC",
             String("draining pendingSessionQueueRecord id=") + pendingSessionQueueRecord_.sessionId,
             LogLevel::Normal);
    logEvent("SYNC",
             String("before queue stackWords=") + String(uxTaskGetStackHighWaterMark(nullptr)) +
                 " heap=" + String(ESP.getFreeHeap()),
             LogLevel::Normal);
    const bool compactSummaryUpload = !kUseCloudSyncWorker && summaryVisible;
    if (compactSummaryUpload) {
      logEvent("SYNC", "compact session upload queued in single-context mode", LogLevel::Normal);
    }
    if (!queueSessionUpload(pendingSessionQueueRecord_, !compactSummaryUpload)) {
      logEvent("SYNC_QUEUE",
               "defer session reason=queue_capacity pendingSessionPreserved=1",
               LogLevel::Normal);
      return;
    }
    pendingSessionQueueEnqueue_ = false;
    uploadYieldAfterBatchEnqueue_ = true;
    logEvent("SYNC",
             String("after queue stackWords=") + String(uxTaskGetStackHighWaterMark(nullptr)) +
                 " heap=" + String(ESP.getFreeHeap()),
             LogLevel::Normal);
    if (!pendingWebDetailUpload_) {
      clearSessionHistoryRecordInPlace(pendingSessionQueueRecord_);
    }
    const bool hasQueuedUploadWork =
        pendingWebDetailUpload_ || pendingWebDetailPhase_ != 0 ||
        localPersistenceStore_.getPendingUploadCount() > 0 ||
        pendingSessionDetailQueueEnqueue_;
    if (hasQueuedUploadWork) {
      forceCloudSyncAfterFinish_ = true;
    }
    logEvent("SYNC",
             String("after queue pendingWebDetailUpload=") + (pendingWebDetailUpload_ ? "1" : "0") +
                 " queueCount=" + String(localPersistenceStore_.getPendingUploadCount()),
             LogLevel::Normal);
  }
  if (forceCloudSyncAfterFinish_ &&
      !pendingSessionQueueEnqueue_ &&
      !pendingSessionDetailQueueEnqueue_ &&
      !pendingWebDetailUpload_ &&
      pendingWebDetailPhase_ == 0 &&
      localPersistenceStore_.getPendingUploadCount() == 0) {
    forceCloudSyncAfterFinish_ = false;
    syncWorkerForce_ = false;
    logEvent("SYNC",
             String("finish sync complete id=") + lastFinishedSessionId_ +
                 " clearing_force=1 queueCount=0",
             LogLevel::Normal);
  }
  const bool fullyIdleForHeavyUpload =
      state_ == State::Idle &&
      currentUiScreen_ == UiScreenMode::Idle &&
      activeUser_ == nullptr &&
      !anonymousMode_ &&
      (nowMs - lastUserActivityMs_) >= kHeavyUploadIdleMs;
  if (pendingSessionDetailQueueEnqueue_ && fullyIdleForHeavyUpload) {
    pendingSessionDetailWaitLogged_ = false;
    if (!queueSessionUpload(pendingSessionDetailQueueRecord_, true)) {
      logEvent("SYNC_PHASE", "details deferred reason=queue_pressure", LogLevel::Normal);
      return;
    }
    pendingSessionDetailQueueEnqueue_ = false;
    clearSessionHistoryRecordInPlace(pendingSessionDetailQueueRecord_);
  } else if (pendingSessionDetailQueueEnqueue_ && !pendingSessionDetailWaitLogged_) {
    logEvent("SYNC",
             "session detail upload waiting for idle window; compact summary already queued",
             LogLevel::Normal);
    pendingSessionDetailWaitLogged_ = true;
  }
  if (summaryVisible) {
    if (uploadYieldAfterBatchEnqueue_ && !forcePendingUploads) {
      uploadYieldAfterBatchEnqueue_ = false;
      return;
    }
    // Summary sync is user-visible. Drain compact queued docs only.
    if (!kUseUploadTransportWorker && retryPendingUploads(nowMs, forcePendingUploads)) {
      return;
    }
    pumpPendingWebDetailUpload();
    return;
  }
  // Keep workout graph/UI smooth: defer queued cloud writes while actively training.
  if (!forcePendingUploads && state_ == State::Training) {
    return;
  }
  if (uploadYieldAfterBatchEnqueue_ && !forcePendingUploads) {
    uploadYieldAfterBatchEnqueue_ = false;
    return;
  }
  if (!kUseUploadTransportWorker && retryPendingUploads(nowMs, forcePendingUploads)) {
    return;
  }
  pumpPendingWebDetailUpload();
  if (timedOut()) {
    return;
  }
  // During the workout summary, cloud work is limited to queued session docs.
  // Heartbeats/config reads can wait; this avoids extra TLS calls while the
  // summary screen is being shown.
  if (!forcePendingUploads && state_ == State::Summary) {
    return;
  }
  const bool hasPendingUploads = localPersistenceStore_.getPendingUploadCount() > 0;
  if (hasPendingUploads && !forcePendingUploads) {
    return;
  }
  if (syncDeviceIdentityToCloud(nowMs, forcePendingUploads)) {
    return;
  }
  if (timedOut()) {
    return;
  }
  if (pendingMachineConfigUpload_) {
    pushCurrentMachineConfigToCloud();
    if (timedOut()) {
      return;
    }
  }

  if (machineProfile_ == nullptr) {
    return;
  }
  // Prioritize write-back reliability and UI smoothness over config reads.

  if (lastMachineCloudPollMs_ != 0 &&
      (nowMs - lastMachineCloudPollMs_) < firebaseService_.getMachinePollIntervalMs()) {
    return;
  }

  lastMachineCloudPollMs_ = nowMs;
  MachineCloudConfig cloudConfig;
  cloudConfig.machineId = machineProfile_->machineId;
  cloudConfig.strokeLengthMm = machineProfile_->strokeLengthMm;
  cloudConfig.idealRomPercent = machineProfile_->idealRomPercent;
  cloudConfig.defaultCalibrationWeightKg = machineProfile_->defaultCalibrationWeightKg;
  cloudConfig.targetRepsPerSet = machineProfile_->targetRepsPerSet;
  for (uint8_t i = 0; i < 5; ++i) {
    cloudConfig.recommendations[i] = machineProfile_->recommendations[i];
  }

  if (state_ == State::Training || state_ == State::Summary) {
    return;
  }

  if (timedOut()) {
    return;
  }
  if (shouldSuppressOptionalCloudRead("machine_config_poll", nowMs)) {
    return;
  }
  if (firebaseService_.fetchMachineConfig(machineProfile_->machineId, cloudConfig)) {
    // Validate BEFORE applying; never mutate runtime profile with bad payload.
    const float baselineIdealRom = machineProfile_->idealRomPercent;
    const float baselineStrokeMm = machineProfile_->strokeLengthMm;
    const uint8_t baselineTargetReps = machineProfile_->targetRepsPerSet;
    const float baselineDefaultKg = machineProfile_->defaultCalibrationWeightKg;

    if (cloudConfig.idealRomPercent < 70.0f || cloudConfig.idealRomPercent > 100.0f) {
      cloudConfig.idealRomPercent = baselineIdealRom;
    }
    if (cloudConfig.strokeLengthMm < 200.0f || cloudConfig.strokeLengthMm > 3000.0f) {
      cloudConfig.strokeLengthMm = baselineStrokeMm;
    }
    if (cloudConfig.targetRepsPerSet == 0 || cloudConfig.targetRepsPerSet > 40) {
      cloudConfig.targetRepsPerSet = baselineTargetReps;
    }
    if (cloudConfig.defaultCalibrationWeightKg < kMinWeightKg ||
        cloudConfig.defaultCalibrationWeightKg > kMaxWeightKg) {
      cloudConfig.defaultCalibrationWeightKg = baselineDefaultKg;
    }

    if (machineRegistry_.applyCloudConfig(cloudConfig)) {
      logEvent("CFG",
               String("cloud machine catalog loaded version=") + String(cloudConfig.version) +
                   " machineId=" + cloudConfig.machineId,
               LogLevel::Normal);
      machineProfile_ = machineRegistry_.findById(machineProfile_->machineId);
      if (machineProfile_ != nullptr) {
        if (selectedWeightKg_ <= 0.0f && !sessionRecorder_.isActive() && state_ != State::Training) {
          refreshActiveCalibration();
        } else {
          logEvent("WEIGHT",
                   String("machine pin preserved kg=") + String(selectedWeightKg_, 1) +
                       " reason=cloud_machine_config",
                   LogLevel::Normal);
        }
        applyMachineSensorCalibration();
      }
    }
  } else {
    logEvent("CFG",
             String("cloud machine catalog unavailable, using embedded fallback machineId=") +
                 machineProfile_->machineId +
                 " http=" + String(firebaseService_.getLastHttpStatusCode()) +
                 " error=" + firebaseService_.getLastErrorSummary(),
             LogLevel::Normal);
  }
}

bool SmartGymTouchApp::tryResplitQueuedRepSetPatch(const PendingUploadRecord& upload,
                                                   UploadMemoryMode mode) {
  if (!upload.method.equalsIgnoreCase("PATCH") ||
      upload.path.indexOf("/repSets/set") < 0 ||
      upload.payload.isEmpty()) {
    return false;
  }

  uint16_t rescueTargetBytes = kFirebaseClientRepSetsSplitPatchTargetBytesConstrained;
  uint16_t rescueHardMaxBytes = kFirebaseClientRepSetsSplitPatchHardMaxBytesConstrained;
  uint8_t rescueMaxRepsPerPatch = 2;
  const char* modeText = "constrained";
  if (mode == UploadMemoryMode::Critical) {
    rescueTargetBytes = 500U;
    rescueHardMaxBytes = 700U;
    rescueMaxRepsPerPatch = 1;
    modeText = "critical";
  } else if (mode == UploadMemoryMode::Normal) {
    rescueTargetBytes = 900U;
    rescueHardMaxBytes = 1100U;
    rescueMaxRepsPerPatch = 2;
    modeText = "normal_rescue";
  }

  String keys[SessionHistoryRecord::kMaxReps];
  String values[SessionHistoryRecord::kMaxReps];
  size_t fieldCount = 0;
  if (!splitTopLevelJsonObject(upload.payload,
                               keys,
                               values,
                               SessionHistoryRecord::kMaxReps,
                               fieldCount) ||
      fieldCount <= 1) {
    return false;
  }

  String chunks[kMaxQueueCountAfterWebAppDetails];
  uint8_t chunkCount = 0;
  String patch = "{";
  uint8_t patchRepCount = 0;

  const auto flushPatch = [&]() -> bool {
    if (patchRepCount == 0) {
      return true;
    }
    patch += "}";
    if (patch.length() > rescueHardMaxBytes || chunkCount >= kMaxQueueCountAfterWebAppDetails) {
      return false;
    }
    chunks[chunkCount++] = patch;
    patch = "{";
    patchRepCount = 0;
    return true;
  };

  for (size_t i = 0; i < fieldCount; ++i) {
    const String entry = "\"" + jsonEscape(keys[i]) + "\":" + values[i];
    if ((entry.length() + 2U) > rescueHardMaxBytes) {
      return false;
    }
    const size_t projectedBytes = patch.length() + (patchRepCount > 0 ? 1U : 0U) + entry.length() + 1U;
    if (patchRepCount > 0 &&
        (projectedBytes > rescueTargetBytes || patchRepCount >= rescueMaxRepsPerPatch)) {
      if (!flushPatch()) {
        return false;
      }
    }
    if (patchRepCount > 0) {
      patch += ",";
    }
    patch += entry;
    patchRepCount++;
  }
  if (!flushPatch() || chunkCount <= 1) {
    return false;
  }

  logEvent("SYNC",
           String("repSet split limits mode=") + modeText +
               " targetBytes=" + String(rescueTargetBytes) +
               " hardMaxBytes=" + String(rescueHardMaxBytes) +
               " maxRepsPerPatch=" + String(rescueMaxRepsPerPatch),
           LogLevel::Normal);
  logEvent("SYNC",
           String("queue item resplit reason=repeated_heap_delay payloadBytes=") +
               String(upload.payload.length()),
           LogLevel::Normal);
  logEvent("SYNC",
           String("repSet patch split reason=constrained_heap oldPayloadBytes=") +
               String(upload.payload.length()) +
               " newChunks=" + String(chunkCount),
           LogLevel::Normal);

  if (!localPersistenceStore_.dropOldestUpload()) {
    return false;
  }
  for (uint8_t i = 0; i < chunkCount; ++i) {
    if (!localPersistenceStore_.enqueueUpload(upload.path, chunks[i], "PATCH")) {
      logEvent("SYNC",
               String("field split paused reason=queue_capacity queueCount=") +
                   String(localPersistenceStore_.getPendingUploadCount()) +
                   " needed=" + String(chunkCount - i),
               LogLevel::Normal);
      return false;
    }
  }
  return true;
}

bool SmartGymTouchApp::retryPendingUploads(uint32_t nowMs, bool force) {
  if (!cloudEnabled_ || !firebaseService_.isWifiConnected()) {
    return false;
  }

  if (uploadRetryHoldoffUntilMs_ != 0 && nowMs < uploadRetryHoldoffUntilMs_) {
    return false;
  }

  PendingUploadRecord peek;
  uint32_t dynamicBackoffMs = kUploadRetryIntervalMs;
  if (localPersistenceStore_.peekUpload(peek)) {
    if (peek.attempts == 0) {
      const SessionStage stage = getSessionStage(nowMs);
      if (stage == SessionStage::Train || stage == SessionStage::Rest) {
        dynamicBackoffMs = kUploadRetryIntervalMs;
      } else if (stage == SessionStage::Summary) {
        dynamicBackoffMs = kSummaryUploadUiSmoothCadenceMs;
      } else if (stage == SessionStage::Idle || stage == SessionStage::Identify) {
        dynamicBackoffMs = kReadyUploadFastCadenceMs;
      } else {
        dynamicBackoffMs = kUploadSuccessCadenceMs;
      }
    } else if (peek.attempts == 1) {
      dynamicBackoffMs = kUploadFailureCooldown1Ms;
    } else if (peek.attempts == 2) {
      dynamicBackoffMs = kUploadFailureCooldown2Ms;
    } else {
      dynamicBackoffMs = kUploadFailureCooldown3Ms;
    }
  } else {
    uploadSuccessStreak_ = 0;
  }
  if (!force && lastUploadRetryMs_ != 0 && (nowMs - lastUploadRetryMs_) < dynamicBackoffMs) {
    return false;
  }

  PendingUploadRecord upload;
  if (!localPersistenceStore_.peekUpload(upload)) {
    return false;
  }
  const bool fromUploadWorker =
      kUseUploadTransportWorker &&
      uploadTransportTaskHandle_ != nullptr &&
      xTaskGetCurrentTaskHandle() == uploadTransportTaskHandle_;
  const uint32_t freeHeap = ESP.getFreeHeap();
  uint32_t internalHeap = internalFree8BitHeap();
  uint32_t largestInternalBlock = internalLargestFree8BitBlock();
  UploadMemoryMode uploadMemoryMode = getUploadMemoryMode(internalHeap, largestInternalBlock);
  if (uploadMemoryMode != lastUploadMemoryMode_ ||
      lastUploadMemoryModeLogMs_ == 0 ||
      (nowMs - lastUploadMemoryModeLogMs_) >= 5000UL) {
    lastUploadMemoryMode_ = uploadMemoryMode;
    lastUploadMemoryModeLogMs_ = nowMs;
    logEvent("SYNC",
             String("upload memory mode=") + uploadMemoryModeText(uploadMemoryMode) +
                 " internalHeap=" + String(internalHeap) +
                 " largestBlock=" + String(largestInternalBlock),
             LogLevel::Normal);
  }
  if (uploadMemoryMode != UploadMemoryMode::Normal && !lowMemoryUploadUiFreed_) {
    freeOptionalUiForMemory("low_memory_upload", true);
    lowMemoryUploadUiFreed_ = true;
  }
  const String retryBeginMsg = String("retry begin method=") + upload.method +
                               " queueCount=" + String(localPersistenceStore_.getPendingUploadCount()) +
                               " path=" + upload.path +
                               " payloadBytes=" + String(upload.payload.length()) +
                               " attempts=" + String(upload.attempts) +
                               " heap=" + String(freeHeap) +
                               " internalHeap=" + String(internalHeap) +
                               " largestInternalBlock=" + String(largestInternalBlock);
  if (fromUploadWorker) {
    Serial.printf("[SYNC][worker] %s\n", retryBeginMsg.c_str());
  } else {
    logEvent("SYNC", retryBeginMsg, LogLevel::Normal);
  }

  const bool writeRequest = upload.method.equalsIgnoreCase("PUT") || upload.method.equalsIgnoreCase("PATCH");
  const size_t payloadBytes = upload.payload.length();
  const uint16_t queueCountNow = localPersistenceStore_.getPendingUploadCount();
  const bool firebaseClientGuardMode = kUseFirebaseClientQueuedTransport;
  const bool isSessionRootPath = upload.path.indexOf("/sessions/") >= 0 &&
                                 !upload.path.endsWith("/setDetails") &&
                                 upload.path.indexOf("/repSets/") < 0 &&
                                 upload.path.indexOf("/representativeReps") < 0;
  const auto handleRepeatedHeapDelay = [&](UploadMemoryMode delayMode) -> bool {
    const uint16_t trackedPayloadBytes =
        static_cast<uint16_t>(payloadBytes > 0xFFFFU ? 0xFFFFU : payloadBytes);
    const bool sameBlockedItem =
        lastHeapDelayPath_.equals(upload.path) &&
        lastHeapDelayPayloadBytes_ == trackedPayloadBytes;
    repeatedHeapDelayCount_ = sameBlockedItem ? static_cast<uint8_t>(min<int>(repeatedHeapDelayCount_ + 1, 255)) : 1;
    lastHeapDelayPath_ = upload.path;
    lastHeapDelayPayloadBytes_ = trackedPayloadBytes;
    logEvent("SYNC",
             String("heap delay repeated count=") + String(repeatedHeapDelayCount_) +
                 " path=" + upload.path +
                 " payloadBytes=" + String(payloadBytes),
             LogLevel::Normal);
    if (repeatedHeapDelayCount_ >= 2 && !lowMemoryUploadUiFreed_) {
      logEvent("SYNC",
               "fallback reason=repeated_heap_delay action=free_optional_ui",
               LogLevel::Normal);
      freeOptionalUiForMemory("low_memory_upload", true);
      lowMemoryUploadUiFreed_ = true;
    }
    if (repeatedHeapDelayCount_ >= 3 &&
        queueCountNow == 1 &&
        upload.method.equalsIgnoreCase("PATCH") &&
        upload.path.indexOf("/repSets/set") >= 0) {
      logEvent("SYNC",
               "fallback reason=repeated_heap_delay action=resplit_smaller",
               LogLevel::Normal);
      if (tryResplitQueuedRepSetPatch(upload, delayMode)) {
        repeatedHeapDelayCount_ = 0;
        lastHeapDelayPath_.remove(0);
        lastHeapDelayPayloadBytes_ = 0;
        return true;
      }
    }
    return false;
  };
  uint32_t minInternalHeapForUpload = 44000UL;
  uint32_t minLargestInternalBlockForUpload = 24000UL;
  const char* guardBucket = "default";
  bool hardBlockedByMode = false;
  const char* hardBlockReason = nullptr;
  if (writeRequest) {
    if (firebaseClientGuardMode) {
      if (uploadMemoryMode == UploadMemoryMode::Normal) {
        if (payloadBytes <= 1200U) {
          minInternalHeapForUpload = 43000UL;
          minLargestInternalBlockForUpload = 22000UL;
          guardBucket = "<=1200";
        } else if (payloadBytes <= 2200U) {
          minInternalHeapForUpload = 50000UL;
          minLargestInternalBlockForUpload = 24000UL;
          guardBucket = "1201..2200";
        } else if (payloadBytes <= 3000U) {
          minInternalHeapForUpload = 52000UL;
          minLargestInternalBlockForUpload = 26000UL;
          guardBucket = "2201..3000";
        } else if (payloadBytes <= 6200U) {
          minInternalHeapForUpload = 56000UL;
          minLargestInternalBlockForUpload = 30000UL;
          guardBucket = "3001..6200";
        } else {
          minInternalHeapForUpload = 58000UL;
          minLargestInternalBlockForUpload = 34000UL;
          guardBucket = ">6200";
        }
      } else if (uploadMemoryMode == UploadMemoryMode::Constrained) {
        if (payloadBytes <= 700U) {
          minInternalHeapForUpload = 22000UL;
          minLargestInternalBlockForUpload = 7000UL;
          guardBucket = "<=700(constrained)";
        } else if (payloadBytes <= 1200U) {
          minInternalHeapForUpload = 23000UL;
          minLargestInternalBlockForUpload = 9000UL;
          guardBucket = "701..1200(constrained)";
        } else {
          hardBlockedByMode = true;
          hardBlockReason = "constrained_payload_gt_1200";
          minInternalHeapForUpload = 60000UL;
          minLargestInternalBlockForUpload = 36000UL;
          guardBucket = ">1200(blocked)";
        }
      } else {
        if (payloadBytes <= 700U) {
          minInternalHeapForUpload = 20000UL;
          minLargestInternalBlockForUpload = 6000UL;
          guardBucket = "<=700(critical)";
        } else {
          hardBlockedByMode = true;
          hardBlockReason = "critical_payload_gt_700";
          minInternalHeapForUpload = 60000UL;
          minLargestInternalBlockForUpload = 36000UL;
          guardBucket = ">700(blocked)";
        }
      }
    } else {
      if (payloadBytes <= 700U) {
        minInternalHeapForUpload = 44000UL;
        minLargestInternalBlockForUpload = 24000UL;
        guardBucket = "<=700";
      } else if (payloadBytes <= 1100U || (isSessionRootPath && payloadBytes > 900U)) {
        minInternalHeapForUpload = 50000UL;
        minLargestInternalBlockForUpload = 30000UL;
        guardBucket = "701..1100";
      } else {
        minInternalHeapForUpload = 56000UL;
        minLargestInternalBlockForUpload = 34000UL;
        guardBucket = ">1100";
      }
    }
  }
  logEvent("SYNC",
           String("queue guard mode=") + (firebaseClientGuardMode ? "firebaseclient" : "legacy") +
               " bucket=" + guardBucket +
               " payloadBytes=" + String(payloadBytes) +
               " minInternalHeap=" + String(minInternalHeapForUpload) +
               " minLargestBlock=" + String(minLargestInternalBlockForUpload) +
               " actualInternalHeap=" + String(internalHeap) +
               " actualLargestBlock=" + String(largestInternalBlock),
           LogLevel::Normal);
  if (firebaseClientGuardMode &&
      writeRequest &&
      payloadBytes > 1200U &&
      largestInternalBlock < 30000UL &&
      largestInternalBlock >= minLargestInternalBlockForUpload &&
      internalHeap >= minInternalHeapForUpload) {
    logEvent("SYNC",
             String("queue guard relaxed mode=firebaseclient reason=read_write_memory_state") +
                 " payloadBytes=" + String(payloadBytes) +
                 " bucket=" + guardBucket +
                 " actualInternalHeap=" + String(internalHeap) +
                 " actualLargestBlock=" + String(largestInternalBlock),
             LogLevel::Normal);
  }
  if (hardBlockedByMode ||
      internalHeap < minInternalHeapForUpload ||
      largestInternalBlock < minLargestInternalBlockForUpload) {
    const uint32_t backoffMs = kUploadLowMemoryBackoffMs;
    const String lowHeapMsg =
        String("queue upload delayed reason=internal_heap heap=") + String(freeHeap) +
        " internalHeap=" + String(internalHeap) +
        " largestInternalBlock=" + String(largestInternalBlock) +
        " thresholdInternalHeap=" + String(minInternalHeapForUpload) +
        " thresholdBlock=" + String(minLargestInternalBlockForUpload) +
        " method=" + upload.method +
        " payloadBytes=" + String(payloadBytes) +
        " backoffMs=" + String(backoffMs) +
        (hardBlockedByMode ? String(" modeBlock=") + String(hardBlockReason != nullptr ? hardBlockReason : "1")
                           : String(""));
    if (fromUploadWorker) {
      Serial.printf("[SYNC][worker] %s\n", lowHeapMsg.c_str());
    } else {
      logEvent("SYNC", lowHeapMsg, LogLevel::Normal);
    }
    if (handleRepeatedHeapDelay(uploadMemoryMode)) {
      lastUploadRetryMs_ = nowMs;
      uploadRetryHoldoffUntilMs_ = nowMs + backoffMs;
      return false;
    }
    lastUploadRetryMs_ = nowMs;
    uploadRetryHoldoffUntilMs_ = nowMs + backoffMs;
    return false;
  }

  if (!force && isSessionRootPath && payloadBytes > 900U &&
      lastUploadRetryMs_ != 0 && (nowMs - lastUploadRetryMs_) < 900UL) {
    uploadRetryHoldoffUntilMs_ = nowMs + 750UL;
    logEvent("SYNC",
             String("queue upload delayed reason=root_settle payloadBytes=") + String(payloadBytes) +
                  " heap=" + String(freeHeap) +
                  " internalHeap=" + String(internalHeap) +
                  " largestInternalBlock=" + String(largestInternalBlock),
             LogLevel::Normal);
    return false;
  }

  const bool optionalPath = upload.path.endsWith("/uploadManifest");
  if (optionalPath && upload.attempts >= 2) {
    logEvent("SYNC",
             String("queue drop path=") + upload.path +
                 " reason=optional attempts=" + String(upload.attempts) +
                 " payloadBytes=" + String(upload.payload.length()) +
                 " queueCount=" + String(localPersistenceStore_.getPendingUploadCount()),
             LogLevel::Normal);
    localPersistenceStore_.dropOldestUpload();
    return false;
  }

  const uint32_t ageMs = (upload.enqueuedAtMs == 0 || nowMs < upload.enqueuedAtMs)
                             ? 0
                             : (nowMs - upload.enqueuedAtMs);
  if (upload.attempts >= kUploadRetryMaxAttempts || ageMs > kUploadStaleDropMs) {
    logEvent("SYNC",
             String("queue drop path=") + upload.path +
                 " reason=retry_limit ageMs=" + String(ageMs) +
                 " attempts=" + String(upload.attempts) +
                 " payloadBytes=" + String(upload.payload.length()) +
                 " queueCount=" + String(localPersistenceStore_.getPendingUploadCount()),
             LogLevel::Normal);
    localPersistenceStore_.dropOldestUpload();
    return false;
  }

  const bool isPatch = upload.method.equalsIgnoreCase("PATCH");
  if (state_ == State::Summary || currentUiScreen_ == UiScreenMode::Summary) {
    if (upload.path == pendingWebRootPath_) {
      summaryUploadStageText_ = "Uploading root...";
    } else if (!pendingWebCoreBundlePath_.isEmpty() && upload.path == pendingWebCoreBundlePath_) {
      summaryUploadStageText_ = "Uploading core...";
    } else if (upload.path.endsWith("/meta") ||
               upload.path.endsWith("/daySummary") ||
               upload.path.indexOf("/timeline/") >= 0 ||
               upload.path.endsWith("/weekSummary")) {
      summaryUploadStageText_ = "Uploading core...";
    } else if (upload.path.indexOf("/setDetails") >= 0 ||
               upload.path.indexOf("/representativeReps") >= 0) {
      summaryUploadStageText_ = "Uploading details...";
    } else if (upload.path.indexOf("/repSets/") >= 0) {
      summaryUploadStageText_ = "Uploading reps...";
    } else {
      summaryUploadStageText_ = "Saving session...";
    }
  }
  lastUploadRetryMs_ = nowMs;
  const uint32_t requestStartMs = millis();
  if (kSyncRequestTimingTrace) {
    const String requestBeginMsg = String("request begin method=") + upload.method +
                                   " payloadBytes=" + String(payloadBytes) +
                                   " heap=" + String(ESP.getFreeHeap()) +
                                   " internalHeap=" + String(internalFree8BitHeap()) +
                                   " largestInternalBlock=" + String(internalLargestFree8BitBlock());
    if (fromUploadWorker) {
      Serial.printf("[SYNC][worker] %s\n", requestBeginMsg.c_str());
    } else {
      logEvent("SYNC", requestBeginMsg, LogLevel::Normal);
    }
  }
  if (fromUploadWorker) {
    const UBaseType_t stackWordsBefore = uxTaskGetStackHighWaterMark(nullptr);
    Serial.printf("[SYNC][worker] stackWordsBefore=%u heap=%lu\n",
                  static_cast<unsigned>(stackWordsBefore),
                  static_cast<unsigned long>(ESP.getFreeHeap()));
  }
  const bool useFirebaseClientQueuedTransport = kUseFirebaseClientQueuedTransport;
  if (useFirebaseClientQueuedTransport && writeRequest) {
    uint32_t sendInternalHeap = internalFree8BitHeap();
    uint32_t sendLargestBlock = internalLargestFree8BitBlock();
    UploadMemoryMode sendMemoryMode = getUploadMemoryMode(sendInternalHeap, sendLargestBlock);
    uint32_t sendMinInternalHeap = 44000UL;
    uint32_t sendMinLargestBlock = 24000UL;
    const char* sendGuardBucket = "default";
    bool sendHardBlockedByMode = false;
    const char* sendBlockReason = nullptr;
    if (sendMemoryMode == UploadMemoryMode::Normal) {
      if (payloadBytes <= 1200U) {
        sendMinInternalHeap = 43000UL;
        sendMinLargestBlock = 22000UL;
        sendGuardBucket = "<=1200";
      } else if (payloadBytes <= 2200U) {
        sendMinInternalHeap = 50000UL;
        sendMinLargestBlock = 24000UL;
        sendGuardBucket = "1201..2200";
      } else if (payloadBytes <= 3000U) {
        sendMinInternalHeap = 52000UL;
        sendMinLargestBlock = 26000UL;
        sendGuardBucket = "2201..3000";
      } else if (payloadBytes <= 6200U) {
        sendMinInternalHeap = 56000UL;
        sendMinLargestBlock = 30000UL;
        sendGuardBucket = "3001..6200";
      } else {
        sendMinInternalHeap = 58000UL;
        sendMinLargestBlock = 34000UL;
        sendGuardBucket = ">6200";
      }
    } else if (sendMemoryMode == UploadMemoryMode::Constrained) {
      if (payloadBytes <= 700U) {
        sendMinInternalHeap = 22000UL;
        sendMinLargestBlock = 7000UL;
        sendGuardBucket = "<=700(constrained)";
      } else if (payloadBytes <= 1200U) {
        sendMinInternalHeap = 23000UL;
        sendMinLargestBlock = 9000UL;
        sendGuardBucket = "701..1200(constrained)";
      } else {
        sendHardBlockedByMode = true;
        sendBlockReason = "constrained_payload_gt_1200";
        sendMinInternalHeap = 60000UL;
        sendMinLargestBlock = 36000UL;
        sendGuardBucket = ">1200(blocked)";
      }
    } else {
      if (payloadBytes <= 700U) {
        sendMinInternalHeap = 20000UL;
        sendMinLargestBlock = 6000UL;
        sendGuardBucket = "<=700(critical)";
      } else {
        sendHardBlockedByMode = true;
        sendBlockReason = "critical_payload_gt_700";
        sendMinInternalHeap = 60000UL;
        sendMinLargestBlock = 36000UL;
        sendGuardBucket = ">700(blocked)";
      }
    }
    if (sendHardBlockedByMode ||
        sendInternalHeap < sendMinInternalHeap ||
        sendLargestBlock < sendMinLargestBlock) {
      const uint32_t backoffMs = kUploadLowMemoryBackoffMs;
      const String sendLowHeapMsg =
          String("queue upload delayed reason=internal_heap heap=") + String(sendInternalHeap) +
          " internalHeap=" + String(sendInternalHeap) +
          " largestInternalBlock=" + String(sendLargestBlock) +
          " thresholdInternalHeap=" + String(sendMinInternalHeap) +
          " thresholdBlock=" + String(sendMinLargestBlock) +
          " method=" + upload.method +
          " payloadBytes=" + String(payloadBytes) +
          " backoffMs=" + String(backoffMs) +
          " stage=send" +
          (sendHardBlockedByMode ? String(" modeBlock=") + String(sendBlockReason != nullptr ? sendBlockReason : "1")
                                 : String(""));
      if (fromUploadWorker) {
        Serial.printf("[SYNC][worker] %s\n", sendLowHeapMsg.c_str());
      } else {
        logEvent("SYNC", sendLowHeapMsg, LogLevel::Normal);
      }
      if (handleRepeatedHeapDelay(sendMemoryMode)) {
        lastUploadRetryMs_ = nowMs;
        uploadRetryHoldoffUntilMs_ = nowMs + backoffMs;
        return false;
      }
      lastUploadRetryMs_ = nowMs;
      uploadRetryHoldoffUntilMs_ = nowMs + backoffMs;
      return false;
    }
  }
  if (useFirebaseClientQueuedTransport) {
    Serial.printf("[SYNC][fc] begin method=%s payloadBytes=%u heap=%lu internalHeap=%lu largestBlock=%lu\n",
                  upload.method.c_str(),
                  static_cast<unsigned>(payloadBytes),
                  static_cast<unsigned long>(ESP.getFreeHeap()),
                  static_cast<unsigned long>(internalFree8BitHeap()),
                  static_cast<unsigned long>(internalLargestFree8BitBlock()));
  }
  const bool sent = useFirebaseClientQueuedTransport
                        ? (isPatch ? firebaseService_.patchPathJsonQueued(upload.path, upload.payload)
                                   : firebaseService_.putPathJsonQueued(upload.path, upload.payload))
                        : (isPatch ? firebaseService_.patchPathJson(upload.path, upload.payload)
                                   : firebaseService_.putPathJson(upload.path, upload.payload));
  if (fromUploadWorker) {
    const UBaseType_t stackWordsAfter = uxTaskGetStackHighWaterMark(nullptr);
    Serial.printf("[SYNC][worker] stackWordsAfter=%u heap=%lu\n",
                  static_cast<unsigned>(stackWordsAfter),
                  static_cast<unsigned long>(ESP.getFreeHeap()));
  }
  const uint32_t requestDurationMs = millis() - requestStartMs;
  if (requestDurationMs > kUploadLongCallWarnMs) {
    logEvent("SYNC",
             String("long upload call method=") + upload.method +
                 " durationMs=" + String(requestDurationMs) +
                 " payloadBytes=" + String(payloadBytes),
             LogLevel::Normal);
  }
  if (kSyncRequestTimingTrace) {
    const String requestEndMsg = String("request end method=") + upload.method +
                                 " http=" + String(firebaseService_.getLastHttpStatusCode()) +
                                 " durationMs=" + String(requestDurationMs) +
                                 " heap=" + String(ESP.getFreeHeap()) +
                                 " internalHeap=" + String(internalFree8BitHeap()) +
                                 " largestInternalBlock=" + String(internalLargestFree8BitBlock());
    if (fromUploadWorker) {
      Serial.printf("[SYNC][worker] %s\n", requestEndMsg.c_str());
    } else {
      logEvent("SYNC", requestEndMsg, LogLevel::Normal);
    }
  }
  if (useFirebaseClientQueuedTransport) {
    Serial.printf("[SYNC][fc] end method=%s ok=%d durationMs=%lu error=%s heap=%lu internalHeap=%lu largestBlock=%lu\n",
                  upload.method.c_str(),
                  sent ? 1 : 0,
                  static_cast<unsigned long>(requestDurationMs),
                  firebaseService_.getLastErrorSummary().c_str(),
                  static_cast<unsigned long>(ESP.getFreeHeap()),
                  static_cast<unsigned long>(internalFree8BitHeap()),
                  static_cast<unsigned long>(internalLargestFree8BitBlock()));
  }
  if (!sent) {
    logEvent("SYNC",
             String("request failed method=") + upload.method +
                  " http=" + String(firebaseService_.getLastHttpStatusCode()) +
                  " heap=" + String(ESP.getFreeHeap()) +
                  " internalHeap=" + String(internalFree8BitHeap()) +
                  " largestInternalBlock=" + String(internalLargestFree8BitBlock()) +
                  " error=" + firebaseService_.getLastErrorSummary(),
             LogLevel::Normal);
    uploadSuccessStreak_ = 0;
    const uint8_t nextAttempts = static_cast<uint8_t>(upload.attempts + 1);
    uint32_t nextBackoffMs = kUploadFailureCooldown3Ms;
    if (nextAttempts <= 1) {
      nextBackoffMs = kUploadFailureCooldown1Ms;
    } else if (nextAttempts == 2) {
      nextBackoffMs = kUploadFailureCooldown2Ms;
    }
    uploadRetryHoldoffUntilMs_ = nowMs + nextBackoffMs;
    const String failMsg = String("queue ") + (isPatch ? "PATCH" : "PUT") + " failed path=" + upload.path +
                           " http=" + String(firebaseService_.getLastHttpStatusCode()) +
                           " error=" + firebaseService_.getLastErrorSummary() +
                           " queueCount=" + String(localPersistenceStore_.getPendingUploadCount()) +
                           " payloadBytes=" + String(upload.payload.length()) +
                           " attempts=" + String(upload.attempts) +
                           " nextBackoffMs=" + String(nextBackoffMs) +
                           " kept=1";
    if (fromUploadWorker) {
      Serial.printf("[SYNC][worker] %s\n", failMsg.c_str());
    } else {
      logEvent("SYNC", failMsg, LogLevel::Normal);
    }
    localPersistenceStore_.bumpOldestUploadAttempt();
    return true;
  }

  if (upload.method.equalsIgnoreCase("PUT") && upload.path == pendingWebRootPath_) {
    pendingWebRootUploaded_ = true;
  }
  if (pendingWebCoreBundleRequired_ &&
      !pendingWebCoreBundleUploaded_ &&
      upload.method.equalsIgnoreCase("PATCH") &&
      upload.path == pendingWebCoreBundlePath_) {
    pendingWebCoreBundleUploaded_ = true;
    logEvent("SYNC",
             String("core bundle upload ok path=") + upload.path +
                 " payloadBytes=" + String(pendingWebCoreBundlePayloadBytes_),
             LogLevel::Normal);
  }

  repeatedHeapDelayCount_ = 0;
  lastHeapDelayPath_.remove(0);
  lastHeapDelayPayloadBytes_ = 0;
  localPersistenceStore_.dropOldestUpload();
  uploadSuccessStreak_ = static_cast<uint8_t>(uploadSuccessStreak_ + 1);
  const SessionStage successStage = getSessionStage(nowMs);
  uint32_t successCadenceMs = kUploadSuccessCadenceMs;
  if (successStage == SessionStage::Train || successStage == SessionStage::Rest) {
    successCadenceMs = kUploadRetryIntervalMs;
  } else if (successStage == SessionStage::Summary) {
    successCadenceMs = kSummaryUploadUiSmoothCadenceMs;
  } else if (successStage == SessionStage::Idle || successStage == SessionStage::Identify) {
    successCadenceMs = kReadyUploadFastCadenceMs;
  }
  uploadRetryHoldoffUntilMs_ = nowMs + successCadenceMs;
  if (!force && uploadSuccessStreak_ >= kUploadStreakCooldownCount) {
    uploadSuccessStreak_ = 0;
    uploadRetryHoldoffUntilMs_ = nowMs + kUploadStreakCooldownMs;
    logEvent("SYNC",
             String("upload cooldown reason=request_streak count=") +
                 String(kUploadStreakCooldownCount) +
                 " delayMs=" + String(kUploadStreakCooldownMs),
             LogLevel::Normal);
  }
  const String okMsg = String("queue ") + (isPatch ? "PATCH" : "PUT") + " ok path=" + upload.path +
                       " http=" + String(firebaseService_.getLastHttpStatusCode()) +
                       " remaining=" + String(localPersistenceStore_.getPendingUploadCount());
  if (fromUploadWorker) {
    Serial.printf("[SYNC][worker] %s\n", okMsg.c_str());
  } else {
    logEvent("SYNC", okMsg, LogLevel::Normal);
  }
  if (localPersistenceStore_.getPendingUploadCount() == 0) {
    uploadSuccessStreak_ = 0;
  }
  return true;
}

bool SmartGymTouchApp::syncDeviceIdentityToCloud(uint32_t nowMs, bool force) {
  if (!cloudEnabled_ || machineProfile_ == nullptr) {
    return false;
  }
  if (!force && lastWebUploadPhaseCompleteMs_ != 0 &&
      (nowMs - lastWebUploadPhaseCompleteMs_) < kCloudReadPostUploadCooldownMs) {
    logEvent("CloudWrite",
             "skipped reason=post_upload_cooldown kind=device_heartbeat",
             LogLevel::Normal);
    return false;
  }
  CloudLockGuard cloudLock(cloudMutex_, force ? 80 : 0);
  if (!cloudLock) {
    requestCloudSync(force);
    return false;
  }

  if (!force && lastDeviceHeartbeatMs_ != 0 &&
      (nowMs - lastDeviceHeartbeatMs_) < kDeviceHeartbeatIntervalMs) {
    if (lastHeartbeatSkipLogMs_ == 0 || (nowMs - lastHeartbeatSkipLogMs_) >= 5000UL) {
      lastHeartbeatSkipLogMs_ = nowMs;
      logEvent("CloudWrite",
               String("skipped reason=heartbeat_interval kind=device_heartbeat elapsedMs=") +
                   String(nowMs - lastDeviceHeartbeatMs_),
               LogLevel::Normal);
    }
    return false;
  }

  const String activeUid = anonymousMode_ || activeUser_ == nullptr ? String("") : activeUser_->rfidUid;
  lastDeviceHeartbeatMs_ = nowMs;
  firebaseService_.pushDeviceHeartbeat(machineProfile_->machineId,
                                       machineProfile_->machineTypeId,
                                       machineProfile_->displayName,
                                       stateToText(),
                                       activeUid,
                                       deviceEncoderCalibrationValid_,
                                       deviceEncoderZeroRaw_,
                                       deviceEncoderFullRaw_,
                                       encoderReferenceDistanceMm_,
                                       deviceEncoderInvertDirection_);
  return true;
}

void SmartGymTouchApp::syncActiveUserToCloud() {
  if (!cloudEnabled_ || activeUser_ == nullptr) {
    return;
  }
  if (!activeUserProfileDirty_) {
    logEvent("PROFILE",
             String("write skipped reason=not_dirty uid=") + activeUser_->rfidUid,
             LogLevel::Normal);
    return;
  }
  const String writeReason = activeUserProfileDirtyReason_.isEmpty()
                                 ? String("explicit_edit")
                                 : activeUserProfileDirtyReason_;
  if (state_ == State::Calibration) {
    logEvent("CloudWrite",
             "skipped reason=calibration_active kind=profile_write",
             LogLevel::Normal);
    return;
  }
  {
    const uint32_t nowMs = millis();
    if (isSessionUploadBusy(nowMs, nullptr, nullptr)) {
      logEvent("CloudWrite",
               "skipped reason=upload_active kind=profile_write",
               LogLevel::Normal);
      return;
    }
  }
  const auto enqueueProfileWriteOnce = [&]() {
    const String path = "usersByRfid/" + activeUser_->rfidUid;
    const String payload = firebaseService_.buildUserProfileJson(*activeUser_);
    const uint32_t nowMs = millis();
    if (path == lastQueuedProfileWritePath_ &&
        payload == lastQueuedProfileWritePayload_ &&
        lastQueuedProfileWriteMs_ != 0 &&
        (nowMs - lastQueuedProfileWriteMs_) < kProfileWriteDedupWindowMs) {
      logEvent("CloudWrite",
               String("skipped reason=duplicate_pending kind=profile_write path=") + path,
               LogLevel::Normal);
      return;
    }
    if (localPersistenceStore_.enqueueUpload(path, payload)) {
      lastQueuedProfileWritePath_ = path;
      lastQueuedProfileWritePayload_ = payload;
      lastQueuedProfileWriteMs_ = nowMs;
      activeUserProfileDirty_ = false;
      activeUserProfileDirtyReason_ = "";
    }
  };
  if (!firebaseService_.isWifiConnected()) {
    enqueueProfileWriteOnce();
    requestCloudSync(true);
    return;
  }

  if (activeUser_->updatedAtEpoch == 0) {
    activeUser_->updatedAtEpoch = firebaseService_.getCurrentEpoch();
    activeUser_->updatedAtIso = firebaseService_.getCurrentIso();
  }
  if (writeReason != "explicit_edit" &&
      lastCloudProfileFetchedEpoch_ != 0 &&
      activeUser_->updatedAtEpoch <= lastCloudProfileFetchedEpoch_) {
    logEvent("PROFILE",
             String("write skipped reason=cloud_fresh uid=") + activeUser_->rfidUid,
             LogLevel::Normal);
    activeUserProfileDirty_ = false;
    activeUserProfileDirtyReason_ = "";
    return;
  }

  CloudLockGuard cloudLock(cloudMutex_, 0);
  if (!cloudLock) {
    enqueueProfileWriteOnce();
    requestCloudSync(true);
    return;
  }
  if (writeReason == "recommendation_compat") {
    UserProfile cloudProfile;
    if (firebaseService_.fetchUserProfile(activeUser_->rfidUid, cloudProfile) &&
        !cloudProfile.rfidUid.isEmpty()) {
      if (!cloudProfile.displayName.isEmpty() && cloudProfile.displayName != activeUser_->displayName) {
        logEvent("PROFILE",
                 String("merge preserving cloud displayName=") + cloudProfile.displayName +
                     " preferredGoal=" + UserRegistry::goalToString(cloudProfile.goal),
                 LogLevel::Normal);
        activeUser_->displayName = cloudProfile.displayName;
        activeUser_->goal = cloudProfile.goal;
      }
    }
  }
  logEvent("PROFILE",
           String("write begin reason=") + writeReason + " uid=" + activeUser_->rfidUid,
           LogLevel::Normal);

  if (!firebaseService_.pushUserProfile(*activeUser_)) {
    enqueueProfileWriteOnce();
    return;
  }
  activeUserProfileDirty_ = false;
  activeUserProfileDirtyReason_ = "";
}

void SmartGymTouchApp::syncActiveCalibrationToCloud() {
  if (!cloudEnabled_ || activeUser_ == nullptr || activeCalibration_ == nullptr) {
    return;
  }
  const String path = "calibrations/" + activeUser_->rfidUid + "/" + activeCalibration_->machineTypeId;
  const String payload = firebaseService_.buildCalibrationJson(*activeCalibration_);
  {
    const uint32_t nowMs = millis();
    if (isSessionUploadBusy(nowMs, nullptr, nullptr)) {
      logEvent("CloudWrite",
               "skipped reason=upload_active kind=calibration_recommendation",
               LogLevel::Normal);
      localPersistenceStore_.enqueueUpload(path, payload);
      requestCloudSync(true);
      return;
    }
  }
  if (!firebaseService_.isWifiConnected()) {
    localPersistenceStore_.enqueueUpload(path, payload);
    requestCloudSync(true);
    return;
  }

  if (activeCalibration_->updatedAtEpoch == 0) {
    activeCalibration_->updatedAtEpoch = firebaseService_.getCurrentEpoch();
    activeCalibration_->updatedAtIso = firebaseService_.getCurrentIso();
  }

  CloudLockGuard cloudLock(cloudMutex_, 0);
  if (!cloudLock) {
    localPersistenceStore_.enqueueUpload(path, payload);
    requestCloudSync(true);
    return;
  }

  Serial.printf("[CloudWrite][fc] begin kind=calibration_recommendation path=%s payloadBytes=%u heap=%lu internalHeap=%lu largestBlock=%lu\n",
                path.c_str(),
                static_cast<unsigned>(payload.length()),
                static_cast<unsigned long>(ESP.getFreeHeap()),
                static_cast<unsigned long>(internalFree8BitHeap()),
                static_cast<unsigned long>(internalLargestFree8BitBlock()));
  if (!firebaseService_.pushCalibration(activeUser_->rfidUid, *activeCalibration_)) {
    Serial.printf("[CloudWrite][fc] end kind=calibration_recommendation ok=0 error=%s heap=%lu internalHeap=%lu largestBlock=%lu\n",
                  firebaseService_.getLastErrorSummary().c_str(),
                  static_cast<unsigned long>(ESP.getFreeHeap()),
                  static_cast<unsigned long>(internalFree8BitHeap()),
                  static_cast<unsigned long>(internalLargestFree8BitBlock()));
    localPersistenceStore_.enqueueUpload(path, payload);
    requestCloudSync(true);
    return;
  }
  Serial.printf("[CloudWrite][fc] end kind=calibration_recommendation ok=1 error=%s heap=%lu internalHeap=%lu largestBlock=%lu\n",
                firebaseService_.getLastErrorSummary().c_str(),
                static_cast<unsigned long>(ESP.getFreeHeap()),
                static_cast<unsigned long>(internalFree8BitHeap()),
                static_cast<unsigned long>(internalLargestFree8BitBlock()));
}

bool SmartGymTouchApp::mergeActiveUserFromCloud(const UserProfile& cloudProfile) {
  if (activeUser_ == nullptr || !activeUser_->rfidUid.equalsIgnoreCase(cloudProfile.rfidUid)) {
    return false;
  }

  bool changed = false;
  // Apply meaningful cloud fields even when manual edits omitted updatedAtEpoch.
  if (!cloudProfile.displayName.isEmpty() && activeUser_->displayName != cloudProfile.displayName) {
    activeUser_->displayName = cloudProfile.displayName;
    changed = true;
  }
  if (cloudProfile.hasBasicData && !activeUser_->hasBasicData) {
    activeUser_->hasBasicData = true;
    changed = true;
  }
  if (cloudProfile.weightKg > 0.0f && fabsf(activeUser_->weightKg - cloudProfile.weightKg) > 0.001f) {
    activeUser_->weightKg = cloudProfile.weightKg;
    changed = true;
  }
  if (cloudProfile.age > 0 && activeUser_->age != cloudProfile.age) {
    activeUser_->age = cloudProfile.age;
    changed = true;
  }
  if (cloudProfile.heightCm > 0.0f && fabsf(activeUser_->heightCm - cloudProfile.heightCm) > 0.001f) {
    activeUser_->heightCm = cloudProfile.heightCm;
    changed = true;
  }
  if (activeUser_->goal != cloudProfile.goal) {
    activeUser_->goal = cloudProfile.goal;
    changed = true;
  }
  if (activeUser_->gender != cloudProfile.gender) {
    activeUser_->gender = cloudProfile.gender;
    changed = true;
  }
  activeUser_->updatedAtEpoch = max(activeUser_->updatedAtEpoch, cloudProfile.updatedAtEpoch);
  if (!cloudProfile.updatedAtIso.isEmpty()) {
    activeUser_->updatedAtIso = cloudProfile.updatedAtIso;
  }
  return changed;
}

bool SmartGymTouchApp::mergeActiveCalibrationFromCloud(const UserMachineCalibration& cloudCalibration) {
  if (activeUser_ == nullptr || machineProfile_ == nullptr ||
      cloudCalibration.machineTypeId.isEmpty() ||
      !cloudCalibration.machineTypeId.equalsIgnoreCase(machineProfile_->machineTypeId)) {
    return false;
  }

  UserMachineCalibration* localCalibration = userRegistry_.findCalibration(*activeUser_, machineProfile_->machineTypeId);
  if (localCalibration == nullptr) {
    localCalibration = userRegistry_.upsertCalibration(*activeUser_,
                                                       machineProfile_->machineTypeId,
                                                       cloudCalibration.suggestedWeightKg,
                                                       cloudCalibration.userRomPercent,
                                                       cloudCalibration.userBottomPct,
                                                       cloudCalibration.userTopPct);
    if (localCalibration == nullptr) {
      return false;
    }
  }

  bool changed = false;
  const uint32_t localNextTs = localCalibration->nextRecommendationUpdatedAt;
  const uint32_t cloudNextTs = cloudCalibration.nextRecommendationUpdatedAt;
  const bool localHasNext = localCalibration->nextRecommendedWeightKg > 0.0f;
  const bool cloudHasNext = cloudCalibration.nextRecommendedWeightKg > 0.0f;
  // Same principle as user profile merge: trust meaningful cloud fields.
  if (cloudCalibration.hasCalibration && !localCalibration->hasCalibration) {
    localCalibration->hasCalibration = true;
    changed = true;
  }
  if (cloudCalibration.suggestedWeightKg > 0.0f &&
      fabsf(localCalibration->suggestedWeightKg - cloudCalibration.suggestedWeightKg) > 0.001f) {
    localCalibration->suggestedWeightKg = cloudCalibration.suggestedWeightKg;
    changed = true;
  }
  if (cloudCalibration.userRomPercent > 0.0f &&
      fabsf(localCalibration->userRomPercent - cloudCalibration.userRomPercent) > 0.001f) {
    localCalibration->userRomPercent = cloudCalibration.userRomPercent;
    changed = true;
  }
  if (fabsf(cloudCalibration.userTopPct - cloudCalibration.userBottomPct) >= 5.0f) {
    if (fabsf(localCalibration->userBottomPct - cloudCalibration.userBottomPct) > 0.001f) {
      localCalibration->userBottomPct = cloudCalibration.userBottomPct;
      changed = true;
    }
    if (fabsf(localCalibration->userTopPct - cloudCalibration.userTopPct) > 0.001f) {
      localCalibration->userTopPct = cloudCalibration.userTopPct;
      changed = true;
    }
  }
  bool acceptCloudNext = false;
  if (cloudHasNext) {
    if (!localHasNext) {
      acceptCloudNext = true;
    } else if (cloudNextTs > 0 && localNextTs == 0) {
      acceptCloudNext = true;
    } else if (cloudNextTs > 0 && localNextTs > 0 && cloudNextTs >= localNextTs) {
      acceptCloudNext = true;
    } else if (cloudNextTs == 0 && localNextTs == 0) {
      acceptCloudNext = true;
    }
  }
  if (acceptCloudNext) {
    if (fabsf(localCalibration->nextRecommendedWeightKg - cloudCalibration.nextRecommendedWeightKg) > 0.001f) {
      localCalibration->nextRecommendedWeightKg = cloudCalibration.nextRecommendedWeightKg;
      changed = true;
    }
    if (!cloudCalibration.nextRecommendationSource.isEmpty() &&
        localCalibration->nextRecommendationSource != cloudCalibration.nextRecommendationSource) {
      localCalibration->nextRecommendationSource = cloudCalibration.nextRecommendationSource;
      changed = true;
    }
    if (!cloudCalibration.nextRecommendationReason.isEmpty() &&
        localCalibration->nextRecommendationReason != cloudCalibration.nextRecommendationReason) {
      localCalibration->nextRecommendationReason = cloudCalibration.nextRecommendationReason;
      changed = true;
    }
    if (cloudNextTs > 0 && localCalibration->nextRecommendationUpdatedAt != cloudNextTs) {
      localCalibration->nextRecommendationUpdatedAt = cloudNextTs;
      changed = true;
    }
  } else if (localHasNext && cloudHasNext && localNextTs > cloudNextTs) {
    logEvent("REC", "next recommendation differs reason=local_newer", LogLevel::Normal);
  }
  localCalibration->updatedAtEpoch = max(localCalibration->updatedAtEpoch, cloudCalibration.updatedAtEpoch);
  if (!cloudCalibration.updatedAtIso.isEmpty()) {
    localCalibration->updatedAtIso = cloudCalibration.updatedAtIso;
  }
  return changed;
}

bool SmartGymTouchApp::reconcileCloudStateForScan() {
  if (!cloudEnabled_ || activeUser_ == nullptr) {
    if (activeUser_ != nullptr) {
      refreshActiveCalibration();
      configureRepDetectorThresholds();
      logEvent("USER_SYNC",
               String("recommendation resolved kg=") +
                   String(resolvedRecommendation_.hasRecommendation ? resolvedRecommendation_.kg : 0.0f, 1) +
                   " source=" +
                   (resolvedRecommendation_.hasRecommendation ? resolvedRecommendation_.source : String("fallback")),
               LogLevel::Normal);
    }
    return true;
  }
  CloudLockGuard cloudLock(cloudMutex_, 0);
  if (!cloudLock) {
    requestCloudSync(true);
    return false;
  }

  if (pendingScanReconcileStep_ == 0) {
    pendingScanReconcileStep_ = 1;
  }

  if (pendingScanReconcileStep_ == 1) {
    updateUserLoadingPopupStage(activeUser_->rfidUid, "profile");
    logEvent("USER_SYNC", "stage=profile", LogLevel::Normal);
    refreshDeviceCalibrationFromCloud();
    pendingScanReconcileStep_ = 2;
    return false;
  }

  if (pendingScanReconcileStep_ == 2) {
    const uint32_t nowEpoch = firebaseService_.getCurrentEpoch();
    UserProfile cloudProfile;
    const bool profileFetched = firebaseService_.fetchUserProfile(activeUser_->rfidUid, cloudProfile);
    if (profileFetched) {
      lastCloudProfileFetchedEpoch_ = cloudProfile.updatedAtEpoch;
      if (mergeActiveUserFromCloud(cloudProfile)) {
        saveUsers();
      }
      logEvent("PROFILE",
               String("write skipped reason=cloud_fresh uid=") + activeUser_->rfidUid,
               LogLevel::Normal);
      logEvent("USER_SYNC",
               String("profile ok uid=") + activeUser_->rfidUid,
               LogLevel::Normal);
    } else if (firebaseService_.getLastHttpStatusCode() == 200) {
      activeUser_->updatedAtEpoch = nowEpoch;
      activeUser_->updatedAtIso = firebaseService_.getCurrentIso();
      logEvent("PROFILE",
               String("write skipped reason=cloud_fresh uid=") + activeUser_->rfidUid,
               LogLevel::Normal);
      logEvent("USER_SYNC",
               String("profile ok uid=") + activeUser_->rfidUid,
               LogLevel::Normal);
    } else {
      logEvent("USER_SYNC",
               String("profile fallback reason=read_failed uid=") + activeUser_->rfidUid,
               LogLevel::Normal);
    }
    pendingScanReconcileStep_ = 3;
    return false;
  }

  if (pendingScanReconcileStep_ == 3) {
    updateUserLoadingPopupStage(activeUser_->rfidUid, "recommendation");
    const String machineId = machineProfile_ != nullptr ? machineProfile_->machineTypeId : String("unknown");
    logEvent("USER_SYNC",
             String("stage=calibration machine=") + machineId,
             LogLevel::Normal);
    UserMachineCalibration cloudCalibration;
    const bool calibrationFetched =
        machineProfile_ != nullptr &&
        firebaseService_.fetchCalibration(activeUser_->rfidUid, machineProfile_->machineTypeId, cloudCalibration);
    if (calibrationFetched) {
      if (mergeActiveCalibrationFromCloud(cloudCalibration)) {
        saveUsers();
      }
      logEvent("USER_SYNC",
               String("calibration ok uid=") + activeUser_->rfidUid +
                   " machine=" + machineId +
                   " payloadBytes=0",
               LogLevel::Normal);
      if (activeCalibration_ != nullptr &&
          activeCalibration_->updatedAtEpoch > cloudCalibration.updatedAtEpoch) {
        syncActiveCalibrationToCloud();
      }
    } else if (firebaseService_.getLastHttpStatusCode() == 200 &&
               activeCalibration_ != nullptr && activeCalibration_->hasCalibration) {
      logEvent("USER_SYNC", "calibration fallback reason=cloud_missing_using_local", LogLevel::Normal);
      syncActiveCalibrationToCloud();
    } else {
      logEvent("USER_SYNC", "calibration fallback reason=read_failed", LogLevel::Normal);
    }
    updateUserLoadingPopupStage(activeUser_->rfidUid, "applying");
    refreshActiveCalibration();
    applyMachineSensorCalibration();
    const String recSource = resolvedRecommendation_.hasRecommendation
                                 ? resolvedRecommendation_.source
                                 : String("fallback");
    logEvent("USER_SYNC",
             String("recommendation resolved kg=") +
                 String(resolvedRecommendation_.hasRecommendation ? resolvedRecommendation_.kg : 0.0f, 1) +
                 " source=" + recSource,
             LogLevel::Normal);
    if (activeCalibration_ != nullptr && activeCalibration_->hasCalibration &&
        activeCalibration_->userTopPct > activeCalibration_->userBottomPct) {
      logEvent("USER_SYNC",
               String("rom applied source=user_calibration bottom=") +
                   String(activeCalibration_->userBottomPct, 1) +
                   " top=" + String(activeCalibration_->userTopPct, 1),
               LogLevel::Normal);
    } else {
      logEvent("USER_SYNC", "rom applied source=machine_default", LogLevel::Normal);
    }
    pendingScanReconcileStep_ = 0;
    return true;
  }

  pendingScanReconcileStep_ = 0;
  return true;
}

bool SmartGymTouchApp::queueSessionUpload(const SessionHistoryRecord& record, bool includeHeavyDetails) {
  const uint16_t pendingBefore = localPersistenceStore_.getPendingUploadCount();
  const uint16_t queueRepairMaxPayloadBytes =
      kUseFirebaseClientQueuedTransport ? kFirebaseClientPatchPayloadHardMaxBytes : kNvsUploadMaxPayloadBytes;
  localPersistenceStore_.repairUploadQueue(queueRepairMaxPayloadBytes, false);
  const uint16_t queueCount = localPersistenceStore_.getPendingUploadCount();
  const uint8_t estimatedCoreItems =
      (!includeHeavyDetails && kUseFirebaseClientQueuedTransport && kUseFirebaseClientCoreBundle) ? 2U : 5U;
  const uint8_t estimatedDetailsItems = includeHeavyDetails ? 0U : 4U;
  const uint8_t neededItems = static_cast<uint8_t>(estimatedCoreItems + estimatedDetailsItems);
  const uint8_t safeCap = includeHeavyDetails ? kSafeQueueSoftCapDetails : kSafeQueueSoftCapCore;
  const bool slotsOk =
      (static_cast<uint16_t>(queueCount) + static_cast<uint16_t>(estimatedCoreItems)) <= kNvsQueueCapacity;
  const bool safeCapOk =
      (static_cast<uint16_t>(queueCount) + static_cast<uint16_t>(neededItems)) <= safeCap;
  const bool preflightOk = slotsOk && safeCapOk;
  logEvent("SYNC_QUEUE",
           String("preflight session=") + record.sessionId +
               " neededItems=" + String(neededItems) +
               " queueCount=" + String(queueCount) +
               " capacity=" + String(kNvsQueueCapacity) +
               " ok=" + (preflightOk ? "1" : "0"),
           LogLevel::Normal);
  if (!preflightOk) {
    logEvent("SYNC_QUEUE",
             String("preflight failed reason=not_enough_slots needed=") + String(neededItems) +
                 " available=" +
                 String(queueCount < kNvsQueueCapacity ? (kNvsQueueCapacity - queueCount) : 0),
             LogLevel::Normal);
    if (includeHeavyDetails) {
      logEvent("SYNC_PHASE", "details deferred reason=queue_pressure", LogLevel::Normal);
    }
    return false;
  }
  const String userKey = record.userUid.isEmpty() ? "anonymous" : record.userUid;
  String dayKey = "undated";
  String weekKey = "unscheduled";
  const uint32_t anchorEpoch = record.startedAtEpoch != 0 ? record.startedAtEpoch : record.endedAtEpoch;
  if (anchorEpoch != 0) {
    time_t rawTime = static_cast<time_t>(anchorEpoch);
    struct tm timeInfo {};
    localtime_r(&rawTime, &timeInfo);
    char dayBuffer[16];
    char weekBuffer[16];
    if (strftime(dayBuffer, sizeof(dayBuffer), "%Y-%m-%d", &timeInfo) > 0) {
      dayKey = String(dayBuffer);
    }
    if (strftime(weekBuffer, sizeof(weekBuffer), "%G-W%V", &timeInfo) > 0) {
      weekKey = String(weekBuffer);
    }
  }

  bool scratchLoaded = false;
  const auto ensureScratchLoaded = [&]() -> bool {
    if (!scratchLoaded) {
      uploadScratchRecorder_.loadRecord(record);
      scratchLoaded = true;
    }
    return true;
  };
  const String weekBasePath = "athleteWeeklySessions/" + userKey + "/" + weekKey;
  const String dayBasePath = weekBasePath + "/days/" + dayKey;
  const String sessionPath = dayBasePath + "/sessions/" + record.sessionId;
  const String timelineKey = String(anchorEpoch) + "_" + record.sessionId;
  const auto withMeta = [&](const String& json, const String& idem) {
    if (json.isEmpty() || !json.endsWith("}")) {
      return json;
    }
    String out = json;
    out.remove(out.length() - 1);
    out += ",\"schemaVersion\":2";
    out += ",\"idempotencyKey\":\"";
    out += idem;
    out += "\"}";
    return out;
  };
  const String dayMetaId = "meta_" + record.sessionId;
  const String daySummaryId = "daysummary_" + dayKey + "_" + record.sessionId;
  const String timelineId = "timeline_" + timelineKey;
  const String weekSummaryId = "weeksummary_" + weekKey + "_" + record.sessionId;
  const String sessionIdem = "session_" + record.sessionId;
  const String setDetailsIdem = "setdetails_" + record.sessionId;
  const String rawSessionIdem = "rawsession_" + record.sessionId;
  const String manifestIdem = "manifest_" + record.sessionId;
  uint16_t queuedItems = 0;
  uint16_t deferredItems = 0;
  uint16_t failedItems = 0;
  bool enqueueFailureDetected = false;
  const auto queueWithEvict = [&](const String& path,
                                  const String& payload,
                                  const char* label,
                                  bool allowEvict,
                                  const String& method = "PUT",
                                  bool allowLargeInCompactPhase = false) -> bool {
    if (!includeHeavyDetails &&
        !allowLargeInCompactPhase &&
        payload.length() > kNvsUploadMaxPayloadBytes) {
      deferredItems++;
      logEvent("SYNC", String("defer heavy upload ") + label + " path=" + path, LogLevel::Verbose);
      return true;
    }
    if (localPersistenceStore_.enqueueUpload(path, payload, method)) {
      queuedItems++;
      return true;
    }
    enqueueFailureDetected = true;
    logEvent("SYNC_QUEUE",
             String("enqueue failed reason=nvs_not_enough_space path=") + path +
                 " payloadBytes=" + String(payload.length()),
             LogLevel::Normal);
    if (allowEvict) {
      // Compact summary docs are the priority. If NVS is carrying stale queue
      // entries, evict enough old entries to make room for the new summary.
      for (uint8_t attempt = 0; attempt < 16; ++attempt) {
        if (!localPersistenceStore_.dropOldestUpload()) {
          break;
        }
        if (localPersistenceStore_.enqueueUpload(path, payload, method)) {
          queuedItems++;
          logEvent("SYNC", String("queue recovered after evict ") + label + " path=" + path, LogLevel::Normal);
          return true;
        }
      }
    }
    // NVS queue could not accept this payload. Do not let optional/heavy docs
    // evict the compact workout summary docs; defer them instead.
    failedItems++;
    logEvent("SYNC", String("queue failed ") + label + " path=" + path +
                         " bytes=" + String(payload.length()),
             LogLevel::Normal);
    return false;
  };

  const auto buildWebAppCompatibleSessionRootJson = [&](bool compactAnalysis,
                                                         bool compactSetOverview,
                                                         bool compactMinimal) -> String {
    float volumeLoadKg = record.selectedWeightKg * static_cast<float>(record.validReps);
    float validRepRate = 0.0f;
    if (record.repCount > 0) {
      validRepRate = static_cast<float>(record.validReps) / static_cast<float>(record.repCount);
    }

    uint16_t validAtOrAboveUserRomCount = 0;
    uint16_t validBelowUserRomCount = 0;
    uint16_t validAtOrAboveIdealRomCount = 0;
    uint16_t invalidShortRomCount = 0;
    uint16_t invalidTooFastCount = 0;
    uint16_t invalidTopNotReachedCount = 0;
    uint16_t invalidNoConcentricCount = 0;
    float firstSetAvgPeakVelocityPctPerSec = 0.0f;
    float firstSetAvgRomPercent = 0.0f;
    float lastSetAvgPeakVelocityPctPerSec = 0.0f;
    float lastSetAvgRomPercent = 0.0f;
    uint8_t firstSetNum = 0;
    uint8_t lastSetNum = 0;
    float restSecondsSum = 0.0f;
    uint8_t restSetCount = 0;

    String setOverview = "[";
    bool firstSetEntry = true;
    for (uint8_t setNumber = 1; setNumber <= SessionHistoryRecord::kMaxSets; ++setNumber) {
      const SetHistoryRecord& setRecord = record.sets[setNumber - 1];
      uint16_t repCountInSet = 0;
      bool hasWeight = false;
      float weightStart = 0.0f;
      float weightEnd = 0.0f;
      bool weightChanged = false;
      for (uint16_t repIndex = 0; repIndex < record.repCount; ++repIndex) {
        const RepHistoryRecord& repRecord = record.reps[repIndex];
        if (repRecord.setNumber != setNumber) {
          continue;
        }
        repCountInSet++;
        if (!hasWeight) {
          weightStart = repRecord.selectedWeightKg;
          weightEnd = repRecord.selectedWeightKg;
          hasWeight = true;
        } else {
          weightEnd = repRecord.selectedWeightKg;
          if (fabsf(weightEnd - weightStart) > 0.05f) {
            weightChanged = true;
          }
        }
        if (repRecord.valid) {
          if (record.userRomPercent > 0.0f && repRecord.romPercent >= record.userRomPercent) {
            validAtOrAboveUserRomCount++;
          } else {
            validBelowUserRomCount++;
          }
          if (record.machineIdealRomPercent > 0.0f &&
              repRecord.romPercent >= record.machineIdealRomPercent) {
            validAtOrAboveIdealRomCount++;
          }
        } else {
          if (repRecord.invalidFlags & RepInvalidShortRom) {
            invalidShortRomCount++;
          }
          if (repRecord.invalidFlags & RepInvalidTooFast) {
            invalidTooFastCount++;
          }
          if (repRecord.invalidFlags & RepInvalidTopNotReached) {
            invalidTopNotReachedCount++;
          }
          if (repRecord.invalidFlags & RepInvalidNoConcentricPhase) {
            invalidNoConcentricCount++;
          }
        }
      }

      const bool hasSetData = repCountInSet > 0 || setRecord.validReps > 0 || setRecord.invalidReps > 0;
      if (!hasSetData) {
        continue;
      }
      if (firstSetNum == 0) {
        firstSetNum = setNumber;
        firstSetAvgPeakVelocityPctPerSec = setRecord.avgPeakVelocityPctPerSec;
        firstSetAvgRomPercent = setRecord.avgRomPercent;
      }
      lastSetNum = setNumber;
      lastSetAvgPeakVelocityPctPerSec = setRecord.avgPeakVelocityPctPerSec;
      lastSetAvgRomPercent = setRecord.avgRomPercent;
      if (setRecord.actualRestSeconds > 0) {
        restSecondsSum += static_cast<float>(setRecord.actualRestSeconds);
        restSetCount++;
      }

      if (!firstSetEntry) {
        setOverview += ",";
      }
      firstSetEntry = false;
      setOverview += "{";
      setOverview += "\"setNumber\":" + String(setNumber) + ",";
      setOverview += "\"repCount\":" + String(repCountInSet) + ",";
      setOverview += "\"validReps\":" + String(setRecord.validReps) + ",";
      setOverview += "\"invalidReps\":" + String(setRecord.invalidReps) + ",";
      setOverview += "\"avgRomPercent\":" + String(setRecord.avgRomPercent, 2);
      if (!compactSetOverview) {
        setOverview += ",\"fastEccentricWarnings\":" + String(setRecord.fastEccentricWarnings);
        setOverview += ",\"avgConcentricTimeMs\":" + String(setRecord.avgConcentricTimeMs, 2);
        setOverview += ",\"avgPeakVelocityPctPerSec\":" + String(setRecord.avgPeakVelocityPctPerSec, 2);
        setOverview += ",\"avgPeakEccentricVelocityPctPerSec\":" +
                       String(setRecord.avgPeakEccentricVelocityPctPerSec, 2);
        setOverview += ",\"selectedWeightKgStart\":" + String(hasWeight ? weightStart : record.selectedWeightKg, 2);
        setOverview += ",\"selectedWeightKgEnd\":" + String(hasWeight ? weightEnd : record.selectedWeightKg, 2);
        setOverview += ",\"targetRepsUsed\":" + String(setRecord.targetRepsUsed);
        setOverview += ",\"plannedRestSeconds\":" + String(setRecord.plannedRestSeconds);
        setOverview += ",\"actualRestSeconds\":" + String(setRecord.actualRestSeconds);
        setOverview += ",\"weightChangedDuringSet\":" + String(weightChanged ? "true" : "false");
      }
      setOverview += "}";
    }
    setOverview += "]";

    const float avgRestSecondsPerSet =
        restSetCount > 0 ? (restSecondsSum / static_cast<float>(restSetCount)) : 0.0f;
    const float fatigueRomDrop =
        (firstSetNum != 0 && lastSetNum != 0) ? (firstSetAvgRomPercent - lastSetAvgRomPercent) : 0.0f;
    const float fatigueVelocityDrop =
        (firstSetNum != 0 && lastSetNum != 0)
            ? (firstSetAvgPeakVelocityPctPerSec - lastSetAvgPeakVelocityPctPerSec)
            : 0.0f;
    const float romComplianceRate = record.validReps > 0
                                        ? (static_cast<float>(validAtOrAboveUserRomCount) /
                                           static_cast<float>(record.validReps))
                                        : 0.0f;
    const float idealRomHitRate = record.validReps > 0
                                      ? (static_cast<float>(validAtOrAboveIdealRomCount) /
                                         static_cast<float>(record.validReps))
                                      : 0.0f;

    String analysisJson = "{";
    if (compactMinimal) {
      analysisJson += "\"fatigueRomDrop\":" + String(fatigueRomDrop, 2) + ",";
      analysisJson += "\"fatigueVelocityDrop\":" + String(fatigueVelocityDrop, 2) + ",";
      analysisJson += "\"romComplianceRate\":" + String(romComplianceRate, 4) + ",";
      analysisJson += "\"idealRomHitRate\":" + String(idealRomHitRate, 4);
    } else {
      analysisJson += "\"avgRestSecondsPerSet\":" + String(avgRestSecondsPerSet, 2) + ",";
      analysisJson += "\"fatigueRomDrop\":" + String(fatigueRomDrop, 2) + ",";
      analysisJson += "\"fatigueVelocityDrop\":" + String(fatigueVelocityDrop, 2) + ",";
      analysisJson += "\"firstSetAvgPeakVelocityPctPerSec\":" + String(firstSetAvgPeakVelocityPctPerSec, 2) + ",";
      analysisJson += "\"firstSetAvgRomPercent\":" + String(firstSetAvgRomPercent, 2) + ",";
      analysisJson += "\"lastSetAvgPeakVelocityPctPerSec\":" + String(lastSetAvgPeakVelocityPctPerSec, 2) + ",";
      analysisJson += "\"lastSetAvgRomPercent\":" + String(lastSetAvgRomPercent, 2) + ",";
      analysisJson += "\"romComplianceRate\":" + String(romComplianceRate, 4) + ",";
      analysisJson += "\"idealRomHitRate\":" + String(idealRomHitRate, 4) + ",";
      analysisJson += "\"validAtOrAboveUserRomCount\":" + String(validAtOrAboveUserRomCount) + ",";
      analysisJson += "\"validBelowUserRomCount\":" + String(validBelowUserRomCount) + ",";
      analysisJson += "\"validAtOrAboveIdealRomCount\":" + String(validAtOrAboveIdealRomCount) + ",";
      analysisJson += "\"invalidShortRomCount\":" + String(invalidShortRomCount);
      if (!compactAnalysis) {
        analysisJson += ",\"invalidTooFastCount\":" + String(invalidTooFastCount);
        analysisJson += ",\"invalidTopNotReachedCount\":" + String(invalidTopNotReachedCount);
        analysisJson += ",\"invalidNoConcentricCount\":" + String(invalidNoConcentricCount);
      }
    }
    analysisJson += "}";

    String json;
    json.reserve(2800);
    json += "{";
    json += "\"schemaVersion\":2,";
    json += "\"sessionId\":\"" + jsonEscape(record.sessionId) + "\",";
    json += "\"idempotencyKey\":\"" + jsonEscape(sessionIdem) + "\",";
    json += "\"identity\":{";
    json += "\"anonymous\":" + String(record.anonymous ? "true" : "false") + ",";
    json += "\"userUid\":\"" + jsonEscape(record.userUid) + "\",";
    json += "\"userDisplayName\":\"" + jsonEscape(record.userDisplayName) + "\"";
    json += "},";
    json += "\"machine\":{";
    json += "\"machineId\":\"" + jsonEscape(record.machineId) + "\",";
    json += "\"machineTypeId\":\"" + jsonEscape(record.machineTypeId) + "\",";
    json += "\"machineDisplayName\":\"" + jsonEscape(record.machineDisplayName) + "\"";
    if (!compactMinimal) {
      json += ",\"exerciseCategory\":\"" + jsonEscape(record.exerciseCategory) + "\"";
      json += ",\"primaryMuscleGroup\":\"" + jsonEscape(record.primaryMuscleGroup) + "\"";
      json += ",\"secondaryMuscleGroup\":\"" + jsonEscape(record.secondaryMuscleGroup) + "\"";
      json += ",\"machineIdealRomPercent\":" + String(record.machineIdealRomPercent, 2);
    }
    json += "},";
    json += "\"plan\":{";
    json += "\"calibrationBased\":" + String(record.calibrationBased ? "true" : "false") + ",";
    json += "\"goal\":\"" + jsonEscape(record.goal) + "\",";
    json += "\"plannedRestSeconds\":" + String(record.plannedRestSeconds) + ",";
    json += "\"selectedWeightKg\":" + String(record.selectedWeightKg, 2) + ",";
    if (!compactMinimal) {
      json += "\"suggestedWeightKg\":" + String(record.suggestedWeightKg, 2) + ",";
    }
    json += "\"targetRepsMax\":" + String(record.targetRepsMax) + ",";
    json += "\"targetRepsMin\":" + String(record.targetRepsMin) + ",";
    json += "\"targetSets\":" + String(record.targetSets);
    if (!compactMinimal) {
      json += ",\"userRomPercent\":" + String(record.userRomPercent, 2);
    }
    json += "},";
    json += "\"timing\":{";
    if (!compactMinimal) {
      json += "\"startMs\":" + String(record.startMs) + ",";
      json += "\"endMs\":" + String(record.endMs) + ",";
    }
    json += "\"durationMs\":" + String(record.durationMs) + ",";
    json += "\"startedAtEpoch\":" + String(record.startedAtEpoch) + ",";
    json += "\"endedAtEpoch\":" + String(record.endedAtEpoch);
    if (!compactMinimal) {
      json += ",\"startedAtIso\":\"" + jsonEscape(record.startedAtIso) + "\"";
      json += ",\"endedAtIso\":\"" + jsonEscape(record.endedAtIso) + "\"";
    }
    if (!compactMinimal) {
      json += ",\"totalRestMs\":" + String(record.totalRestMs);
    }
    json += "},";
    json += "\"summary\":{";
    json += "\"avgConcentricTimeMs\":" + String(record.avgConcentricTimeMs, 2) + ",";
    if (!compactMinimal) {
      json += "\"avgPeakEccentricVelocityPctPerSec\":" + String(record.avgPeakEccentricVelocityPctPerSec, 2) + ",";
    }
    json += "\"avgPeakVelocityPctPerSec\":" + String(record.avgPeakVelocityPctPerSec, 2) + ",";
    json += "\"avgRomPercent\":" + String(record.avgRomPercent, 2) + ",";
    if (!compactMinimal) {
      json += "\"bestRomPercent\":" + String(record.bestRomPercent, 2) + ",";
      json += "\"fastEccentricWarnings\":" + String(record.fastEccentricWarnings) + ",";
    }
    json += "\"invalidReps\":" + String(record.invalidReps) + ",";
    json += "\"repCount\":" + String(record.repCount) + ",";
    if (!compactMinimal) {
      json += "\"sessionQualityScore\":" + String(record.sessionQualityScore, 2) + ",";
      json += "\"sessionQualityTier\":\"" + jsonEscape(record.sessionQualityTier) + "\",";
    }
    json += "\"setCount\":" + String(record.setCount) + ",";
    json += "\"setsCompleted\":" + String(record.setsCompleted) + ",";
    json += "\"validRepRate\":" + String(validRepRate, 4) + ",";
    json += "\"validReps\":" + String(record.validReps);
    if (!compactMinimal) {
      json += ",\"volumeLoadKg\":" + String(volumeLoadKg, 2);
    }
    json += "},";
    json += "\"analysis\":" + analysisJson + ",";
    json += "\"setOverview\":" + setOverview + ",";
    json += "\"paths\":{";
    json += "\"repSets\":\"repSets\",";
    json += "\"setDetails\":\"setDetails\"";
    json += "}";
    json += "}";
    return json;
  };

  bool queuedSession = false;
  if (kUploadSessionRootDocument) {
    String webAppSessionRoot = buildWebAppCompatibleSessionRootJson(false, false, false);
    logEvent("SYNC",
             String("webapp session root payloadBytes=") + String(webAppSessionRoot.length()),
             LogLevel::Normal);
    if (webAppSessionRoot.length() > kWebAppSessionRootMaxPayloadBytes) {
      webAppSessionRoot = buildWebAppCompatibleSessionRootJson(true, false, false);
      logEvent("SYNC",
               String("webapp session root compacted payloadBytes=") + String(webAppSessionRoot.length()) +
                   " target=" + String(kWebAppSessionRootMaxPayloadBytes) + " mode=analysis",
               LogLevel::Normal);
    }
    if (webAppSessionRoot.length() > kWebAppSessionRootMaxPayloadBytes) {
      webAppSessionRoot = buildWebAppCompatibleSessionRootJson(true, true, false);
      logEvent("SYNC",
               String("webapp session root compacted payloadBytes=") + String(webAppSessionRoot.length()) +
                   " target=" + String(kWebAppSessionRootMaxPayloadBytes) + " mode=setOverview",
               LogLevel::Normal);
    }
    if (webAppSessionRoot.length() > kWebAppSessionRootMaxPayloadBytes) {
      webAppSessionRoot = buildWebAppCompatibleSessionRootJson(true, true, true);
      logEvent("SYNC",
               String("webapp session root compacted payloadBytes=") + String(webAppSessionRoot.length()) +
                   " target=" + String(kWebAppSessionRootMaxPayloadBytes) + " mode=minimal",
               LogLevel::Normal);
    }
    logEvent("SYNC",
             String("webapp session root final payloadBytes=") + String(webAppSessionRoot.length()) +
                 " target=" + String(kWebAppSessionRootMaxPayloadBytes) +
                 " hardMax=" + String(kWebAppSessionRootHardMaxPayloadBytes),
             LogLevel::Normal);
    if (webAppSessionRoot.length() > kWebAppSessionRootHardMaxPayloadBytes) {
      failedItems++;
      logEvent("SYNC",
               String("webapp session root too large payloadBytes=") + String(webAppSessionRoot.length()) +
                   " hardMax=" + String(kWebAppSessionRootHardMaxPayloadBytes),
               LogLevel::Normal);
    } else {
      logEvent("SYNC",
               String("webapp session root accepted payloadBytes=") + String(webAppSessionRoot.length()) +
                   " hardMax=" + String(kWebAppSessionRootHardMaxPayloadBytes),
               LogLevel::Normal);
      queuedSession = queueWithEvict(sessionPath, webAppSessionRoot, "session", true);
    }
  }
  const auto firebaseClientGuardAllows = [&](size_t payloadBytes,
                                             uint32_t& minInternalHeap,
                                             uint32_t& minLargestBlock,
                                             uint32_t& actualInternalHeap,
                                             uint32_t& actualLargestBlock) -> bool {
    UploadMemoryMode mode = getUploadMemoryMode(actualInternalHeap, actualLargestBlock);
    if (mode == UploadMemoryMode::Normal) {
      if (payloadBytes <= 1200U) {
        minInternalHeap = 43000UL;
        minLargestBlock = 22000UL;
      } else if (payloadBytes <= 2200U) {
        minInternalHeap = 50000UL;
        minLargestBlock = 24000UL;
      } else if (payloadBytes <= 3000U) {
        minInternalHeap = 52000UL;
        minLargestBlock = 26000UL;
      } else if (payloadBytes <= 6200U) {
        minInternalHeap = 56000UL;
        minLargestBlock = 30000UL;
      } else {
        minInternalHeap = 58000UL;
        minLargestBlock = 34000UL;
      }
    } else if (mode == UploadMemoryMode::Constrained) {
      if (payloadBytes <= 700U) {
        minInternalHeap = 22000UL;
        minLargestBlock = 7000UL;
      } else if (payloadBytes <= 1200U) {
        minInternalHeap = 23000UL;
        minLargestBlock = 9000UL;
      } else {
        minInternalHeap = 60000UL;
        minLargestBlock = 36000UL;
      }
    } else {
      if (payloadBytes <= 700U) {
        minInternalHeap = 20000UL;
        minLargestBlock = 6000UL;
      } else {
        minInternalHeap = 60000UL;
        minLargestBlock = 36000UL;
      }
    }
    return actualInternalHeap >= minInternalHeap && actualLargestBlock >= minLargestBlock;
  };

  const String metaPayload =
      withMeta(firebaseService_.buildDailyMetaJson(record, dayKey, weekKey), dayMetaId);
  const String daySummaryPayload =
      withMeta(firebaseService_.buildAggregateSummaryJson("", record, false), daySummaryId);
  const String timelinePayload =
      withMeta(firebaseService_.buildSessionTimelineJson(record, dayKey, weekKey), timelineId);
  bool queuedMeta = false;
  bool queuedDaySummary = false;
  bool queuedTimeline = false;
  bool queuedWeekSummary = false;
  String weekSummaryPayload;
  {
    const String signalMachineId = record.machineId.isEmpty()
                                       ? (machineProfile_ != nullptr ? machineProfile_->machineId : String(""))
                                       : record.machineId;
    WeeklyTrainingSignal weeklySignal;
    const uint32_t signalEpoch = record.endedAtEpoch != 0 ? record.endedAtEpoch : record.startedAtEpoch;
    const bool hasWeeklySignal = !record.userUid.isEmpty() &&
                                 localPersistenceStore_.buildWeeklyTrainingSignal(record.userUid,
                                                                                  signalMachineId,
                                                                                  signalEpoch,
                                                                                  weeklySignal) &&
                                 weeklySignal.hasData;
    const float setCompletionRatio = record.targetSets > 0
                                         ? (static_cast<float>(record.setsCompleted) /
                                            static_cast<float>(record.targetSets))
                                         : 0.0f;
    const float repCompletionRatio = record.repCount > 0
                                         ? (static_cast<float>(record.validReps) /
                                            static_cast<float>(record.repCount))
                                         : 0.0f;
    const float poorFormSessionRatio = record.sessionQualityScore < 60.0f ? 1.0f : 0.0f;
    String weekSummaryJson = "{";
    weekSummaryJson += "\"schemaVersion\":2,";
    weekSummaryJson += "\"scope\":\"week\",";
    weekSummaryJson += "\"weekKey\":\"" + weekKey + "\",";
    weekSummaryJson += "\"userUid\":\"" + record.userUid + "\",";
    weekSummaryJson += "\"machineId\":\"" + signalMachineId + "\",";
    weekSummaryJson += "\"sessions\":" + String(hasWeeklySignal ? weeklySignal.sessions : 1) + ",";
    weekSummaryJson += "\"recentSessionsUsed\":" + String(hasWeeklySignal ? weeklySignal.recentSessionsUsed : 1) + ",";
    weekSummaryJson += "\"avgSelectedWeightKg\":" +
                       String(hasWeeklySignal ? weeklySignal.avgSelectedWeightKg : record.selectedWeightKg, 2) + ",";
    weekSummaryJson += "\"lastSelectedWeightKg\":" +
                       String(hasWeeklySignal ? weeklySignal.lastSelectedWeightKg : record.selectedWeightKg, 2) + ",";
    weekSummaryJson += "\"avgSetCompletionRatio\":" +
                       String(hasWeeklySignal ? weeklySignal.avgSetCompletionRatio : setCompletionRatio, 4) + ",";
    weekSummaryJson += "\"avgRepCompletionRatio\":" +
                       String(hasWeeklySignal ? weeklySignal.avgRepCompletionRatio : repCompletionRatio, 4) + ",";
    weekSummaryJson += "\"avgPeakVelocityPctPerSec\":" +
                       String(hasWeeklySignal ? weeklySignal.avgPeakVelocityPctPerSec : record.avgPeakVelocityPctPerSec, 2) + ",";
    weekSummaryJson += "\"avgRepQualityScore\":" +
                       String(hasWeeklySignal ? weeklySignal.avgRepQualityScore : record.sessionQualityScore, 2) + ",";
    weekSummaryJson += "\"poorFormSessionRatio\":" +
                       String(hasWeeklySignal ? weeklySignal.poorFormSessionRatio : poorFormSessionRatio, 4) + ",";
    weekSummaryJson += "\"updatedAtEpoch\":" + String(signalEpoch) + ",";
    weekSummaryJson += "\"lastSessionId\":\"" + record.sessionId + "\"";
    weekSummaryJson += "}";
    weekSummaryPayload = withMeta(weekSummaryJson, weekSummaryId);
    logEvent("SYNC",
             String("webapp weekSummary payloadBytes=") + String(weekSummaryPayload.length()),
             LogLevel::Normal);
  }

  bool queuedCoreBundle = false;
  uint16_t coreBundlePayloadBytes = 0;
  if (!includeHeavyDetails && kUseFirebaseClientQueuedTransport && kUseFirebaseClientCoreBundle) {
    String coreBundlePayload = "{";
    coreBundlePayload += "\"days/" + dayKey + "/meta\":" + metaPayload + ",";
    coreBundlePayload += "\"days/" + dayKey + "/daySummary\":" + daySummaryPayload + ",";
    coreBundlePayload += "\"days/" + dayKey + "/timeline/" + timelineKey + "\":" + timelinePayload + ",";
    coreBundlePayload += "\"weekSummary\":" + weekSummaryPayload;
    coreBundlePayload += "}";
    coreBundlePayloadBytes = static_cast<uint16_t>(coreBundlePayload.length());
    logEvent("SYNC",
             String("core bundle mode=firebaseclient payloadBytes=") + String(coreBundlePayloadBytes),
             LogLevel::Normal);
    if (coreBundlePayload.length() > kFirebaseClientPatchPayloadHardMaxBytes) {
      logEvent("SYNC",
               String("core bundle fallback reason=payload_too_large payloadBytes=") +
                   String(coreBundlePayload.length()),
               LogLevel::Normal);
    } else {
      uint32_t minHeap = 0;
      uint32_t minBlock = 0;
      uint32_t actualHeap = 0;
      uint32_t actualBlock = 0;
      if (!queuedSession) {
        logEvent("SYNC", "core bundle enqueue deferred reason=root_first", LogLevel::Normal);
      } else if (!firebaseClientGuardAllows(coreBundlePayload.length(),
                                     minHeap,
                                     minBlock,
                                     actualHeap,
                                     actualBlock)) {
        logEvent("SYNC",
                 String("core bundle fallback reason=heap_guard internalHeap=") + String(actualHeap) +
                     " largestBlock=" + String(actualBlock) +
                     " minInternalHeap=" + String(minHeap) +
                     " minLargestBlock=" + String(minBlock),
                 LogLevel::Normal);
      } else {
        queuedCoreBundle = queueWithEvict(weekBasePath,
                                          coreBundlePayload,
                                          "coreBundle",
                                          true,
                                          "PATCH",
                                          true);
        if (queuedCoreBundle) {
          logEvent("SYNC",
                   String("core bundle enqueue after_root path=") + weekBasePath +
                       " payloadBytes=" + String(coreBundlePayload.length()),
                   LogLevel::Normal);
          logEvent("SYNC",
                   String("core bundle queued path=") + weekBasePath,
                   LogLevel::Normal);
          queuedMeta = true;
          queuedDaySummary = true;
          queuedTimeline = true;
          queuedWeekSummary = true;
        }
      }
    }
  }

  if (!queuedCoreBundle) {
    queuedMeta = queueWithEvict(dayBasePath + "/meta",
                                metaPayload,
                                "meta",
                                true);
    queuedDaySummary = queueWithEvict(dayBasePath + "/daySummary",
                                      daySummaryPayload,
                                      "daySummary",
                                      true);
    queuedTimeline = queueWithEvict(dayBasePath + "/timeline/" + timelineKey,
                                    timelinePayload,
                                    "timeline",
                                    true);
    queuedWeekSummary = queueWithEvict(weekBasePath + "/weekSummary",
                                       weekSummaryPayload,
                                       "weekSummary",
                                       true);
  }
  if (!includeHeavyDetails) {
    const bool mandatoryCoreQueued =
        queuedMeta && queuedDaySummary && queuedTimeline && queuedSession && queuedWeekSummary;
    if (mandatoryCoreQueued) {
      logEvent("SYNC_PHASE", "core enqueue begin", LogLevel::Normal);
      pendingWebDetailUpload_ = true;
      pendingWebDetailWeekKey_ = weekKey;
      pendingWebDetailDayKey_ = dayKey;
      pendingWebDetailSessionPath_ = sessionPath;
      pendingWebRootPath_ = sessionPath;
      pendingWebRootUploaded_ = false;
      pendingWebCoreBundleRequired_ = queuedCoreBundle;
      pendingWebCoreBundleUploaded_ = !queuedCoreBundle;
      pendingWebCoreBundlePath_ = queuedCoreBundle ? weekBasePath : String("");
      pendingWebCoreBundlePayloadBytes_ = queuedCoreBundle ? coreBundlePayloadBytes : 0;
      pendingWebDetailPhase_ = 1;
      pendingWebDetailNextSet_ = 1;
      pendingWebDetailNextRep_ = 1;
      pendingWebDetailSplitSet_ = false;
      pendingWebDetailSetSummaryQueued_ = false;
      pendingWebDetailPauseLogMs_ = 0;
      pendingWebDetailPauseQueueCount_ = 0xFFFF;
      pendingWebDetailDeferSet_ = 0;
      pendingWebDetailDeferQueueCount_ = 0xFFFF;
      pendingWebDetailDeferLogMs_ = 0;
      logEvent("SYNC",
               String("webapp upload phase=core queued=") + String(queuedCoreBundle ? 2 : 5),
               LogLevel::Normal);
      logEvent("SYNC_PHASE",
               String("core enqueue complete queued=") + String(queuedItems),
               LogLevel::Normal);
    } else {
      resetPendingWebDetailUpload();
      logEvent("SYNC",
               "webapp upload phase=core failed; detail phases paused",
               LogLevel::Normal);
      return false;
    }
  }
  bool queuedSessionAnalysis = false;
  if (includeHeavyDetails) {
    ensureScratchLoaded();
    const String fullAnalysis = withMeta(uploadScratchRecorder_.toAthleteAnalysisJson(),
                                         sessionIdem + "_full");
    if (fullAnalysis.length() <= kNvsUploadMaxPayloadBytes) {
      queuedSessionAnalysis =
          queueWithEvict(sessionPath + "/analysisFull", fullAnalysis, "analysisFull", false);
    } else if (!kUploadSessionAnalysisChunks) {
      deferredItems++;
      logEvent("SYNC", "field_split skipped reason=queue_capacity", LogLevel::Normal);
      logEvent("SYNC", "details deferred reason=queue_capacity", LogLevel::Normal);
      queuedSessionAnalysis = false;
    } else {
      queuedSessionAnalysis = false;
    }
  }
  bool queuedSetDetails = false;
  if (includeHeavyDetails && kUploadSetDetailsDocument) {
    ensureScratchLoaded();
    queuedSetDetails =
        queueWithEvict(sessionPath + "/setDetails",
                       withMeta(uploadScratchRecorder_.toSetDetailsJson(), setDetailsIdem),
                       "setDetails",
                       false);
  }
  bool queuedRawSession = false;
  if (includeHeavyDetails && kUploadRawSessionDocument) {
    ensureScratchLoaded();
    const String rawSessionPayload = withMeta(uploadScratchRecorder_.toJson(), rawSessionIdem);
    const bool allowRawSession = !kUploadRawSessionOnlyWhenCompact ||
                                 rawSessionPayload.length() <= kUploadRawSessionMaxBytes;
    if (allowRawSession) {
      // Full-fidelity per-rep payload for debugging/analytics exports.
      queuedRawSession = queueWithEvict(sessionPath + "/rawSession", rawSessionPayload, "rawSession", false);
    } else {
      logEvent("SYNC",
               String("skip rawSession: payload too large bytes=") + String(rawSessionPayload.length()) +
                   " max=" + String(kUploadRawSessionMaxBytes),
               LogLevel::Normal);
    }
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
  bool queuedAllRepSets = true;
  if (includeHeavyDetails && kUploadRepSetsDocuments) {
    ensureScratchLoaded();
    for (uint8_t setNumber = 1; setNumber <= SessionHistoryRecord::kMaxSets; ++setNumber) {
      if (!seenSet[setNumber]) {
        continue;
      }
      const String repSetIdem = "repset_" + record.sessionId + "_s" + String(setNumber);
      const bool ok = queueWithEvict(sessionPath + "/repSets/set" + String(setNumber),
                                     withMeta(uploadScratchRecorder_.toRepSetJson(setNumber), repSetIdem),
                                     "repSet",
                                     false);
      if (!ok) {
        queuedAllRepSets = false;
      }
    }
  }
  String manifestJson = "{";
  manifestJson += "\"sessionId\":\"" + record.sessionId + "\",";
  manifestJson += "\"schemaVersion\":2,";
  manifestJson += "\"recordedRepCount\":" + String(record.repCount) + ",";
  manifestJson += "\"recordedSetCount\":" + String(record.setCount) + ",";
  manifestJson += "\"hasRawSession\":" + String(queuedRawSession ? "true" : "false") + ",";
  manifestJson += "\"hasSetDetails\":" + String(queuedSetDetails ? "true" : "false") + ",";
  manifestJson += "\"hasRepSets\":" + String(queuedAllRepSets ? "true" : "false") + ",";
  manifestJson += "\"hasSessionRoot\":" + String(queuedSession ? "true" : "false") + ",";
  manifestJson += "\"hasSessionAnalysis\":" + String(queuedSessionAnalysis ? "true" : "false") + ",";
  manifestJson += "\"hasDaySummary\":" + String(queuedDaySummary ? "true" : "false") + ",";
  manifestJson += "\"hasWeekSummary\":" + String(queuedWeekSummary ? "true" : "false") + ",";
  manifestJson += "\"hasTimeline\":" + String(queuedTimeline ? "true" : "false") + ",";
  manifestJson += "\"hasMeta\":" + String(queuedMeta ? "true" : "false");
  manifestJson += "}";
  // Manifest is optional debug metadata, not required by dashboard consumption.
  if (kUploadManifestDocument) {
    queueWithEvict(sessionPath + "/uploadManifest", withMeta(manifestJson, manifestIdem), "manifest", false);
  }

  const uint16_t pendingAfter = localPersistenceStore_.getPendingUploadCount();
  if (enqueueFailureDetected) {
    logEvent("SYNC_QUEUE",
             String("partial enqueue detected queued=") + String(queuedItems) +
                 " failed=" + String(failedItems),
             LogLevel::Normal);
    logEvent("SYNC_QUEUE", "details pending preserved=1", LogLevel::Normal);
    logEvent("SYNC_QUEUE", "retry after drain", LogLevel::Normal);
  }
  logEvent("SYNC",
           String("queued session bundle id=") + record.sessionId +
               " queued=" + String(queuedItems) +
               " deferred=" + String(deferredItems) +
               " failed=" + String(failedItems) +
               " items=" + String(pendingAfter >= pendingBefore ? (pendingAfter - pendingBefore) : 0) +
               " queue=" + String(pendingAfter),
           LogLevel::Normal);
  return !enqueueFailureDetected && (!includeHeavyDetails || failedItems == 0);
}

void SmartGymTouchApp::refreshActiveUserFromCloud(const String& uid) {
  if (!cloudEnabled_ || uid.isEmpty()) {
    return;
  }
  const uint32_t nowMs = millis();
  if (shouldSuppressOptionalCloudRead("profile_refresh", nowMs) ||
      shouldSkipRecentCloudRead("profile", uid, nowMs)) {
    return;
  }
  CloudLockGuard cloudLock(cloudMutex_, 0);
  if (!cloudLock) {
    requestCloudSync(true);
    return;
  }

  UserProfile cloudProfile;
  const bool profileFetched = firebaseService_.fetchUserProfile(uid, cloudProfile);
  if (profileFetched) {
    UserProfile* localProfile = userRegistry_.findByUid(uid);
    if (localProfile == nullptr) {
      userRegistry_.upsertProfile(cloudProfile);
      activeUser_ = userRegistry_.findByUid(uid);
      saveUsers();
      return;
    }
    const bool changed = mergeActiveUserFromCloud(cloudProfile);
    if (changed) {
      saveUsers();
    }
  }
}

void SmartGymTouchApp::refreshActiveCalibrationFromCloud() {
  if (!cloudEnabled_ || activeUser_ == nullptr || machineProfile_ == nullptr) {
    return;
  }
  if (shouldSuppressOptionalCloudRead("calibration_refresh", millis())) {
    return;
  }
  CloudLockGuard cloudLock(cloudMutex_, 0);
  if (!cloudLock) {
    requestCloudSync(true);
    return;
  }

  UserMachineCalibration cloudCalibration;
  if (firebaseService_.fetchCalibration(activeUser_->rfidUid,
                                        machineProfile_->machineTypeId,
                                        cloudCalibration)) {
    const bool changed = mergeActiveCalibrationFromCloud(cloudCalibration);
    if (changed) {
      saveUsers();
    }
  }
}

void SmartGymTouchApp::handleRep(const RepMetrics& rep, uint32_t nowMs) {
  hasLastCompletedRep_ = true;
  lastCompletedRep_ = rep;
  if (rep.peakVelocityPctPerSec > sessionBestRepPeakVelocityPctPerSec_) {
    sessionBestRepPeakVelocityPctPerSec_ = rep.peakVelocityPctPerSec;
  }
  if (rep.valid) {
    lastRepSummary_ = "VALID | ROM " + String(rep.romPercent, 0) + "% | Good rep";
  } else {
    lastRepSummary_ = "CHECK | " + buildInvalidReason(rep) + " | " +
                      String(rep.durationMs / 1000.0f, 1) + "s";
  }

  if (state_ == State::Calibration) {
    processCalibrationRep(rep, nowMs);
    refreshUi();
    return;
  }

  if (state_ != State::Training) {
    return;
  }

  // Hard gate: while rest countdown is active between sets, ignore rep events.
  // This prevents accidental carry-over reps from motion/noise right after set end.
  if (isRestCountdownActive(nowMs, nullptr)) {
    setStatusMessage("Rest active. Wait for next set.");
    return;
  }

  const GoalRecommendation* recommendation = getRecommendation();
  if (!rep.valid) {
    const float halfRepCutoff = kActiveRomMinValidRangePct;
    const bool tooSmallToCount = rep.romPercent < halfRepCutoff;
    const bool tooNoisyToCount = rep.durationMs < 220;
    const char* romSource = (activeCalibration_ != nullptr && activeCalibration_->hasCalibration &&
                             activeCalibration_->userTopPct > activeCalibration_->userBottomPct)
                                ? "user_calibration"
                                : "machine_default";

    logEvent("REP_ROM",
             String("top check rom=") + String(rep.maxRomPercent, 1) +
                 " threshold=" + String(kActiveRomTopReachedPct, 1) +
                 " source=" + romSource +
                 " passed=" + String(rep.maxRomPercent >= kActiveRomTopReachedPct ? 1 : 0),
             LogLevel::Normal);
    logEvent("REP_ROM",
             String("bottom check rom=") + String(rep.minRomPercent, 1) +
                 " threshold=" + String(kActiveRomBottomReachedPct, 1) +
                 " source=" + romSource +
                 " passed=" + String(rep.minRomPercent <= kActiveRomBottomReachedPct ? 1 : 0),
             LogLevel::Normal);
    logEvent("REP_ROM",
             String("range check range=") + String(rep.romPercent, 1) +
                 " threshold=" + String(kActiveRomMinValidRangePct, 1) +
                 " passed=" + String(rep.romPercent >= kActiveRomMinValidRangePct ? 1 : 0),
             LogLevel::Normal);

    if (tooSmallToCount || tooNoisyToCount) {
      setStatusMessage("Rep not counted: partial/noisy movement.");
      if (tooSmallToCount) {
        lastRepSummary_ = "CHECK | half rep";
      }
      refreshUi();
      return;
    }

    // Count truly invalid reps as session attempts so invalidReps and valid-rate
    // stay accurate in live/session summaries.
    const uint8_t activeSetNumber = completedSets_ + 1;
    const uint8_t attemptNumberInSet = currentSetRepCount_ + 1;
    const float repWeightKg = selectedWeightKg_;
    sessionRecorder_.recordRep(rep, activeSetNumber, attemptNumberInSet, repWeightKg, nowMs);

    poorRepStreak_ = static_cast<uint8_t>(min<int>(poorRepStreak_ + 1, 10));
    logEvent("REP",
             String("rejected reason=") + buildInvalidReason(rep) +
                 " rom=" + String(rep.romPercent, 1) +
                 " topThreshold=" + String(kActiveRomTopReachedPct, 1) +
                 " bottomThreshold=" + String(kActiveRomBottomReachedPct, 1),
             LogLevel::Normal);
    setStatusMessage("Rep rejected: " + buildInvalidReason(rep));
    if (poorRepStreak_ >= 2) {
      setStatusMessage("Coach: shorten speed and hit full ROM.");
    }
    refreshUi();
    return;
  }

  if (lastAcceptedRepAtMs_ != 0 && (nowMs - lastAcceptedRepAtMs_) < kNoisyRepMinIntervalMs) {
    setStatusMessage("Rep not counted: noise/bounce.");
    lastRepSummary_ = "CHECK | noisy interval";
    refreshUi();
    return;
  }

  const float activeIdealRom = 100.0f;
  const float minRomQuality = max(kRepQualityMinRomPct, kActiveRomMinValidRangePct);
  const float qScore = repQualityScore(rep, activeIdealRom);
  const RepQualityTier qTier = repQualityTierFromScore(qScore);
  if (rep.romPercent < minRomQuality || rep.peakVelocityPctPerSec < kRepQualityMinPeakVelPctPerSec ||
      qTier == RepQualityTier::Bad) {
    poorRepStreak_ = static_cast<uint8_t>(min<int>(poorRepStreak_ + 1, 10));
    const String qualityReason = rep.romPercent < minRomQuality ? "low ROM"
                              : (rep.peakVelocityPctPerSec < kRepQualityMinPeakVelPctPerSec ? "low effort"
                                                                                              : "bad quality");
    setStatusMessage("Rep not counted: " + qualityReason + ".");
    if (poorRepStreak_ >= 2) {
      setStatusMessage("Coach warning: repeat poor reps. Control tempo and ROM.");
    }
    lastRepSummary_ = "CHECK | " + String(repQualityTierText(qTier)) + " | q " + String(qScore, 0);
    refreshUi();
    return;
  }

  // If we are coming out of an inter-set rest, close that rest window in the
  // session recorder exactly when the first valid rep of the next set starts.
  if (currentSetRepCount_ == 0 && completedSets_ > 0 && lastSetCompletedMs_ != 0) {
    sessionRecorder_.endRest(nowMs);
    lastSetCompletedMs_ = 0;
  }

  const uint8_t activeSetNumber = completedSets_ + 1;
  if (currentSetRepCount_ == 0) {
    setStartedAtMs_ = nowMs;
  }
  currentSetRepCount_++;
  lastAcceptedRepAtMs_ = nowMs;
  setPauseCandidateMs_ = 0;
  sessionRepCount_++;
  const float repWeightKg = selectedWeightKg_;
  sessionRecorder_.recordRep(rep, activeSetNumber, currentSetRepCount_, repWeightKg, nowMs);
  const uint8_t targetReps = activeSessionTargetRepsMax_ > 0
                                 ? activeSessionTargetRepsMax_
                                 : (recommendation != nullptr ? recommendation->repsMax : 10);
  const uint8_t targetSets = activeSessionTargetSets_ > 0
                                 ? activeSessionTargetSets_
                                 : (recommendation != nullptr ? recommendation->targetSets : 3);
  const uint16_t restSeconds = activeSessionRestSeconds_ > 0
                                   ? activeSessionRestSeconds_
                                   : (recommendation != nullptr ? recommendation->restSeconds : 45);
  logEvent("SESSION",
           String("rep accepted set=") + String(activeSetNumber) +
               " repInSet=" + String(currentSetRepCount_) +
               " sessionReps=" + String(sessionRepCount_) +
               " targetReps=" + String(targetReps) +
               " weightKg=" + String(repWeightKg, 1),
           LogLevel::Normal);
  poorRepStreak_ = 0;

  setStatusMessage("Rep accepted (" + String(repQualityTierText(qTier)) + ").");
  if (rep.warningFastEccentric) {
    setStatusMessage("Coach warning: eccentric too fast.");
  }
  if (currentSetRepCount_ >= targetReps) {
    sessionRecorder_.completeSet(activeSetNumber, targetReps, restSeconds, nowMs);
    completedSets_++;
    logEvent("SESSION",
             String("set complete set=") + String(activeSetNumber) +
                 " completedSets=" + String(completedSets_) +
                 " targetSets=" + String(targetSets) +
                 " targetReps=" + String(targetReps) +
                 " source=target_reps",
             LogLevel::Normal);
    currentSetRepCount_ = 0;
    setStartedAtMs_ = 0;
    lastAcceptedRepAtMs_ = 0;
    setPauseCandidateMs_ = 0;
    if (completedSets_ >= targetSets) {
      if (!autoFinishRequestPending_) {
        autoFinishRequestPending_ = true;
        autoFinishReason_ = "All target sets completed.";
        logEvent("SESSION",
                 String("auto finish requested reason=") + autoFinishReason_ +
                     " completedSets=" + String(completedSets_) +
                     " targetSets=" + String(targetSets),
                 LogLevel::Normal);
      }
    } else {
      setStatusMessage("Set completed. Rest started.");
      lastSetCompletedMs_ = nowMs;
      sessionRecorder_.beginRest(activeSetNumber, nowMs);
    }
  }

  refreshUi();
}

void SmartGymTouchApp::startCalibration(const char* source) {
  const String sourceText = source != nullptr ? String(source) : String("unknown");
  const bool explicitUserAction =
      sourceText == "button_calibrate" || sourceText == "button_recalibrate";
  const uint32_t nowMs = millis();
  if (nowMs < calibrationStartBlockedUntilMs_) {
    logEvent("CAL",
             String("start blocked reason=post_save_cooldown source=") + sourceText,
             LogLevel::Normal);
    return;
  }
  if (!explicitUserAction) {
    logEvent("CAL",
             String("start blocked reason=not_explicit_user_action source=") + sourceText,
             LogLevel::Normal);
    return;
  }
  if (state_ == State::Idle && nowMs < trainingStartBlockedUntilMs_) {
    logEvent("CAL",
             "start blocked reason=not_explicit_user_action source=profile_edit",
             LogLevel::Normal);
    setStatusMessage("Finish profile edit first.");
    refreshUi();
    return;
  }
  if (machineProfile_ == nullptr) {
    setStatusMessage("Machine profile missing.");
    refreshUi();
    return;
  }
  if (activeUser_ == nullptr && !anonymousMode_) {
    setStatusMessage("No user loaded.");
    refreshUi();
    return;
  }
  logEvent("CAL", String("start allowed source=") + sourceText, LogLevel::Normal);

  calibrationService_.reset();
  repDetector_.reset();
  currentSetRepCount_ = 0;
  state_ = State::Calibration;
  calibrationFlowState_ = CalibrationFlowState::Intro;
  calibrationCurrentSetIndex_ = 0;
  calibrationTargetRepsPerSet_ = kCalibrationMinValidReps;
  calibrationValidRepsInSet_ = 0;
  calibrationRejectedRepsInSet_ = 0;
  calibrationRepVelocitySum_ = 0.0f;
  calibrationRepRomSum_ = 0.0f;
  calibrationRepMinRomSum_ = 0.0f;
  calibrationRepMaxRomSum_ = 0.0f;
  calibrationRepDurationSumMs_ = 0.0f;
  calibrationRepVelocityCount_ = 0;
  calibrationSetSnapshotCount_ = 0;
  lastCalibrationLiveLoadLogKey_ = "";
  lastCalibrationLoadUiLogKey_ = "";
  calibrationHasUserWeightOverride_ = false;
  calibrationResultRecommendedKg_ = 0.0f;
  calibrationResultReason_ = "";
  calibrationResultAction_ = "keep";
  calibrationResultConfidence_ = "low";
  userRomBottomCaptured_ = false;
  userRomTopCaptured_ = false;

  if (!deriveMotionTargetsForActiveMachine(calibrationMotionTargets_)) {
    calibrationMotionTargets_ = MotionTargetConfig{};
  }
  calibrationSuggestedStartWeightKg_ = computeFirstCalibrationWeightKg();
  calibrationCurrentSetWeightKg_ = selectedWeightKg_;
  calibrationNextWeightKg_ = calibrationSuggestedStartWeightKg_;
  logEvent("CAL_LOAD",
           String("startSuggested=") + String(calibrationSuggestedStartWeightKg_, 1) +
               " pin=" + String(selectedWeightKg_, 1),
           LogLevel::Normal);
  logEvent("CAL",
           String("suggested start kg=") + String(calibrationSuggestedStartWeightKg_, 1) +
               " pinKg=" + String(selectedWeightKg_, 1),
           LogLevel::Normal);
  logEvent("WEIGHT",
           String("machine pin preserved kg=") + String(selectedWeightKg_, 1) +
               " reason=calibration_start_suggestion",
           LogLevel::Normal);

  idealGraphStartMs_ = millis();
  sessionMotionTemplateLatched_ = false;
  idealPhaseMs_ = 0;
  idealPhaseLastTickMs_ = 0;
  idealBottomStillSinceMs_ = 0;
  resetMotionGraph();

  transitionCalibrationState(CalibrationFlowState::ConfirmStartWeight, "skip_manual_rom_capture");
  setStatusMessage("Set the physical pin to the suggested load, then tap LOAD SET.");
  showUiScreen(UiScreenMode::Calibration);
  refreshUi();
}

void SmartGymTouchApp::transitionCalibrationState(CalibrationFlowState nextState, const String& reason) {
  calibrationFlowState_ = nextState;
  String stateName = "Idle";
  switch (nextState) {
    case CalibrationFlowState::Intro: stateName = "Intro"; break;
    case CalibrationFlowState::SetBottomRom: stateName = "SetBottomRom"; break;
    case CalibrationFlowState::SetTopRom: stateName = "SetTopRom"; break;
    case CalibrationFlowState::RecommendStartWeight: stateName = "RecommendStartWeight"; break;
    case CalibrationFlowState::ConfirmStartWeight: stateName = "ConfirmStartWeight"; break;
    case CalibrationFlowState::CollectSet: stateName = "CollectSet"; break;
    case CalibrationFlowState::AnalyzeSet: stateName = "AnalyzeSet"; break;
    case CalibrationFlowState::AskNextSet: stateName = "AskNextSet"; break;
    case CalibrationFlowState::Result: stateName = "Result"; break;
    case CalibrationFlowState::Saving: stateName = "Saving"; break;
    case CalibrationFlowState::Saved: stateName = "Saved"; break;
    case CalibrationFlowState::Cancelled: stateName = "Cancelled"; break;
    case CalibrationFlowState::Idle:
    default: break;
  }
  logEvent("CAL", String("state=") + stateName + (reason.isEmpty() ? "" : (" reason=" + reason)), LogLevel::Normal);
}

void SmartGymTouchApp::cancelCalibration(const String& reason) {
  const String uid = activeUser_ != nullptr ? activeUser_->rfidUid : String(anonymousMode_ ? "anonymous" : "none");
  const float pinKg = selectedWeightKg_;
  const float recommendationKg = resolvedRecommendation_.hasRecommendation ? resolvedRecommendation_.kg : 0.0f;
  const String recommendationSource = resolvedRecommendation_.hasRecommendation
                                          ? resolvedRecommendation_.source
                                          : String("none");
  const bool collectionActive = calibrationFlowState_ == CalibrationFlowState::CollectSet;
  logEvent("CAL",
           String("cancel begin state=") + stateToText() +
               " screen=" + uiScreenToText(currentUiScreen_) +
               (reason.isEmpty() ? "" : (" reason=" + reason)),
           LogLevel::Normal);
  logEvent("CAL",
           String("cancel stop collection active=") + String(collectionActive ? 1 : 0),
           LogLevel::Normal);

  calibrationService_.reset();
  repDetector_.reset();
  currentSetRepCount_ = 0;
  calibrationCurrentSetIndex_ = 0;
  calibrationValidRepsInSet_ = 0;
  calibrationRejectedRepsInSet_ = 0;
  calibrationRepVelocitySum_ = 0.0f;
  calibrationRepRomSum_ = 0.0f;
  calibrationRepMinRomSum_ = 0.0f;
  calibrationRepMaxRomSum_ = 0.0f;
  calibrationRepDurationSumMs_ = 0.0f;
  calibrationRepVelocityCount_ = 0;
  for (float& velocity : calibrationRepVelocities_) {
    velocity = 0.0f;
  }
  calibrationSetSnapshotCount_ = 0;
  calibrationHasUserWeightOverride_ = false;
  calibrationCurrentSetWeightKg_ = 0.0f;
  calibrationNextWeightKg_ = 0.0f;
  calibrationResultRecommendedKg_ = 0.0f;
  calibrationResultReason_ = "";
  calibrationResultAction_ = "keep";
  calibrationResultConfidence_ = "low";

  transitionCalibrationState(CalibrationFlowState::Cancelled, reason);
  state_ = State::Idle;
  setStatusMessage("Calibration cancelled.");
  pendingWorkoutAfterCalibration_ = false;
  userLoading_ = false;
  showUiScreen(UiScreenMode::Main);
  if (currentUiScreen_ == UiScreenMode::Calibration && screen_ != nullptr) {
    lv_scr_load(screen_);
    currentUiScreen_ = UiScreenMode::Main;
  }
  destroyCalibrationUiForCancel();
  transitionCalibrationState(CalibrationFlowState::Idle, "calibration_cancel_complete");
  logEvent("STATE", "READY stage=0 reason=calibration_cancel", LogLevel::Normal);
  logEvent("UI", "input unblocked reason=calibration_cancel", LogLevel::Normal);
  logEvent("WEIGHT",
           String("machine pin preserved kg=") + String(pinKg, 1) +
               " reason=calibration_cancel",
           LogLevel::Normal);
  logEvent("REC",
           String("recommendation preserved kg=") + String(recommendationKg, 1) +
               " source=" + recommendationSource,
           LogLevel::Normal);
  logEvent("CAL",
           String("cancel return ready user=") + uid +
               " pinKg=" + String(pinKg, 1) +
               " recommendationKg=" + String(recommendationKg, 1),
           LogLevel::Normal);
  refreshUi();
}

float SmartGymTouchApp::roundWeightToMachineIncrement(float kg) const {
  const float increment = machineProfile_ != nullptr && machineProfile_->machineIncrementKg > 0.0f
                              ? machineProfile_->machineIncrementKg
                              : 5.0f;
  const float minKg = machineProfile_ != nullptr ? machineProfile_->machineMinKg : kMinWeightKg;
  const float maxKg = machineProfile_ != nullptr ? machineProfile_->machineMaxKg : kMaxWeightKg;
  const float rounded = roundf(kg / increment) * increment;
  return constrain(rounded, minKg, maxKg);
}

bool SmartGymTouchApp::isDebugGoalSelectionEnabled() const {
  if (logLevel_ == LogLevel::Verbose) {
    return true;
  }
  if (currentUiScreen_ == UiScreenMode::Debug) {
    return true;
  }
  if (debugModal_ != nullptr && !lv_obj_has_flag(debugModal_, LV_OBJ_FLAG_HIDDEN)) {
    return true;
  }
  return false;
}

TrainingGoal SmartGymTouchApp::cycleProfileGoal(TrainingGoal current, int direction, bool allowTestGoal) const {
  static constexpr TrainingGoal kPublicGoals[] = {
      TrainingGoal::Strength, TrainingGoal::Hypertrophy, TrainingGoal::Endurance};
  static constexpr TrainingGoal kDebugGoals[] = {
      TrainingGoal::Strength, TrainingGoal::Hypertrophy, TrainingGoal::Endurance, TrainingGoal::Test};

  const TrainingGoal* goals = allowTestGoal ? kDebugGoals : kPublicGoals;
  const int count = allowTestGoal ? 4 : 3;
  int index = 1;  // Hypertrophy default
  for (int i = 0; i < count; ++i) {
    if (goals[i] == current) {
      index = i;
      break;
    }
  }
  index += direction;
  if (index < 0) {
    index = count - 1;
  } else if (index >= count) {
    index = 0;
  }
  return goals[index];
}

TrainingGoal SmartGymTouchApp::normalizeGoalId(TrainingGoal rawGoal, const char* source, bool allowTestGoal) {
  const String sourceText = source != nullptr ? String(source) : String("unknown");
  const String rawGoalText = UserRegistry::goalToString(rawGoal);
  const bool noisyRefreshSource =
      sourceText == "motion_curve" || sourceText == "ui_timing_text" || sourceText == "motion_targets";
  if (rawGoal == TrainingGoal::Strength ||
      rawGoal == TrainingGoal::Hypertrophy ||
      rawGoal == TrainingGoal::Endurance) {
    return rawGoal;
  }
  if (rawGoal == TrainingGoal::Test) {
    if (allowTestGoal) {
      const String key = String("accept:test:") + sourceText;
      if (!noisyRefreshSource && lastGoalNormalizationLogKey_ != key) {
        lastGoalNormalizationLogKey_ = key;
        logEvent("GOAL",
                 String("accepted debug goal raw=test source=") + sourceText,
                 LogLevel::Normal);
      }
      return TrainingGoal::Test;
    }
    const String key = String("fallback:test:") + sourceText;
    if (!noisyRefreshSource && lastGoalNormalizationLogKey_ != key) {
      lastGoalNormalizationLogKey_ = key;
      logEvent("GOAL",
               String("normalized raw=test fallback=hypertrophy source=") + sourceText +
                   " reason=test_not_allowed",
               LogLevel::Normal);
    }
    return TrainingGoal::Hypertrophy;
  }
  if (rawGoal == TrainingGoal::General) {
    const String key = String("fallback:general:") + sourceText;
    if (!noisyRefreshSource && lastGoalNormalizationLogKey_ != key) {
      lastGoalNormalizationLogKey_ = key;
      logEvent("GOAL",
               String("normalized raw=general fallback=hypertrophy source=legacy"),
               LogLevel::Normal);
    }
    return TrainingGoal::Hypertrophy;
  }
  const String key = String("invalid:") + rawGoalText + ":" + sourceText;
  if (!noisyRefreshSource && lastGoalNormalizationLogKey_ != key) {
    lastGoalNormalizationLogKey_ = key;
    logEvent("GOAL",
             String("invalid goal raw=") + rawGoalText +
                 " fallback=hypertrophy source=" + sourceText,
             LogLevel::Normal);
  }
  return TrainingGoal::Hypertrophy;
}

bool SmartGymTouchApp::deriveMotionTargetsForActiveMachine(MotionTargetConfig& outConfig) {
  const TrainingGoal rawGoal = activeUser_ != nullptr ? activeUser_->goal : TrainingGoal::Hypertrophy;
  const bool allowTestGoal = isDebugGoalSelectionEnabled();
  return deriveMotionTargetsForActiveMachine(outConfig, rawGoal, allowTestGoal, "motion_targets");
}

bool SmartGymTouchApp::deriveMotionTargetsForActiveMachine(MotionTargetConfig& outConfig,
                                                           TrainingGoal requestedGoal,
                                                           bool allowTestGoal,
                                                           const char* source) {
  if (machineProfile_ == nullptr) {
    return false;
  }
  const TrainingGoal goal = normalizeGoalId(requestedGoal, source, allowTestGoal);
  const String goalId = MachineRegistry::trainingGoalToGoalId(goal);
  return machineRegistry_.getMotionTargetsForMachineGoal(machineProfile_->machineTypeId, goalId, outConfig);
}

float SmartGymTouchApp::computeFirstCalibrationWeightKg() {
  if (machineProfile_ == nullptr) {
    calibrationStartWeightSource_ = "fallback";
    return 10.0f;
  }
  const float minKg = machineProfile_->machineMinKg > 0.0f ? machineProfile_->machineMinKg : 5.0f;
  const float maxKg = machineProfile_->machineMaxKg > minKg ? machineProfile_->machineMaxKg : 100.0f;
  if (activeUser_ != nullptr) {
    const uint32_t nowEpoch = cloudEnabled_ ? firebaseService_.getCurrentEpoch() : timeService_.getEpoch();
    WeeklyTrainingSignal signal;
    if (localPersistenceStore_.buildWeeklyTrainingSignal(activeUser_->rfidUid,
                                                         machineProfile_->machineId,
                                                         nowEpoch,
                                                         signal) &&
        signal.hasData &&
        signal.lastSelectedWeightKg > 0.0f) {
      const float recommended = roundWeightToMachineIncrement(signal.lastSelectedWeightKg * 0.65f);
      calibrationStartWeightSource_ = "history";
      logEvent("CAL", String("start weight source=history kg=") + String(recommended, 1), LogLevel::Normal);
      return constrain(recommended, minKg, maxKg);
    }
    if (activeCalibration_ != nullptr && activeCalibration_->hasCalibration) {
      const float previous = activeCalibration_->nextRecommendedWeightKg > 0.0f
                                 ? activeCalibration_->nextRecommendedWeightKg
                                 : activeCalibration_->suggestedWeightKg;
      if (previous > 0.0f) {
        const float recommended = roundWeightToMachineIncrement(previous * 0.75f);
        calibrationStartWeightSource_ = "previous_calibration";
        logEvent("CAL",
                 String("start weight source=previous_calibration kg=") + String(recommended, 1),
                 LogLevel::Normal);
        return constrain(recommended, minKg, maxKg);
      }
    }
  }
  if (machineProfile_->defaultSafeCalibrationKg > 0.0f) {
    const float recommended = roundWeightToMachineIncrement(machineProfile_->defaultSafeCalibrationKg);
    calibrationStartWeightSource_ = "machine_default";
    logEvent("CAL",
             String("start weight source=machine_default kg=") + String(recommended, 1),
             LogLevel::Normal);
    return constrain(recommended, minKg, maxKg);
  }
  const float fromMax = roundWeightToMachineIncrement(maxKg * 0.20f);
  if (fromMax > 0.0f) {
    calibrationStartWeightSource_ = "machine_percent";
    logEvent("CAL", String("start weight source=machine_percent kg=") + String(fromMax, 1), LogLevel::Normal);
    return constrain(fromMax, minKg, maxKg);
  }
  const float fallback = roundWeightToMachineIncrement(max(10.0f, minKg));
  calibrationStartWeightSource_ = "fallback";
  logEvent("CAL", String("start weight source=fallback kg=") + String(fallback, 1), LogLevel::Normal);
  return fallback;
}

void SmartGymTouchApp::updateCalibrationFromButtonPress(bool secondaryAction) {
  if (state_ != State::Calibration) {
    return;
  }
  calibrationActionBusyUntilMs_ = millis() + 320UL;
  if (secondaryAction) {
    if (calibrationFlowState_ == CalibrationFlowState::ConfirmStartWeight) {
      setStatusMessage("Use +/- to adjust load, then tap WEIGHT SET.");
      refreshUi();
      return;
    }
    if (calibrationFlowState_ == CalibrationFlowState::AskNextSet ||
        calibrationFlowState_ == CalibrationFlowState::Result) {
      transitionCalibrationState(CalibrationFlowState::Result, "user_finish");
      setStatusMessage("Calibration finished. Press CALIBRATE to save result.");
      refreshUi();
      return;
    }
    cancelCalibration("user_cancel");
    return;
  }

  switch (calibrationFlowState_) {
    case CalibrationFlowState::Intro:
    case CalibrationFlowState::SetBottomRom:
    case CalibrationFlowState::SetTopRom:
    case CalibrationFlowState::RecommendStartWeight:
      transitionCalibrationState(CalibrationFlowState::ConfirmStartWeight, "manual_rom_bypassed");
      setStatusMessage("Set the physical pin to the suggested load, then tap LOAD SET.");
      break;
    case CalibrationFlowState::ConfirmStartWeight:
      calibrationHasUserWeightOverride_ = fabsf(selectedWeightKg_ - calibrationSuggestedStartWeightKg_) > 0.01f;
      calibrationCurrentSetWeightKg_ = selectedWeightKg_;
      logEvent("CAL",
               String("load set confirmed set=1 loadKg=") +
                   String(calibrationCurrentSetWeightKg_, 1) +
                   " suggestedKg=" + String(calibrationSuggestedStartWeightKg_, 1),
               LogLevel::Normal);
      logEvent("CAL_LOAD",
               String("activeSetLoad set=1 kg=") +
                   String(calibrationCurrentSetWeightKg_, 1) +
                   " source=load_set",
               LogLevel::Normal);
      logEvent("CAL",
               String("user chose pin load kg=") + String(calibrationCurrentSetWeightKg_, 1) +
                   " suggestedKg=" + String(calibrationSuggestedStartWeightKg_, 1),
               LogLevel::Normal);
      if (calibrationHasUserWeightOverride_) {
        logEvent("CAL",
                 String("start weight overridden suggestedKg=") +
                     String(calibrationSuggestedStartWeightKg_, 1) +
                     " actualKg=" + String(calibrationCurrentSetWeightKg_, 1),
                 LogLevel::Normal);
      }
      calibrationCurrentSetIndex_ = 1;
      calibrationValidRepsInSet_ = 0;
      calibrationRejectedRepsInSet_ = 0;
      calibrationRepVelocitySum_ = 0.0f;
      calibrationRepRomSum_ = 0.0f;
      calibrationRepMinRomSum_ = 0.0f;
      calibrationRepMaxRomSum_ = 0.0f;
      calibrationRepDurationSumMs_ = 0.0f;
      calibrationRepVelocityCount_ = 0;
      transitionCalibrationState(CalibrationFlowState::CollectSet);
      setStatusMessage("Do 3 to 5 smooth full-range reps.");
      break;
    case CalibrationFlowState::AskNextSet:
      calibrationCurrentSetIndex_ = static_cast<uint8_t>(calibrationSetSnapshotCount_ + 1);
      logEvent("CAL_LOAD",
               String("nextSuggested=") + String(calibrationNextWeightKg_, 1) +
                   " previousSetLoad=" +
                   String(calibrationSetSnapshotCount_ > 0
                              ? calibrationSetSnapshots_[calibrationSetSnapshotCount_ - 1].selectedWeightKg
                              : 0.0f,
                          1) +
                   " pin=" + String(selectedWeightKg_, 1),
               LogLevel::Normal);
      if (calibrationSetSnapshotCount_ > 0 &&
          fabsf(selectedWeightKg_ - calibrationSetSnapshots_[calibrationSetSnapshotCount_ - 1].selectedWeightKg) < 0.01f) {
        logEvent("CAL",
                 String("same-load repeat pinKg=") + String(selectedWeightKg_, 1) +
                     " suggestedKg=" + String(calibrationNextWeightKg_, 1),
                 LogLevel::Normal);
        logEvent("CAL",
                 String("user kept current pin for next set pinKg=") + String(selectedWeightKg_, 1) +
                     " suggestedKg=" + String(calibrationNextWeightKg_, 1),
                 LogLevel::Normal);
      } else {
        logEvent("CAL",
                 String("next load confirmed pinKg=") + String(selectedWeightKg_, 1) +
                     " suggestedKg=" + String(calibrationNextWeightKg_, 1),
                 LogLevel::Normal);
      }
      calibrationCurrentSetWeightKg_ = selectedWeightKg_;
      logEvent("CAL",
               String("load set confirmed set=") + String(calibrationCurrentSetIndex_) +
                   " loadKg=" + String(calibrationCurrentSetWeightKg_, 1) +
                   " suggestedKg=" + String(calibrationNextWeightKg_, 1),
               LogLevel::Normal);
      logEvent("CAL_LOAD",
               String("activeSetLoad set=") + String(calibrationCurrentSetIndex_) +
                   " kg=" + String(calibrationCurrentSetWeightKg_, 1) +
                   " source=load_set",
               LogLevel::Normal);
      logEvent("WEIGHT",
               String("machine pin preserved kg=") + String(selectedWeightKg_, 1) +
                   " reason=calibration_next_set_suggestion suggestedKg=" +
                   String(calibrationNextWeightKg_, 1),
               LogLevel::Normal);
      calibrationValidRepsInSet_ = 0;
      calibrationRejectedRepsInSet_ = 0;
      calibrationRepVelocitySum_ = 0.0f;
      calibrationRepRomSum_ = 0.0f;
      calibrationRepMinRomSum_ = 0.0f;
      calibrationRepMaxRomSum_ = 0.0f;
      calibrationRepDurationSumMs_ = 0.0f;
      calibrationRepVelocityCount_ = 0;
      transitionCalibrationState(CalibrationFlowState::CollectSet);
      setStatusMessage("Set pin if needed, then do 3 to 5 smooth reps at " +
                       String(calibrationCurrentSetWeightKg_, 1) + " kg.");
      break;
    case CalibrationFlowState::Result: {
      transitionCalibrationState(CalibrationFlowState::Saving);
      if (activeUser_ != nullptr && machineProfile_ != nullptr) {
        UserMachineCalibration* localCalibration = userRegistry_.findCalibration(*activeUser_, machineProfile_->machineTypeId);
        if (localCalibration == nullptr) {
          localCalibration = userRegistry_.upsertCalibration(*activeUser_,
                                                             machineProfile_->machineTypeId,
                                                             calibrationResultRecommendedKg_,
                                                             userRomTopCapturePct_ - userRomBottomCapturePct_,
                                                             userRomBottomCapturePct_,
                                                             userRomTopCapturePct_);
        }
        if (localCalibration != nullptr) {
          localCalibration->schemaVersion = 1;
          localCalibration->source = "unified_velocity_calibration_v1";
          localCalibration->userUid = activeUser_->rfidUid;
          localCalibration->machineTypeId = machineProfile_->machineTypeId;
          localCalibration->goalId = MachineRegistry::trainingGoalToGoalId(
              normalizeGoalId(activeUser_->goal, "calibration_save", isDebugGoalSelectionEnabled()));
          localCalibration->hasCalibration = true;
          localCalibration->userBottomPct = userRomBottomCapturePct_;
          localCalibration->userTopPct = userRomTopCapturePct_;
          localCalibration->userRomPercent = max(0.0f, userRomTopCapturePct_ - userRomBottomCapturePct_);
          localCalibration->romBottomRaw = userRomBottomCapturePct_;
          localCalibration->romTopRaw = userRomTopCapturePct_;
          localCalibration->romRangeRaw = localCalibration->userRomPercent;
          localCalibration->romRangePct = localCalibration->userRomPercent;
          localCalibration->romValid = true;
          localCalibration->machineIncrementKg = machineProfile_->machineIncrementKg;
          localCalibration->machineMinKg = machineProfile_->machineMinKg;
          localCalibration->machineMaxKg = machineProfile_->machineMaxKg;
          localCalibration->startWeightSource = calibrationStartWeightSource_;
          localCalibration->suggestedStartWeightKg = calibrationSuggestedStartWeightKg_;
          localCalibration->actualFirstSetWeightKg = calibrationCurrentSetWeightKg_;
          localCalibration->userOverrodeStartWeight = calibrationHasUserWeightOverride_;
          localCalibration->resultRecommendedWeightKg = calibrationResultRecommendedKg_;
          localCalibration->suggestedWeightKg = calibrationResultRecommendedKg_;
          localCalibration->nextRecommendationSource = "calibration";
          localCalibration->nextRecommendationReason = calibrationResultReason_;
          localCalibration->nextRecommendationUpdatedAt = firebaseService_.getCurrentEpoch();
          localCalibration->resultAction = UserRegistry::calibrationActionFromString(calibrationResultAction_);
          localCalibration->resultConfidence = UserRegistry::calibrationConfidenceFromString(calibrationResultConfidence_);
          localCalibration->resultReasonText = calibrationResultReason_;
          localCalibration->motionTargetsUsed.targetRepsMin = calibrationMotionTargets_.targetRepsMin;
          localCalibration->motionTargetsUsed.targetRepsMax = calibrationMotionTargets_.targetRepsMax;
          localCalibration->motionTargetsUsed.targetSetsMin = calibrationMotionTargets_.targetSetsMin;
          localCalibration->motionTargetsUsed.targetSetsMax = calibrationMotionTargets_.targetSetsMax;
          localCalibration->motionTargetsUsed.targetSetsDefault = calibrationMotionTargets_.targetSetsDefault;
          localCalibration->motionTargetsUsed.restSecondsDefault = calibrationMotionTargets_.restSecondsDefault;
          localCalibration->motionTargetsUsed.riseTimeSecMin = calibrationMotionTargets_.riseTimeSecMin;
          localCalibration->motionTargetsUsed.riseTimeSecMax = calibrationMotionTargets_.riseTimeSecMax;
          localCalibration->motionTargetsUsed.riseTimeSecDefault = calibrationMotionTargets_.riseTimeSecDefault;
          localCalibration->motionTargetsUsed.lowerTimeSecMin = calibrationMotionTargets_.lowerTimeSecMin;
          localCalibration->motionTargetsUsed.lowerTimeSecMax = calibrationMotionTargets_.lowerTimeSecMax;
          localCalibration->motionTargetsUsed.lowerTimeSecDefault = calibrationMotionTargets_.lowerTimeSecDefault;
          localCalibration->motionTargetsUsed.topPauseSec = calibrationMotionTargets_.topPauseSec;
          localCalibration->motionTargetsUsed.bottomPauseSec = calibrationMotionTargets_.bottomPauseSec;
          localCalibration->calibrationSetCount = calibrationSetSnapshotCount_;
          for (uint8_t i = 0; i < calibrationSetSnapshotCount_; ++i) {
            localCalibration->calibrationSets[i] = calibrationSetSnapshots_[i];
          }
          float strengthKg = roundWeightToMachineIncrement(calibrationResultRecommendedKg_ * 1.15f);
          float hypertrophyKg = roundWeightToMachineIncrement(calibrationResultRecommendedKg_);
          float enduranceKg = roundWeightToMachineIncrement(calibrationResultRecommendedKg_ * 0.80f);
          float regressionSlope = 0.0f;
          float regressionIntercept = 0.0f;
          float estimatedOneRepMaxKg = 0.0f;
          bool modelValid = false;
          String modelInvalidReason = "insufficient_valid_sets";
          float heaviestCleanKg = 0.0f;
          float latestCleanKg = 0.0f;
          CalibrationSetClassification latestCleanClass = CalibrationSetClassification::Unknown;
          static constexpr uint8_t kMaxGroupedCalibrationLoads = UserMachineCalibration::kMaxCalibrationSets;
          float groupedLoadKg[kMaxGroupedCalibrationLoads] = {};
          float groupedVelocitySum[kMaxGroupedCalibrationLoads] = {};
          uint8_t groupedSetCount[kMaxGroupedCalibrationLoads] = {};
          uint8_t groupedCount = 0;
          float sumX = 0.0f;
          float sumY = 0.0f;
          float sumXY = 0.0f;
          float sumXX = 0.0f;
          for (uint8_t i = 0; i < calibrationSetSnapshotCount_; ++i) {
            const CalibrationSetSnapshot& set = calibrationSetSnapshots_[i];
            if (set.validRepCount < kCalibrationMinValidReps || set.selectedWeightKg <= 0.0f ||
                set.medianConcentricVelocityPctPerSec <= 0.0f) {
              continue;
            }
            const bool cleanForFallback =
                set.avgRomPercent >= 50.0f &&
                set.classification != CalibrationSetClassification::NoisyInvalid &&
                set.classification != CalibrationSetClassification::TooHeavyOrInvalid;
            if (cleanForFallback) {
              heaviestCleanKg = max(heaviestCleanKg, set.selectedWeightKg);
              latestCleanKg = set.selectedWeightKg;
              latestCleanClass = set.classification;
            }
            logEvent("CAL_MODEL",
                     String("set load=") + String(set.selectedWeightKg, 2) +
                         " vel=" + String(set.medianConcentricVelocityPctPerSec, 2) +
                         " romRange=" + String(set.avgRomPercent, 2) +
                         " valid=" + String(set.validRepCount),
                     LogLevel::Normal);
            bool grouped = false;
            for (uint8_t g = 0; g < groupedCount; ++g) {
              if (fabsf(groupedLoadKg[g] - set.selectedWeightKg) < 0.01f) {
                groupedVelocitySum[g] += set.medianConcentricVelocityPctPerSec;
                groupedSetCount[g]++;
                grouped = true;
                break;
              }
            }
            if (!grouped && groupedCount < kMaxGroupedCalibrationLoads) {
              groupedLoadKg[groupedCount] = set.selectedWeightKg;
              groupedVelocitySum[groupedCount] = set.medianConcentricVelocityPctPerSec;
              groupedSetCount[groupedCount] = 1;
              groupedCount++;
            }
          }
          for (uint8_t g = 0; g < groupedCount; ++g) {
            const float groupedVelocity = groupedVelocitySum[g] / max<uint8_t>(1, groupedSetCount[g]);
            logEvent("CAL_MODEL",
                     String("grouped load=") + String(groupedLoadKg[g], 1) +
                         " sets=" + String(groupedSetCount[g]) +
                         " vel=" + String(groupedVelocity, 2),
                     LogLevel::Normal);
            sumX += groupedLoadKg[g];
            sumY += groupedVelocity;
            sumXY += groupedLoadKg[g] * groupedVelocity;
            sumXX += groupedLoadKg[g] * groupedLoadKg[g];
          }
          if (groupedCount >= 2) {
            const float denom = (static_cast<float>(groupedCount) * sumXX) - (sumX * sumX);
            if (fabsf(denom) > 0.001f) {
              regressionSlope = ((static_cast<float>(groupedCount) * sumXY) - (sumX * sumY)) / denom;
              regressionIntercept = (sumY - (regressionSlope * sumX)) / static_cast<float>(groupedCount);
              const float mvtPctPerSec = 30.0f;
              if (regressionSlope < -0.001f) {
                estimatedOneRepMaxKg = roundWeightToMachineIncrement((mvtPctPerSec - regressionIntercept) /
                                                                     regressionSlope);
                const float maxObservedLoad = calibrationSetSnapshotCount_ > 0
                                                  ? calibrationSetSnapshots_[calibrationSetSnapshotCount_ - 1].selectedWeightKg
                                                  : calibrationResultRecommendedKg_;
                const float safeUpper = machineProfile_->machineMaxKg > 0.0f
                                            ? machineProfile_->machineMaxKg
                                            : maxObservedLoad * 1.5f;
                if (estimatedOneRepMaxKg >= maxObservedLoad && estimatedOneRepMaxKg <= safeUpper) {
                  modelValid = true;
                  strengthKg = roundWeightToMachineIncrement(estimatedOneRepMaxKg * 0.85f);
                  hypertrophyKg = roundWeightToMachineIncrement(estimatedOneRepMaxKg * 0.70f);
                  enduranceKg = roundWeightToMachineIncrement(estimatedOneRepMaxKg * 0.55f);
                } else {
                  modelInvalidReason = "impossible_1rm";
                }
              } else {
                modelInvalidReason = "positive_slope";
                logEvent("CAL_MODEL",
                         String("trend invalid reason=positive_slope b=") + String(regressionSlope, 6),
                         LogLevel::Normal);
              }
            } else {
              modelInvalidReason = "singular_regression";
            }
          }
          if (!modelValid) {
            const float increment = machineProfile_ != nullptr && machineProfile_->machineIncrementKg > 0.0f
                                        ? machineProfile_->machineIncrementKg
                                        : kWeightStepKg;
            const float fallbackBase = heaviestCleanKg > 0.0f ? heaviestCleanKg : calibrationCurrentSetWeightKg_;
            float cappedRecommendation = fallbackBase;
            if (latestCleanClass == CalibrationSetClassification::VeryLight ||
                latestCleanClass == CalibrationSetClassification::Light) {
              cappedRecommendation = roundWeightToMachineIncrement(fallbackBase + increment);
            } else if (latestCleanClass == CalibrationSetClassification::Moderate) {
              cappedRecommendation = latestCleanKg > 0.0f ? latestCleanKg : fallbackBase;
            } else if (latestCleanClass == CalibrationSetClassification::HeavyButUsable) {
              cappedRecommendation = latestCleanKg > 0.0f ? latestCleanKg : fallbackBase;
            }
            const float maxAllowed = roundWeightToMachineIncrement(fallbackBase + increment);
            const float oldRecommendation = calibrationResultRecommendedKg_;
            cappedRecommendation = min(cappedRecommendation, maxAllowed);
            cappedRecommendation = roundWeightToMachineIncrement(cappedRecommendation);
            calibrationResultRecommendedKg_ = cappedRecommendation;
            calibrationResultConfidence_ = "low";
            calibrationResultReason_ = "Basic calibration. Low confidence trend.";
            calibrationResultAction_ = cappedRecommendation > fallbackBase + 0.01f ? "increase" : "keep";
            localCalibration->resultRecommendedWeightKg = calibrationResultRecommendedKg_;
            localCalibration->suggestedWeightKg = calibrationResultRecommendedKg_;
            localCalibration->nextRecommendationReason = calibrationResultReason_;
            localCalibration->resultAction = UserRegistry::calibrationActionFromString(calibrationResultAction_);
            localCalibration->resultConfidence = UserRegistry::calibrationConfidenceFromString(calibrationResultConfidence_);
            localCalibration->resultReasonText = calibrationResultReason_;
            logEvent("CAL_MODEL", String("fallback reason=") + modelInvalidReason, LogLevel::Normal);
            logEvent("CAL_MODEL",
                     String("fallback heaviestCleanKg=") + String(heaviestCleanKg, 1) +
                         " increment=" + String(increment, 1) +
                         " cappedRecommendedKg=" + String(cappedRecommendation, 1),
                     LogLevel::Normal);
            if (oldRecommendation > cappedRecommendation + 0.01f) {
              logEvent("CAL_MODEL",
                       String("low_confidence_cap applied oldKg=") + String(oldRecommendation, 1) +
                           " newKg=" + String(cappedRecommendation, 1),
                       LogLevel::Normal);
            }
            logEvent("CAL_MODEL",
                     String("no_1rm reason=") + modelInvalidReason,
                     LogLevel::Normal);
            strengthKg = roundWeightToMachineIncrement(calibrationResultRecommendedKg_);
            hypertrophyKg = roundWeightToMachineIncrement(calibrationResultRecommendedKg_);
            enduranceKg = roundWeightToMachineIncrement(max(machineProfile_->machineMinKg,
                                                            calibrationResultRecommendedKg_ - increment));
            logEvent("CAL_MODEL",
                     String("goal fallback strength=") + String(strengthKg, 1) +
                         " hypertrophy=" + String(hypertrophyKg, 1) +
                         " endurance=" + String(enduranceKg, 1),
                     LogLevel::Normal);
          }
          strengthKg = roundWeightToMachineIncrement(strengthKg);
          hypertrophyKg = roundWeightToMachineIncrement(hypertrophyKg);
          enduranceKg = roundWeightToMachineIncrement(enduranceKg);
          localCalibration->estimatedOneRepMaxKg = modelValid ? estimatedOneRepMaxKg : 0.0f;
          localCalibration->estimatedOneRepMaxConfidence = modelValid ? calibrationResultConfidence_ : String("low");
          localCalibration->velocityLoadSlope = regressionSlope;
          localCalibration->velocityLoadIntercept = regressionIntercept;
          localCalibration->calibrationModelValid = modelValid;
          localCalibration->strengthRecommendedKg = strengthKg;
          localCalibration->hypertrophyRecommendedKg = hypertrophyKg;
          localCalibration->enduranceRecommendedKg = enduranceKg;
          localCalibration->activeGoalId = localCalibration->goalId;
          if (localCalibration->goalId.equalsIgnoreCase("strength")) {
            localCalibration->activeGoalRecommendedKg = strengthKg;
          } else if (localCalibration->goalId.equalsIgnoreCase("endurance")) {
            localCalibration->activeGoalRecommendedKg = enduranceKg;
          } else {
            localCalibration->activeGoalRecommendedKg = hypertrophyKg;
          }
          localCalibration->nextRecommendedWeightKg =
              localCalibration->activeGoalRecommendedKg > 0.0f
                  ? localCalibration->activeGoalRecommendedKg
                  : calibrationResultRecommendedKg_;
          logEvent("CAL_MODEL",
                   String("regression distinctLoads=") + String(groupedCount) +
                       " a=" + String(regressionIntercept, 3) +
                       " b=" + String(regressionSlope, 6) +
                       " valid=" + String(modelValid ? 1 : 0),
                   LogLevel::Normal);
          logEvent("CAL_MODEL",
                   String("mvtPct=30.00 estimated1RM=") + String(localCalibration->estimatedOneRepMaxKg, 1) +
                       " confidence=" + localCalibration->estimatedOneRepMaxConfidence,
                   LogLevel::Normal);
          logEvent("CAL_MODEL",
                   String("goal strength=") + String(strengthKg, 1) +
                       " hypertrophy=" + String(hypertrophyKg, 1) +
                       " endurance=" + String(enduranceKg, 1),
                   LogLevel::Normal);
          localCalibration->updatedAtEpoch = firebaseService_.getCurrentEpoch();
          localCalibration->updatedAtIso = firebaseService_.getCurrentIso();
          activeCalibration_ = localCalibration;
          saveUsers();
          syncActiveCalibrationToCloud();
          configureRepDetectorThresholds();
          logEvent("WEIGHT",
                   String("machine pin preserved kg=") + String(selectedWeightKg_, 1) +
                       " reason=calibration_saved",
                   LogLevel::Normal);
          refreshResolvedRecommendation(false, "calibration_saved");
          logEvent("REC",
                   String("calibration saved kg=") +
                       String(localCalibration->nextRecommendedWeightKg > 0.0f
                                  ? localCalibration->nextRecommendedWeightKg
                                  : localCalibration->resultRecommendedWeightKg,
                              1) +
                       " goal=" + localCalibration->goalId +
                       " confidence=" + calibrationResultConfidence_,
                   LogLevel::Normal);
          logEvent("CAL",
                   String("saved recommendedKg=") + String(localCalibration->resultRecommendedWeightKg, 1) +
                       " confidence=" + calibrationResultConfidence_ +
                       " action=" + calibrationResultAction_,
                   LogLevel::Normal);
        }
      }
      transitionCalibrationState(CalibrationFlowState::Saved);
      state_ = State::Idle;
      pendingWorkoutAfterCalibration_ = false;
      calibrationStartBlockedUntilMs_ = millis() + 1200UL;
      calibrationActionBusyUntilMs_ = millis() + 650UL;
      setStatusMessage("Calibration saved.");
      showUiScreen(UiScreenMode::Main);
      destroyCalibrationUiForCancel("calibration_saved");
      break;
    }
    case CalibrationFlowState::Saved:
      state_ = State::Idle;
      break;
    case CalibrationFlowState::Saving:
    case CalibrationFlowState::CollectSet:
    case CalibrationFlowState::AnalyzeSet:
    case CalibrationFlowState::Cancelled:
    case CalibrationFlowState::Idle:
    default:
      break;
  }
  refreshUi();
}

void SmartGymTouchApp::processCalibrationRep(const RepMetrics& rep, uint32_t nowMs) {
  (void)nowMs;
  if (calibrationFlowState_ != CalibrationFlowState::CollectSet) {
    return;
  }
  if (!rep.valid || rep.concentricTimeMs == 0 || rep.romPercent < 40.0f) {
    calibrationRejectedRepsInSet_++;
    logEvent("CAL", String("rep rejected reason=") + buildInvalidReason(rep), LogLevel::Normal);
    logEvent("CAL_UI",
             String("reps valid=") + String(calibrationValidRepsInSet_) +
                 " rejected=" + String(calibrationRejectedRepsInSet_),
             LogLevel::Normal);
    logEvent("CAL_UI", String("feedback=") + String("Motion noisy, repeat"), LogLevel::Normal);
    setStatusMessage("Calibration rep rejected: " + buildInvalidReason(rep));
    return;
  }

  const float concentricSec = static_cast<float>(rep.concentricTimeMs) / 1000.0f;
  const float velocityPctPerSec = concentricSec > 0.0f ? rep.romPercent / concentricSec : 0.0f;
  bool velocityUsableForModel = true;
  if (rep.concentricTimeMs < kCalibrationModelMinConcentricMs) {
    velocityUsableForModel = false;
    logEvent("CAL",
             String("rep velocity ignored reason=duration_too_short durationMs=") +
                 String(rep.concentricTimeMs),
             LogLevel::Normal);
  } else if (velocityPctPerSec > kCalibrationModelMaxVelocityPctPerSec) {
    velocityUsableForModel = false;
    logEvent("CAL",
             String("rep velocity ignored reason=outlier velocity=") +
                 String(velocityPctPerSec, 2) +
                 " median=0.00",
             LogLevel::Normal);
  }
  if (velocityUsableForModel &&
      calibrationRepVelocityCount_ < (sizeof(calibrationRepVelocities_) / sizeof(calibrationRepVelocities_[0]))) {
    calibrationRepVelocities_[calibrationRepVelocityCount_++] = velocityPctPerSec;
    calibrationRepVelocitySum_ += velocityPctPerSec;
  }
  calibrationRepRomSum_ += rep.romPercent;
  calibrationRepMinRomSum_ += rep.minRomPercent;
  calibrationRepMaxRomSum_ += rep.maxRomPercent;
  calibrationRepDurationSumMs_ += static_cast<float>(rep.concentricTimeMs);
  calibrationValidRepsInSet_++;
  logEvent("CAL",
           String("rep accepted index=") + String(calibrationValidRepsInSet_) +
               " velocityPct=" + String(velocityPctPerSec, 2) +
               " romPct=" + String(rep.romPercent, 2) +
               " low=" + String(rep.minRomPercent, 2) +
               " high=" + String(rep.maxRomPercent, 2) +
               " durationMs=" + String(rep.concentricTimeMs),
           LogLevel::Normal);
  logEvent("CAL",
           String("rep rom low=") + String(rep.minRomPercent, 2) +
               " high=" + String(rep.maxRomPercent, 2) +
               " range=" + String(rep.romPercent, 2) +
               " valid=1",
           LogLevel::Normal);
  logEvent("CAL_UI",
           String("reps valid=") + String(calibrationValidRepsInSet_) +
               " rejected=" + String(calibrationRejectedRepsInSet_),
           LogLevel::Normal);
  logEvent("CAL_UI", String("feedback=") + String("Good rep"), LogLevel::Normal);

  if (calibrationValidRepsInSet_ < calibrationTargetRepsPerSet_) {
    setStatusMessage("Calibration set " + String(calibrationCurrentSetIndex_) +
                     ": accepted " + String(calibrationValidRepsInSet_) + "/" +
                     String(calibrationTargetRepsPerSet_));
    return;
  }

  transitionCalibrationState(CalibrationFlowState::AnalyzeSet);
  const float avgRom = calibrationRepRomSum_ / max<uint8_t>(1, calibrationValidRepsInSet_);
  const float measuredBottom = calibrationRepMinRomSum_ / max<uint8_t>(1, calibrationValidRepsInSet_);
  const float measuredTop = calibrationRepMaxRomSum_ / max<uint8_t>(1, calibrationValidRepsInSet_);
  const float measuredRange = max(0.0f, measuredTop - measuredBottom);
  if (measuredRange > 0.0f) {
    userRomBottomCapturePct_ = measuredBottom;
    userRomTopCapturePct_ = measuredTop;
    userRomBottomCaptured_ = true;
    userRomTopCaptured_ = true;
    logEvent("CAL",
             String("measured rom bottom=") + String(measuredBottom, 2) +
                 " top=" + String(measuredTop, 2) +
                 " range=" + String(measuredRange, 2) +
                 " reps=" + String(calibrationValidRepsInSet_),
             LogLevel::Normal);
  }
  const float avgDuration = calibrationRepDurationSumMs_ / max<uint8_t>(1, calibrationValidRepsInSet_);
  float avgV = calibrationRepVelocityCount_ > 0
                   ? calibrationRepVelocitySum_ / static_cast<float>(calibrationRepVelocityCount_)
                   : 0.0f;
  float stdDev = 0.0f;
  float medianVelocity = avgV;
  if (calibrationRepVelocityCount_ > 0) {
    float sorted[sizeof(calibrationRepVelocities_) / sizeof(calibrationRepVelocities_[0])] = {};
    for (uint8_t i = 0; i < calibrationRepVelocityCount_; ++i) {
      sorted[i] = calibrationRepVelocities_[i];
    }
    for (uint8_t i = 0; i < calibrationRepVelocityCount_; ++i) {
      for (uint8_t j = i + 1; j < calibrationRepVelocityCount_; ++j) {
        if (sorted[j] < sorted[i]) {
          const float tmp = sorted[i];
          sorted[i] = sorted[j];
          sorted[j] = tmp;
        }
      }
    }
    medianVelocity = sorted[calibrationRepVelocityCount_ / 2];
    float filteredSum = 0.0f;
    uint8_t filteredCount = 0;
    float filteredVelocities[sizeof(calibrationRepVelocities_) / sizeof(calibrationRepVelocities_[0])] = {};
    for (uint8_t i = 0; i < calibrationRepVelocityCount_; ++i) {
      const float v = calibrationRepVelocities_[i];
      const bool ratioOutlier = medianVelocity > 0.0f &&
                                v > max(medianVelocity * 2.25f, medianVelocity + 180.0f);
      if (ratioOutlier) {
        logEvent("CAL",
                 String("rep velocity ignored reason=outlier velocity=") +
                     String(v, 2) +
                     " median=" + String(medianVelocity, 2),
                 LogLevel::Normal);
        continue;
      }
      filteredSum += v;
      filteredVelocities[filteredCount] = v;
      filteredCount++;
    }
    if (filteredCount > 0 && filteredCount != calibrationRepVelocityCount_) {
      for (uint8_t i = 0; i < filteredCount; ++i) {
        calibrationRepVelocities_[i] = filteredVelocities[i];
      }
      avgV = filteredSum / static_cast<float>(filteredCount);
      calibrationRepVelocityCount_ = filteredCount;
      calibrationRepVelocitySum_ = filteredSum;
    }
    float variance = 0.0f;
    for (uint8_t i = 0; i < calibrationRepVelocityCount_; ++i) {
      const float d = calibrationRepVelocities_[i] - avgV;
      variance += d * d;
    }
    variance /= max<uint8_t>(1, calibrationRepVelocityCount_);
    stdDev = sqrtf(variance);
  }
  logEvent("CAL_MODEL",
           String("set load=") + String(calibrationCurrentSetWeightKg_, 1) +
               " medianVel=" + String(medianVelocity, 2) +
               " avgVel=" + String(avgV, 2) +
               " usedReps=" + String(calibrationRepVelocityCount_),
           LogLevel::Normal);
  CalibrationSetClassification classification = CalibrationSetClassification::Unknown;
  float nextWeight = calibrationCurrentSetWeightKg_;
  String reason = "balanced set";
  String action = "keep";
  String confidence = "medium";

  if (calibrationRepVelocityCount_ == 0) {
    classification = CalibrationSetClassification::NoisyInvalid;
    nextWeight = calibrationCurrentSetWeightKg_;
    reason = "Velocity samples were not reliable.";
    action = "repeat_calibration";
    confidence = "low";
  } else if (calibrationValidRepsInSet_ < kCalibrationMinValidReps) {
    classification = CalibrationSetClassification::NoisyInvalid;
    nextWeight = roundWeightToMachineIncrement(calibrationCurrentSetWeightKg_ * 0.85f);
    reason = "Not enough valid reps.";
    action = "repeat_calibration";
    confidence = "low";
  } else if (measuredRange < 50.0f) {
    classification = CalibrationSetClassification::NoisyInvalid;
    nextWeight = roundWeightToMachineIncrement(calibrationCurrentSetWeightKg_ * 0.9f);
    reason = "Range of motion was too small.";
    action = "decrease";
    confidence = "low";
    logEvent("CAL",
             String("repeat reason=rom_range_too_small range=") + String(measuredRange, 2),
             LogLevel::Normal);
  } else if (avgV > 170.0f) {
    classification = CalibrationSetClassification::VeryLight;
    nextWeight = roundWeightToMachineIncrement(calibrationCurrentSetWeightKg_ * 1.35f);
    reason = "Very fast concentric velocity.";
    action = "increase";
  } else if (avgV > 110.0f) {
    classification = CalibrationSetClassification::Light;
    nextWeight = roundWeightToMachineIncrement(calibrationCurrentSetWeightKg_ * 1.20f);
    reason = "Light set with room to increase.";
    action = "increase";
  } else if (avgV >= 70.0f) {
    classification = CalibrationSetClassification::Moderate;
    nextWeight = roundWeightToMachineIncrement(calibrationCurrentSetWeightKg_ * 1.08f);
    reason = "Moderate velocity and good ROM.";
    action = "keep";
  } else if (avgV >= 45.0f) {
    classification = CalibrationSetClassification::HeavyButUsable;
    nextWeight = roundWeightToMachineIncrement(calibrationCurrentSetWeightKg_ * 0.95f);
    reason = "Heavy but still usable.";
    action = "keep";
  } else {
    classification = CalibrationSetClassification::TooHeavyOrInvalid;
    nextWeight = roundWeightToMachineIncrement(calibrationCurrentSetWeightKg_ * 0.85f);
    reason = "Too slow for calibration quality.";
    action = "decrease";
  }

  if (stdDev > 35.0f) {
    confidence = "low";
  } else if (calibrationSetSnapshotCount_ >= 1 && stdDev < 20.0f) {
    confidence = "high";
  }

  if ((classification == CalibrationSetClassification::VeryLight ||
       classification == CalibrationSetClassification::Light) &&
      nextWeight <= calibrationCurrentSetWeightKg_ + 0.01f) {
    const float oldSuggested = nextWeight;
    const float increment = machineProfile_ != nullptr && machineProfile_->machineIncrementKg > 0.0f
                                ? machineProfile_->machineIncrementKg
                                : kWeightStepKg;
    const float adjusted = roundWeightToMachineIncrement(calibrationCurrentSetWeightKg_ + increment);
    if (adjusted > calibrationCurrentSetWeightKg_ + 0.01f) {
      nextWeight = adjusted;
      logEvent("CAL",
               String("next load adjusted reason=same_as_current oldSuggested=") +
                   String(oldSuggested, 1) +
                   " newSuggested=" + String(nextWeight, 1),
               LogLevel::Normal);
    } else {
      logEvent("CAL",
               String("next load skipped reason=same_as_current currentKg=") +
                   String(calibrationCurrentSetWeightKg_, 1),
               LogLevel::Normal);
      classification = CalibrationSetClassification::Moderate;
      reason = "Good set. Save now or repeat once for better accuracy.";
      action = "keep";
      nextWeight = calibrationCurrentSetWeightKg_;
    }
  }

  CalibrationSetSnapshot snapshot;
  snapshot.setIndex = calibrationCurrentSetIndex_;
  snapshot.selectedWeightKg = calibrationCurrentSetWeightKg_;
  snapshot.targetRepCount = calibrationTargetRepsPerSet_;
  snapshot.validRepCount = calibrationValidRepsInSet_;
  snapshot.rejectedRepCount = calibrationRejectedRepsInSet_;
  snapshot.avgConcentricVelocityPctPerSec = avgV;
  snapshot.medianConcentricVelocityPctPerSec = medianVelocity;
  snapshot.avgConcentricDurationMs = avgDuration;
  snapshot.avgRomPercent = avgRom;
  snapshot.velocityStdDevPctPerSec = stdDev;
  snapshot.qualityScore = constrain((avgRom * 0.6f) + constrain(avgV, 0.0f, 140.0f) * 0.4f, 0.0f, 100.0f);
  snapshot.classification = classification;
  snapshot.suggestedNextWeightKg = nextWeight;
  snapshot.reasonText = reason;
  if (calibrationSetSnapshotCount_ < UserMachineCalibration::kMaxCalibrationSets) {
    calibrationSetSnapshots_[calibrationSetSnapshotCount_++] = snapshot;
  }

  logEvent("CAL",
           String("classify set=") + String(snapshot.setIndex) +
               " avgV=" + String(avgV, 2) +
               " avgRom=" + String(avgRom, 2) +
               " validReps=" + String(snapshot.validRepCount) +
               " rejectedReps=" + String(snapshot.rejectedRepCount) +
               " class=" + UserRegistry::calibrationSetClassificationToString(snapshot.classification),
           LogLevel::Normal);

  calibrationResultRecommendedKg_ = nextWeight;
  calibrationResultReason_ = reason;
  calibrationResultAction_ = action;
  calibrationResultConfidence_ = confidence;
  calibrationNextWeightKg_ = nextWeight;
  logEvent("CAL_LOAD",
           String("nextSuggested=") + String(calibrationNextWeightKg_, 1) +
               " previousSetLoad=" + String(calibrationCurrentSetWeightKg_, 1) +
               " pin=" + String(selectedWeightKg_, 1),
           LogLevel::Normal);

  const bool requestNextSet =
      (classification == CalibrationSetClassification::VeryLight ||
       classification == CalibrationSetClassification::Light) &&
      calibrationSetSnapshotCount_ < UserMachineCalibration::kMaxCalibrationSets;

  if (requestNextSet) {
    transitionCalibrationState(CalibrationFlowState::AskNextSet);
    setStatusMessage("This looks light. Increase to " + String(calibrationNextWeightKg_, 1) +
                     " kg if you want a better estimate.");
  } else {
    transitionCalibrationState(CalibrationFlowState::Result);
    setStatusMessage("Recommended: " + String(calibrationResultRecommendedKg_, 1) +
                     " kg. Confidence: " + calibrationResultConfidence_ +
                     ". Tap SAVE.");
  }
}

void SmartGymTouchApp::applySessionRecommendationFromSummary(const SessionHistoryRecord& record) {
  if (activeUser_ == nullptr || machineProfile_ == nullptr || !record.machineTypeId.equalsIgnoreCase(machineProfile_->machineTypeId)) {
    return;
  }
  UserMachineCalibration* calibration = userRegistry_.findCalibration(*activeUser_, machineProfile_->machineTypeId);
  if (calibration == nullptr) {
    calibration = userRegistry_.upsertCalibration(*activeUser_,
                                                  machineProfile_->machineTypeId,
                                                  record.selectedWeightKg,
                                                  max(0.0f, record.userRomTopPct - record.userRomBottomPct),
                                                  record.userRomBottomPct,
                                                  record.userRomTopPct);
  }
  if (calibration == nullptr) {
    return;
  }
  float nextKg = record.selectedWeightKg;
  String action = "keep";
  String reason = "Session quality held target.";
  if (record.validReps >= static_cast<uint16_t>(record.targetSets) * static_cast<uint16_t>(record.targetRepsMax) &&
      record.avgRomPercent >= 90.0f &&
      record.avgPeakVelocityPctPerSec >= 70.0f) {
    nextKg = roundWeightToMachineIncrement(max(record.selectedWeightKg + machineProfile_->machineIncrementKg,
                                               record.selectedWeightKg * 1.05f));
    action = "increase";
    reason = "Session quality and completion were strong.";
  } else if (record.avgRomPercent < 80.0f || record.avgPeakVelocityPctPerSec < 45.0f) {
    nextKg = roundWeightToMachineIncrement(min(record.selectedWeightKg - machineProfile_->machineIncrementKg,
                                               record.selectedWeightKg * 0.95f));
    action = "decrease";
    reason = "Session quality dropped below target.";
  }
  nextKg = roundWeightToMachineIncrement(nextKg);
  calibration->nextRecommendedWeightKg = nextKg;
  calibration->nextRecommendationSource = "session_summary";
  calibration->nextRecommendationReason = reason;
  calibration->nextRecommendationUpdatedAt = record.endedAtEpoch;
  calibration->suggestedWeightKg = nextKg;
  calibration->activeGoalRecommendedKg = nextKg;
  calibration->activeGoalId = record.goal;
  if (record.goal.equalsIgnoreCase("strength")) {
    calibration->strengthRecommendedKg = nextKg;
  } else if (record.goal.equalsIgnoreCase("endurance")) {
    calibration->enduranceRecommendedKg = nextKg;
  } else if (record.goal.equalsIgnoreCase("hypertrophy")) {
    calibration->hypertrophyRecommendedKg = nextKg;
  }
  calibration->resultAction = UserRegistry::calibrationActionFromString(action);
  calibration->updatedAtEpoch = record.endedAtEpoch;
  calibration->updatedAtIso = record.endedAtIso;
  logEvent("REC",
           String("session result action=") + action +
               " currentKg=" + String(record.selectedWeightKg, 1) +
               " nextKg=" + String(nextKg, 1) +
               " reason=" + reason,
           LogLevel::Normal);
  logEvent("REC",
           String("saved nextRecommendedKg=") + String(nextKg, 1) +
               " source=session_summary uid=" + activeUser_->rfidUid +
               " machine=" + machineProfile_->machineTypeId +
               " ts=" + String(calibration->nextRecommendationUpdatedAt),
           LogLevel::Normal);
  logEvent("WEIGHT",
           String("machine pin preserved kg=") + String(selectedWeightKg_, 1) +
               " reason=new_recommendation",
           LogLevel::Normal);
  refreshResolvedRecommendation(false, "session_summary");
  saveUsers();
  syncActiveCalibrationToCloud();
}

SmartGymTouchApp::ResolvedRecommendation SmartGymTouchApp::resolveRecommendedLoadForActiveUserMachine() {
  ResolvedRecommendation resolved;
  if (activeUser_ == nullptr || machineProfile_ == nullptr || activeCalibration_ == nullptr ||
      !activeCalibration_->hasCalibration) {
    const float defaultKg = machineProfile_ != nullptr ? machineProfile_->defaultSafeCalibrationKg : 0.0f;
    logEvent("REC",
             String("resolve uid=") + (activeUser_ != nullptr ? activeUser_->rfidUid : String("none")) +
                 " machine=" + (machineProfile_ != nullptr ? String(machineProfile_->machineTypeId) : String("none")) +
                 " chosenKg=" + String(defaultKg, 1) +
                 " source=machine_default sessionKg=0.0 sessionTs=0 calibrationKg=0.0 calibrationTs=0",
             LogLevel::Normal);
    return resolved;
  }

  const bool sessionSource = activeCalibration_->nextRecommendationSource.equalsIgnoreCase("session_summary");
  const float sessionKg = sessionSource ? activeCalibration_->nextRecommendedWeightKg : 0.0f;
  const uint32_t sessionTs = sessionSource ? activeCalibration_->nextRecommendationUpdatedAt : 0;

  float calibrationKg = 0.0f;
  if (activeCalibration_->resultRecommendedWeightKg > 0.0f) {
    calibrationKg = activeCalibration_->resultRecommendedWeightKg;
  } else if (activeCalibration_->suggestedWeightKg > 0.0f) {
    calibrationKg = activeCalibration_->suggestedWeightKg;
  } else if (!sessionSource && activeCalibration_->nextRecommendedWeightKg > 0.0f) {
    calibrationKg = activeCalibration_->nextRecommendedWeightKg;
  }
  const uint32_t calibrationTs = activeCalibration_->updatedAtEpoch;

  if (sessionKg > 0.0f && (calibrationKg <= 0.0f || sessionTs >= calibrationTs)) {
    resolved.hasRecommendation = true;
    resolved.kg = roundWeightToMachineIncrement(sessionKg);
    resolved.source = "session_summary";
    resolved.reason = activeCalibration_->nextRecommendationReason;
    resolved.updatedAtEpoch = sessionTs;
  } else if (calibrationKg > 0.0f) {
    resolved.hasRecommendation = true;
    resolved.kg = roundWeightToMachineIncrement(calibrationKg);
    resolved.source = "calibration";
    resolved.reason = activeCalibration_->resultReasonText;
    resolved.updatedAtEpoch = calibrationTs;
    if (sessionKg > 0.0f) {
      logEvent("REC",
               "next recommendation differs reason=calibration_newer",
               LogLevel::Normal);
    }
  }

  logEvent("REC",
           String("resolve uid=") + activeUser_->rfidUid +
               " machine=" + machineProfile_->machineTypeId +
               " chosenKg=" + String(resolved.hasRecommendation ? resolved.kg : 0.0f, 1) +
               " source=" + (resolved.hasRecommendation ? resolved.source : String("none")) +
               " sessionKg=" + String(sessionKg, 1) +
               " sessionTs=" + String(sessionTs) +
               " calibrationKg=" + String(calibrationKg, 1) +
               " calibrationTs=" + String(calibrationTs),
           LogLevel::Normal);
  return resolved;
}

void SmartGymTouchApp::refreshResolvedRecommendation(bool applyToCurrentLoad, const char* source) {
  (void)applyToCurrentLoad;
  (void)source;
  resolvedRecommendation_ = resolveRecommendedLoadForActiveUserMachine();

  if (!resolvedRecommendation_.hasRecommendation) {
    if (machineProfile_ != nullptr && selectedWeightKg_ <= 0.0f) {
      selectedWeightKg_ = roundWeightToMachineIncrement(machineProfile_->defaultCalibrationWeightKg);
      logEvent("WEIGHT",
               String("machine pin init kg=") + String(selectedWeightKg_, 1) +
                   " source=machine_default",
               LogLevel::Normal);
    }
    return;
  }

  logEvent("WEIGHT",
           String("recommendation loaded kg=") + String(resolvedRecommendation_.kg, 1) +
               " source=" + resolvedRecommendation_.source +
               " appliedToPin=0 reason=public_machine_pin",
           LogLevel::Normal);
  logEvent("WEIGHT",
           String("machine pin preserved kg=") + String(selectedWeightKg_, 1) +
               " reason=recommendation_loaded",
           LogLevel::Normal);
}

void SmartGymTouchApp::updateSelectedWeightFromRecommendation() {
  refreshResolvedRecommendation(true, "calibration_refresh");
}

void SmartGymTouchApp::startTraining(bool forceWithoutCalibration) {
  const uint32_t nowMs = millis();
  const bool allowTestGoal = isDebugGoalSelectionEnabled();
  const TrainingGoal runtimeGoal = normalizeGoalId(
      activeUser_ != nullptr ? activeUser_->goal : TrainingGoal::Hypertrophy,
      "profile",
      allowTestGoal);
  if (state_ == State::Idle && nowMs < trainingStartBlockedUntilMs_) {
    logEvent("SESSION", "start blocked reason=profile_transition", LogLevel::Normal);
    setStatusMessage("Start ignored: finish profile edit first.");
    refreshUi();
    return;
  }
  if (machineProfile_ == nullptr) {
    logEvent("SESSION", "start blocked reason=missing_machine_profile", LogLevel::Normal);
    setStatusMessage("Machine profile missing.");
    refreshUi();
    return;
  }
  if (activeUser_ == nullptr && !anonymousMode_) {
    logEvent("SESSION", "start blocked reason=no_user", LogLevel::Normal);
    setStatusMessage("Load a user or use anonymous mode.");
    refreshUi();
    return;
  }
  if (!forceWithoutCalibration && !anonymousMode_ &&
      (activeCalibration_ == nullptr || !activeCalibration_->hasCalibration)) {
    logEvent("SESSION", "start blocked reason=calibration_required", LogLevel::Normal);
    pendingWorkoutAfterCalibration_ = false;
    setStatusMessage("No prior calibration found for this machine.");
    showUiScreen(UiScreenMode::CalibrationGate);
    refreshUi();
    return;
  }

  const TrainingGoal goal = runtimeGoal;
  const GoalRecommendation* recommendation =
      machineProfile_ != nullptr ? machineRegistry_.findGoalRecommendation(*machineProfile_, goal) : nullptr;
  const String userUid = activeUser_ != nullptr ? activeUser_->rfidUid : "anonymous";
  const String userName = activeUser_ != nullptr ? activeUser_->displayName : "Anonymous";

  String trainingStartMessage = "Training session active.";

  // Smart load recommendation with confidence:
  // use recent 2-3 sessions, never increase when form quality is poor.
  if (activeUser_ != nullptr && machineProfile_ != nullptr) {
    const uint32_t nowEpoch = cloudEnabled_ ? firebaseService_.getCurrentEpoch() : timeService_.getEpoch();
    WeeklyTrainingSignal weeklySignal;
    localPersistenceStore_.buildWeeklyTrainingSignal(activeUser_->rfidUid,
                                                     machineProfile_->machineId,
                                                     nowEpoch,
                                                     weeklySignal);
    if (cloudEnabled_ && activeUser_ != nullptr && nowEpoch > 0 &&
        !shouldSuppressOptionalCloudRead("history", millis()) &&
        !shouldSkipRecentCloudRead("history", activeUser_->rfidUid, millis())) {
      if (lastCloudHistoryTimeoutUid_ == activeUser_->rfidUid &&
          lastCloudHistoryTimeoutMs_ != 0 &&
          (millis() - lastCloudHistoryTimeoutMs_) < kCloudReadPreviousTimeoutWindowMs) {
        logEvent("CloudRead",
                 String("skipped reason=previous_timeout kind=history uid=") + activeUser_->rfidUid,
                 LogLevel::Normal);
      } else {
      for (uint8_t weekOffset = 0; weekOffset < 4; ++weekOffset) {
        const uint32_t weekEpoch = nowEpoch > (weekOffset * 7UL * 24UL * 3600UL)
                                       ? nowEpoch - (weekOffset * 7UL * 24UL * 3600UL)
                                       : nowEpoch;
        time_t raw = static_cast<time_t>(weekEpoch);
        struct tm timeInfo {};
        gmtime_r(&raw, &timeInfo);
        char weekBuffer[16] = {0};
        if (strftime(weekBuffer, sizeof(weekBuffer), "%G-W%V", &timeInfo) <= 0) {
          continue;
        }

        const String weekKey = String(weekBuffer);
        String cloudWeekSummaryJson;
        const String cloudWeekPath =
            "athleteWeeklySessions/" + activeUser_->rfidUid + "/" + weekKey + "/weekSummary";
        const uint32_t readStartedMs = millis();
        const bool fetched = firebaseService_.fetchPathJson(cloudWeekPath, cloudWeekSummaryJson);
        const uint32_t readDurationMs = millis() - readStartedMs;
        const String readError = firebaseService_.getLastErrorSummary();
        const int readHttp = firebaseService_.getLastHttpStatusCode();
        if (!fetched || cloudWeekSummaryJson.isEmpty() || cloudWeekSummaryJson == "null") {
          const String errorLower = readError;
          const bool timeoutError = (readHttp == -3) ||
                                    (errorLower.indexOf("timed out") >= 0) ||
                                    (errorLower.indexOf("timeout") >= 0);
          const bool tlsBackoffError = (readHttp == 0) ||
                                       (errorLower.indexOf("Cloud TLS backoff") >= 0);
          if (timeoutError) {
            logEvent("CloudRead",
                     String("optional timeout kind=path_json durationMs=") + String(readDurationMs) +
                         " week=" + weekKey,
                     LogLevel::Normal);
            logEvent("CloudRead",
                     String("history loop stopped reason=timeout week=") + weekKey,
                     LogLevel::Normal);
            lastCloudHistoryTimeoutUid_ = activeUser_->rfidUid;
            lastCloudHistoryTimeoutMs_ = millis();
            break;
          }
          if (tlsBackoffError) {
            logEvent("CloudRead",
                     String("history loop stopped reason=tls_backoff week=") + weekKey,
                     LogLevel::Normal);
            break;
          }
          continue;
        }
        lastCloudHistoryTimeoutUid_ = "";
        lastCloudHistoryTimeoutMs_ = 0;

        uint32_t cloudSessions = 0;
        float cloudAvgSetCompletionRatio = 0.0f;
        float cloudAvgRepCompletionRatio = 0.0f;
        float cloudAvgPeakVelocity = 0.0f;
        float cloudAvgQuality = 0.0f;
        float cloudPoorFormRatio = 0.0f;
        float cloudLastSelectedWeightKg = 0.0f;

        FirebaseService::jsonUIntValue(cloudWeekSummaryJson, "sessions", cloudSessions);
        FirebaseService::jsonFloatValue(cloudWeekSummaryJson, "avgSetCompletionRatio", cloudAvgSetCompletionRatio);
        FirebaseService::jsonFloatValue(cloudWeekSummaryJson, "avgRepCompletionRatio", cloudAvgRepCompletionRatio);
        FirebaseService::jsonFloatValue(cloudWeekSummaryJson, "avgPeakVelocityPctPerSec", cloudAvgPeakVelocity);
        FirebaseService::jsonFloatValue(cloudWeekSummaryJson, "avgRepQualityScore", cloudAvgQuality);
        FirebaseService::jsonFloatValue(cloudWeekSummaryJson, "poorFormSessionRatio", cloudPoorFormRatio);
        FirebaseService::jsonFloatValue(cloudWeekSummaryJson, "lastSelectedWeightKg", cloudLastSelectedWeightKg);

        if (cloudSessions == 0) {
          continue;
        }

        if (!weeklySignal.hasData || weeklySignal.sessions == 0) {
          weeklySignal.hasData = true;
          weeklySignal.sessions = static_cast<uint8_t>(min<uint32_t>(255, cloudSessions));
          weeklySignal.recentSessionsUsed = min<uint8_t>(3, static_cast<uint8_t>(cloudSessions));
          weeklySignal.avgSetCompletionRatio = cloudAvgSetCompletionRatio;
          weeklySignal.avgRepCompletionRatio = cloudAvgRepCompletionRatio;
          weeklySignal.avgPeakVelocityPctPerSec = cloudAvgPeakVelocity;
          weeklySignal.avgRepQualityScore = cloudAvgQuality;
          weeklySignal.poorFormSessionRatio = cloudPoorFormRatio;
          weeklySignal.lastSelectedWeightKg = cloudLastSelectedWeightKg;
        } else {
          // Blend local + cloud for stronger cross-device continuity.
          const float localW = static_cast<float>(max<uint8_t>(1, weeklySignal.sessions));
          const float cloudW = static_cast<float>(cloudSessions);
          const float wSum = localW + cloudW;
          weeklySignal.avgSetCompletionRatio =
              ((weeklySignal.avgSetCompletionRatio * localW) + (cloudAvgSetCompletionRatio * cloudW)) / wSum;
          weeklySignal.avgRepCompletionRatio =
              ((weeklySignal.avgRepCompletionRatio * localW) + (cloudAvgRepCompletionRatio * cloudW)) / wSum;
          weeklySignal.avgPeakVelocityPctPerSec =
              ((weeklySignal.avgPeakVelocityPctPerSec * localW) + (cloudAvgPeakVelocity * cloudW)) / wSum;
          weeklySignal.avgRepQualityScore =
              ((weeklySignal.avgRepQualityScore * localW) + (cloudAvgQuality * cloudW)) / wSum;
          weeklySignal.poorFormSessionRatio =
              ((weeklySignal.poorFormSessionRatio * localW) + (cloudPoorFormRatio * cloudW)) / wSum;
          if (cloudLastSelectedWeightKg > 0.0f) {
            weeklySignal.lastSelectedWeightKg = cloudLastSelectedWeightKg;
          }
          weeklySignal.sessions = max<uint8_t>(weeklySignal.sessions,
                                               static_cast<uint8_t>(min<uint32_t>(255, cloudSessions)));
          weeklySignal.recentSessionsUsed = max<uint8_t>(weeklySignal.recentSessionsUsed,
                                                         min<uint8_t>(3, static_cast<uint8_t>(cloudSessions)));
        }
        break;
      }
      }
    }

    if (weeklySignal.hasData && weeklySignal.sessions > 0) {
      float baseWeight = weeklySignal.lastSelectedWeightKg > 0.0f
                             ? weeklySignal.lastSelectedWeightKg
                             : selectedWeightKg_;
      if (baseWeight <= 0.0f) {
        baseWeight = selectedWeightKg_;
      }

      float deltaKg = 0.0f;
      const bool enoughConfidence = weeklySignal.recentSessionsUsed >= 2;
      const bool poorFormGate = weeklySignal.avgRepQualityScore < 70.0f ||
                                weeklySignal.poorFormSessionRatio >= 0.34f;
      const bool strongTrend = weeklySignal.avgSetCompletionRatio >= 0.98f &&
                               weeklySignal.avgRepCompletionRatio >= 0.95f &&
                               weeklySignal.avgPeakVelocityPctPerSec >= 90.0f &&
                               weeklySignal.avgRepQualityScore >= 82.0f;
      const bool weakTrend = weeklySignal.avgSetCompletionRatio < 0.75f ||
                             weeklySignal.avgRepCompletionRatio < 0.70f ||
                             weeklySignal.avgPeakVelocityPctPerSec < 45.0f ||
                             weeklySignal.avgRepQualityScore < 62.0f;

      if (enoughConfidence && strongTrend && !poorFormGate) {
        deltaKg = 2.5f;
      } else if (weakTrend) {
        deltaKg = -2.5f;
      }
      const float recommended = constrain(baseWeight + deltaKg, kMinWeightKg, kMaxWeightKg);
      if (deltaKg > 0.0f && !poorFormGate) {
        trainingStartMessage = "Training active. Trend: +2.5kg (quality confirmed).";
      } else if (deltaKg > 0.0f && poorFormGate) {
        trainingStartMessage = "Training active. Keep load: improve form quality first.";
      } else if (deltaKg < 0.0f) {
        trainingStartMessage = "Training active. Trend: -2.5kg for control.";
      } else {
        trainingStartMessage = enoughConfidence
                                   ? "Training active. Trend: keep load."
                                   : "Training active. Keep load until more sessions.";
      }
    }
  }

  lowMemoryUploadUiFreed_ = false;
  freeOptionalUiForMemory("session_start", true);

  if (activeUser_ != nullptr && machineProfile_ != nullptr) {
    refreshResolvedRecommendation(false, "session_start");
  }
  const float recommendedLoadKg = resolvedRecommendation_.hasRecommendation ? resolvedRecommendation_.kg : 0.0f;

  const char* startLoadSource = "machine_pin";
  float startLoadKg = selectedWeightKg_;
  if (startLoadKg <= 0.0f) {
    startLoadKg = roundWeightToMachineIncrement(
        machineProfile_ != nullptr ? machineProfile_->defaultCalibrationWeightKg : kMinWeightKg);
    selectedWeightKg_ = startLoadKg;
    logEvent("WEIGHT",
             String("machine pin init kg=") + String(selectedWeightKg_, 1) +
                 " source=machine_default",
             LogLevel::Normal);
  }

  logEvent("WEIGHT",
           String("start load source=") + startLoadSource +
               " kg=" + String(startLoadKg, 1) +
               " recommendedKg=" + String(recommendedLoadKg, 1),
           LogLevel::Normal);

  logEvent("SESSION",
           String("start requested user=") +
               (activeUser_ != nullptr ? activeUser_->rfidUid : String(anonymousMode_ ? "anonymous" : "none")) +
               " machine=" + (machineProfile_ != nullptr ? String(machineProfile_->machineTypeId) : String("none")) +
               " goal=" + String(UserRegistry::goalToString(runtimeGoal)) +
               " selectedKg=" + String(startLoadKg, 1),
           LogLevel::Normal);

  sessionRepCount_ = 0;
  currentSetRepCount_ = 0;
  completedSets_ = 0;
  setStartedAtMs_ = 0;
  lastAcceptedRepAtMs_ = 0;
  setPauseCandidateMs_ = 0;
  repDetector_.reset();
  sessionStartMs_ = millis();
  idealGraphStartMs_ = sessionStartMs_;
  idealPhaseMs_ = 0;
  idealPhaseLastTickMs_ = 0;
  resetMotionGraph();
  lastSetCompletedMs_ = 0;
  hasLastCompletedRep_ = false;
  sessionBestRepPeakVelocityPctPerSec_ = 0.0f;
  poorRepStreak_ = 0;
  lastRepSummary_ = "Awaiting first rep.";
  sessionSummaryStartedMs_ = 0;
  sessionSummaryMinVisibleUntilMs_ = 0;
  sessionLogoutPending_ = false;
  summaryWaitingForSync_ = false;
  summarySyncWaitStartedMs_ = 0;
  lastSummarySyncStatus_ = "";
  if (!pendingSessionQueueEnqueue_ && !pendingSessionDetailQueueEnqueue_ && !pendingWebDetailUpload_) {
    pendingSessionQueueRecord_ = SessionHistoryRecord{};
    pendingSessionDetailQueueRecord_ = SessionHistoryRecord{};
    pendingSessionDetailWaitLogged_ = false;
    resetPendingWebDetailUpload();
  }
  autoFinishRequestPending_ = false;
  autoFinishReason_ = "";
  if (!pendingSessionQueueEnqueue_ && localPersistenceStore_.getPendingUploadCount() == 0) {
    forceCloudSyncAfterFinish_ = false;
  }
  lastSessionUserName_ = activeUser_ != nullptr
                             ? (activeUser_->displayName.isEmpty() ? String("Card User")
                                                                   : activeUser_->displayName)
                             : (anonymousMode_ ? String("Anonymous") : String("None"));
  lastSessionUserUid_ = activeUser_ != nullptr ? activeUser_->rfidUid : String("anonymous");
  lastSessionMachineName_ = machineProfile_ != nullptr ? machineProfile_->displayName : String("Unknown machine");
  activeSessionTargetSets_ = recommendation != nullptr ? recommendation->targetSets : 3;
  activeSessionTargetRepsMin_ = recommendation != nullptr ? recommendation->repsMin : 0;
  activeSessionTargetRepsMax_ = recommendation != nullptr ? recommendation->repsMax : 10;
  activeSessionRestSeconds_ = recommendation != nullptr ? recommendation->restSeconds : 45;
  {
    MotionTargetConfig motionTargets;
    if (deriveMotionTargetsForActiveMachine(motionTargets, goal, allowTestGoal, "session_start")) {
      activeSessionTargetRepsMin_ = motionTargets.targetRepsMin;
      activeSessionTargetRepsMax_ = motionTargets.targetRepsMax;
      activeSessionTargetSets_ = motionTargets.targetSetsDefault;
      activeSessionRestSeconds_ = motionTargets.restSecondsDefault;
      sessionTplRiseMs_ = static_cast<uint16_t>(motionTargets.riseTimeSecDefault * 1000.0f);
      sessionTplTopPauseMs_ = static_cast<uint16_t>(motionTargets.topPauseSec * 1000.0f);
      sessionTplLowerMs_ = static_cast<uint16_t>(motionTargets.lowerTimeSecDefault * 1000.0f);
      sessionTplBottomPauseMs_ = static_cast<uint16_t>(motionTargets.bottomPauseSec * 1000.0f);
      logEvent("CFG",
               String("timing target machineTypeId=") + machineProfile_->machineTypeId +
                   " goal=" + motionTargets.goalId +
                   " rise=" + String(motionTargets.riseTimeSecDefault, 2) +
                   " lower=" + String(motionTargets.lowerTimeSecDefault, 2) +
                   " topPause=" + String(motionTargets.topPauseSec, 2) +
                   " bottomPause=" + String(motionTargets.bottomPauseSec, 2),
               LogLevel::Normal);
      logEvent("GRAPH",
               String("target curve machineTypeId=") + machineProfile_->machineTypeId +
                   " goal=" + motionTargets.goalId +
                   " rise=" + String(motionTargets.riseTimeSecDefault, 2) +
                   " lower=" + String(motionTargets.lowerTimeSecDefault, 2) +
                   " topPause=" + String(motionTargets.topPauseSec, 2) +
                   " bottomPause=" + String(motionTargets.bottomPauseSec, 2),
               LogLevel::Normal);
    } else {
      const MotionGuideTemplate sessionTpl = buildMotionGuideTemplate(machineProfile_, recommendation);
      sessionTplRiseMs_ = sessionTpl.riseMs;
      sessionTplTopPauseMs_ = sessionTpl.topPauseMs;
      sessionTplLowerMs_ = sessionTpl.lowerMs;
      sessionTplBottomPauseMs_ = sessionTpl.bottomPauseMs;
    }
    sessionMotionTemplateLatched_ = true;
  }
  activeSessionWeightKg_ = startLoadKg;
  logEvent("WEIGHT",
           String("session start frozenKg=") + String(activeSessionWeightKg_, 1),
           LogLevel::Normal);

  sessionRecorder_.start(buildSessionId(),
                         userUid,
                         userName,
                         machineProfile_,
                         goal,
                         anonymousMode_,
                         activeSessionWeightKg_,
                         activeCalibration_ != nullptr ? activeCalibration_->suggestedWeightKg : 0.0f,
                         activeCalibration_ != nullptr ? activeCalibration_->userRomPercent : 0.0f,
                         activeCalibration_ != nullptr ? activeCalibration_->userBottomPct : 0.0f,
                         activeCalibration_ != nullptr ? activeCalibration_->userTopPct : 0.0f,
                         recommendation,
                         millis(),
                         cloudEnabled_ ? firebaseService_.getCurrentEpoch() : timeService_.getEpoch(),
                         buildTimeText());
  logEvent("SESSION",
           String("start ok id=") + sessionRecorder_.getRecord().sessionId +
               " selectedKg=" + String(activeSessionWeightKg_, 1) +
               " targetSets=" + String(activeSessionTargetSets_) +
               " targetRepsMin=" + String(activeSessionTargetRepsMin_) +
               " targetRepsMax=" + String(activeSessionTargetRepsMax_) +
               " restSec=" + String(activeSessionRestSeconds_),
           LogLevel::Normal);

  state_ = State::Training;
  pendingWorkoutAfterCalibration_ = false;
  setStatusMessage(trainingStartMessage);
  showUiScreen(UiScreenMode::Main);
  refreshUi();
}

void SmartGymTouchApp::logoutActiveUser(const String& reason, bool goIdleScreen) {
  const uint32_t nowMs = millis();
  const bool preservePendingUpload =
      pendingSessionQueueEnqueue_ || pendingSessionDetailQueueEnqueue_ ||
      pendingWebDetailUpload_ || pendingWebDetailPhase_ != 0 ||
      !pendingSessionQueueRecord_.sessionId.isEmpty();

  autoMotionEnabled_ = false;
  autoMotionPhase_ = AutoMotionPhase::Idle;
  autoRepContinuous_ = false;
  autoRepStopAfterCycle_ = false;
  pendingWorkoutAfterCalibration_ = false;

  sessionStartMs_ = 0;
  lastSetCompletedMs_ = 0;
  sessionSummaryStartedMs_ = 0;
  sessionSummaryMinVisibleUntilMs_ = 0;
  sessionLogoutPending_ = false;
  summaryWaitingForSync_ = false;
  summarySyncWaitStartedMs_ = 0;
  lastSummarySyncStatus_ = "";
  currentSetRepCount_ = 0;
  completedSets_ = 0;
  sessionRepCount_ = 0;
  sessionMotionTemplateLatched_ = false;
  poorRepStreak_ = 0;
  setStartedAtMs_ = 0;
  lastAcceptedRepAtMs_ = 0;
  setPauseCandidateMs_ = 0;
  activeSessionTargetSets_ = 0;
  activeSessionTargetRepsMin_ = 0;
  activeSessionTargetRepsMax_ = 0;
  activeSessionRestSeconds_ = 0;
  activeSessionWeightKg_ = 0.0f;
  autoFinishRequestPending_ = false;
  autoFinishReason_ = "";
  logEvent("WEIGHT",
           String("machine pin preserved kg=") + String(selectedWeightKg_, 1) +
               " reason=logout",
           LogLevel::Normal);
  if (preservePendingUpload) {
    logEvent("SYNC",
             String("logout preserving pending session upload id=") + pendingSessionQueueRecord_.sessionId,
             LogLevel::Normal);
    forceCloudSyncAfterFinish_ = true;
  } else {
    forceCloudSyncAfterFinish_ = false;
  }

  activeCalibration_ = nullptr;
  activeUser_ = nullptr;
  resolvedRecommendation_ = ResolvedRecommendation{};
  anonymousMode_ = false;
  lastScannedUid_ = "None";
  idleWakeCandidateMs_ = 0;
  idleShowCandidateMs_ = 0;
  idleHideCandidateMs_ = 0;
  pendingScanReconcile_ = false;
  pendingScanReconcileEarliestMs_ = 0;
  pendingScanReconcileStartedMs_ = 0;
  pendingScanReconcileStep_ = 0;
  pendingSwitchUid_ = "";
  if (userSwitchPrompt_ != nullptr) {
    lv_obj_add_flag(userSwitchPrompt_, LV_OBJ_FLAG_HIDDEN);
  }
  idleOverlaySuppressedUntilMs_ = nowMs;
  markUserActivity(nowMs);
  cloudBlockedUntilMs_ = max<uint32_t>(cloudBlockedUntilMs_, nowMs + kCloudAfterLogoutQuietMs);
  setStatusMessage(reason);
  if (goIdleScreen) {
    state_ = State::Idle;
    showUiScreen(UiScreenMode::Idle);
  } else {
    state_ = State::Summary;
    showUiScreen(UiScreenMode::Summary);
  }
  refreshUi();
}

void SmartGymTouchApp::maybeAutoLogoutAfterSession(uint32_t nowMs) {
  if (state_ != State::Summary || !sessionLogoutPending_ || sessionSummaryStartedMs_ == 0) {
    return;
  }
  if (sessionSummaryMinVisibleUntilMs_ != 0 && nowMs < sessionSummaryMinVisibleUntilMs_) {
    return;
  }
  const uint32_t elapsedMs = nowMs >= sessionSummaryStartedMs_ ? (nowMs - sessionSummaryStartedMs_) : 0;
  if (elapsedMs < kSessionAutoLogoutMs) {
    return;
  }
  const char* busyReason = nullptr;
  uint16_t busyQueueCount = 0;
  const bool uploadBusy = cloudEnabled_ && isSessionUploadBusy(nowMs, &busyReason, &busyQueueCount);
  if (uploadBusy) {
    logEvent("SUMMARY",
             String("leaving while upload_busy=1 reason=max_visible_elapsed queue=") +
                 String(busyQueueCount) +
                 " detail=" + (busyReason != nullptr ? String(busyReason) : String("unknown")),
             LogLevel::Normal);
    logEvent("SUMMARY",
             String("background saving mode queue=") + String(busyQueueCount) +
                 " heap=" + String(internalFree8BitHeap()) +
                 " largestBlock=" + String(internalLargestFree8BitBlock()),
             LogLevel::Normal);
    logoutActiveUser("Saving in background.");
    return;
  }
  logEvent("SUMMARY", "leaving reason=auto_logout uploadBusy=0", LogLevel::Normal);
  logoutActiveUser("Session ended.");
}

void SmartGymTouchApp::finishTraining(const String& reason, bool logoutNow) {
  logEvent("SESSION",
           String("finish enter reason=") + reason +
               " logoutNow=" + (logoutNow ? "1" : "0") +
               " recorderActive=" + (sessionRecorder_.isActive() ? "1" : "0") +
               " state=" + stateToText() +
               " screen=" + uiScreenToText(currentUiScreen_),
           LogLevel::Normal);
  if (finishTransitionInProgress_) {
    logEvent("SESSION", "finish ignored reason=transition_in_progress", LogLevel::Normal);
    return;
  }
  finishTransitionInProgress_ = true;
  if (!sessionRecorder_.isActive()) {
    logEvent("SESSION", "finish ignored reason=no_active_session", LogLevel::Normal);
    setStatusMessage("No active session.");
    refreshUi();
    finishTransitionInProgress_ = false;
    return;
  }

  sessionRecorder_.finish(millis(),
                          cloudEnabled_ ? firebaseService_.getCurrentEpoch() : timeService_.getEpoch(),
                          buildTimeText());
  const SessionHistoryRecord& record = sessionRecorder_.getRecord();
  // Do not write the local session-history cache inside the LVGL task.
  // Preferences/NVS flash access here overflowed the LVGL stack at the
  // exact last-rep -> summary transition. Firebase upload is queued below.
  if (!lastFinishedSessionId_.isEmpty() && record.sessionId == lastFinishedSessionId_) {
    logEvent("SESSION",
             String("finish ignored reason=duplicate_session id=") + record.sessionId,
             LogLevel::Normal);
    finishTransitionInProgress_ = false;
    return;
  }
  lastFinishedSessionId_ = record.sessionId;
  logEvent("SESSION",
           String("finish record id=") + record.sessionId +
               " reps=" + String(record.repCount) +
               " sets=" + String(record.setCount) +
               " validReps=" + String(record.validReps) +
               " selectedKg=" + String(record.selectedWeightKg, 1) +
               " cloudEnabled=" + (cloudEnabled_ ? "1" : "0"),
           LogLevel::Normal);

  const String qualityTier = record.sessionQualityTier.isEmpty() ? String("n/a")
                                                                  : record.sessionQualityTier;
  const uint32_t totalRepAttempts =
      static_cast<uint32_t>(record.validReps) + static_cast<uint32_t>(record.invalidReps);
  const float validRatePct = totalRepAttempts > 0
                                 ? (static_cast<float>(record.validReps) * 100.0f) / static_cast<float>(totalRepAttempts)
                                 : 0.0f;
  const float volumeKg = record.selectedWeightKg * static_cast<float>(record.validReps);
  const uint32_t durationSec = record.durationMs / 1000UL;
  const uint32_t durationMin = durationSec / 60UL;
  const uint32_t durationRemSec = durationSec % 60UL;

  String head = "Quality " + qualityTier + " (" + String(record.sessionQualityScore, 0) + ")";
  String block1 = "Sets " + String(record.setsCompleted) + "/" + String(record.targetSets) +
                  " | Reps " + String(record.validReps) + "/" + String(totalRepAttempts) +
                  " (" + String(validRatePct, 0) + "%)";
  String block2 = "ROM " + String(record.avgRomPercent, 1) + "% | Timing quality tracked";
  String block3 = "Volume " + String(volumeKg, 1) + " kg | Time " + String(durationMin) + "m " +
                  (durationRemSec < 10 ? String("0") : String("")) + String(durationRemSec) + "s";
  lastSessionSummary_ = head + "\n" + block1 + "\n" + block2 + "\n" + block3;
  lastSessionUserName_ = record.userDisplayName.isEmpty() ? String("Card User") : record.userDisplayName;
  lastSessionUserUid_ = record.userUid.isEmpty() ? String("anonymous") : record.userUid;
  lastSessionMachineName_ = record.machineDisplayName.isEmpty() && machineProfile_ != nullptr
                                ? machineProfile_->displayName
                                : record.machineDisplayName;
  applySessionRecommendationFromSummary(record);
  lastSummaryRecord_ = record;
  sanitizeSessionForUpload(lastSummaryRecord_);

  summaryWaitingForSync_ = false;
  if (cloudEnabled_) {
    // Never do network/TLS work or heavy queue-serialization inside the
    // immediate finish transition. Enqueue on the next cloud service tick.
    pendingCanonicalSessionSync_ = false;
    forceCloudSyncAfterFinish_ = true;
    pendingSessionQueueRecord_ = record;
    sanitizeSessionForUpload(pendingSessionQueueRecord_);
    pendingSessionQueueEnqueue_ = true;
    pendingSessionDetailQueueRecord_ = SessionHistoryRecord{};
    pendingSessionDetailQueueEnqueue_ = false;
    pendingSessionDetailWaitLogged_ = false;
    resetPendingWebDetailUpload();
    summaryWaitingForSync_ = false;
    summarySyncWaitStartedMs_ = 0;
    lastSummarySyncStatus_ = "";
    lastUploadRetryMs_ = 0;
    syncWorkerForce_ = true;
    uiCache_.lastCloudServiceMs = 0;
    logEvent("SYNC",
             String("finish queue scheduled id=") + record.sessionId +
                 " repCount=" + String(record.repCount) +
                 " setCount=" + String(record.setCount) +
                 " pendingSessionQueueEnqueue=" + (pendingSessionQueueEnqueue_ ? "1" : "0") +
                 " forceCloudSyncAfterFinish=" + (forceCloudSyncAfterFinish_ ? "1" : "0"),
             LogLevel::Normal);
  }

  freeOptionalUiForMemory("session_finish", true);
  setStatusMessage("Session finished. " + reason);
  if (logoutNow) {
    logoutActiveUser(statusMessage_, true);
    finishTransitionInProgress_ = false;
    return;
  }

  sessionStartMs_ = 0;
  state_ = State::Summary;
  sessionSummaryStartedMs_ = millis();
  sessionSummaryMinVisibleUntilMs_ = sessionSummaryStartedMs_ + kSummaryMinVisibleMs;
  sessionLogoutPending_ = true;
  lastSummarySyncStatus_ = "";
  logEvent("SUMMARY",
           String("visible start autoLogoutMs=") + String(kSessionAutoLogoutMs),
           LogLevel::Normal);
  logEvent("SESSION",
           String("summary shown id=") + record.sessionId,
           LogLevel::Normal);
  showUiScreen(UiScreenMode::Summary);
  refreshUi();
  finishTransitionInProgress_ = false;
}

void SmartGymTouchApp::sanitizeSessionForUpload(SessionHistoryRecord& out) const {
  uint32_t nowEpoch = cloudEnabled_ ? firebaseService_.getCurrentEpoch() : timeService_.getEpoch();
  if (nowEpoch == 0) {
    nowEpoch = out.endedAtEpoch != 0 ? out.endedAtEpoch : out.startedAtEpoch;
  }
  if (nowEpoch == 0) {
    nowEpoch = 1;
  }

  if (out.endedAtEpoch == 0) {
    out.endedAtEpoch = nowEpoch;
    out.endedAtIso = buildTimeText();
  }

  if (out.startedAtEpoch == 0) {
    const uint32_t durSec = out.durationMs / 1000UL;
    out.startedAtEpoch = out.endedAtEpoch > durSec ? (out.endedAtEpoch - durSec) : out.endedAtEpoch;
    if (out.startedAtIso.isEmpty() || out.startedAtIso == "LOCAL TIME|--:--") {
      out.startedAtIso = out.endedAtIso;
    }
  }

  if (out.sessionId.isEmpty() || out.sessionId == "session_0") {
    out.sessionId = buildSessionId();
  }
}

void SmartGymTouchApp::activateUserByUid(const String& uid, bool createIfMissing, bool reconcileCloud) {
  if (uid.isEmpty()) {
    return;
  }
  if (userLoading_) {
    logEvent("RFID",
             String("scan ignored reason=user_sync_active uid=") + uid,
             LogLevel::Normal);
    logEvent("UI", "input blocked reason=user_loading", LogLevel::Normal);
    return;
  }
  if (state_ == State::Calibration) {
    logEvent("CAL",
             String("interaction blocked reason=calibration_active kind=rfid uid=") + uid,
             LogLevel::Normal);
    setStatusMessage("Calibration active. Scan after calibration is complete.");
    refreshUi();
    return;
  }
  const uint32_t nowMs = millis();
  const char* busyReason = nullptr;
  uint16_t busyQueueCount = 0;
  if (isSessionUploadBusy(nowMs, &busyReason, &busyQueueCount)) {
    logEvent("SYNC",
             String("upload busy reason=") + (busyReason != nullptr ? busyReason : "unknown") +
                 " queueCount=" + String(busyQueueCount) +
                 " phase=" + String(static_cast<unsigned>(pendingWebDetailPhase_)),
             LogLevel::Normal);
    logEvent("RFID",
             String("scan ignored reason=upload_active uid=") + uid,
             LogLevel::Normal);
    setStatusMessage("Syncing session. Scan again in a moment.");
    refreshUi();
    return;
  }
  logEvent("RFID",
           String("scan uid=") + uid +
               " reconcile=" + (reconcileCloud ? "1" : "0"),
           LogLevel::Normal);
  userLoading_ = true;
  showUserLoadingPopup(uid, "profile");
  logEvent("USER_SYNC", String("begin uid=") + uid, LogLevel::Normal);
  logEvent("USER_SYNC", "stage=profile", LogLevel::Normal);
  setStatusMessage("Loading user...");
  refreshUi();
  auto hideUserLoading = [&](const char* result) {
    if (userLoading_) {
      userLoading_ = false;
      hideUserLoadingPopup(uid, result != nullptr ? result : "ok");
      logEvent("USER_SYNC",
               String("complete uid=") + uid +
                   " result=" + (result != nullptr ? String(result) : String("ok")),
               LogLevel::Normal);
    }
  };

  anonymousMode_ = false;
  lastScannedUid_ = uid;
  activeUser_ = userRegistry_.findByUid(uid);

  bool cloudProfileFound = false;
  bool cloudVerifiedMissing = false;
  bool cloudLookupAttempted = false;
  bool cloudLookupFailed = false;
  if (reconcileCloud && cloudEnabled_) {
    if (firebaseService_.isWifiConnected()) {
      cloudLookupAttempted = true;
      UserProfile cloudProfile;
      bool profileFetched = false;
      bool profileSkippedRecent = false;
      if (activeUser_ != nullptr && shouldSkipRecentCloudRead("profile", uid, millis())) {
        profileSkippedRecent = true;
      } else {
        profileFetched = firebaseService_.fetchUserProfile(uid, cloudProfile);
      }
      if (profileSkippedRecent) {
        cloudProfileFound = activeUser_ != nullptr;
      } else if (profileFetched && !cloudProfile.rfidUid.isEmpty()) {
        cloudProfileFound = true;
        logEvent("RFID", String("cloud profile found uid=") + uid, LogLevel::Normal);
        userRegistry_.upsertProfile(cloudProfile);
        activeUser_ = userRegistry_.findByUid(uid);
        if (activeUser_ != nullptr) {
          mergeActiveUserFromCloud(cloudProfile);
          saveUsers();
        }
      } else if (firebaseService_.getLastHttpStatusCode() == 200) {
        cloudVerifiedMissing = true;
        logEvent("RFID", String("cloud profile missing uid=") + uid, LogLevel::Normal);
      } else {
        cloudLookupFailed = true;
        logEvent("RFID",
                 String("cloud lookup failed uid=") + uid +
                     " http=" + String(firebaseService_.getLastHttpStatusCode()) +
                     " err=" + firebaseService_.getLastErrorSummary(),
                 LogLevel::Normal);
      }
    } else {
      logEvent("RFID", String("cloud lookup deferred (wifi down) uid=") + uid, LogLevel::Normal);
    }
  }

  if (activeUser_ == nullptr && reconcileCloud && cloudEnabled_ && !cloudProfileFound) {
    if (!firebaseService_.isWifiConnected()) {
      logEvent("RFID", String("cloud verify blocked (wifi down) uid=") + uid, LogLevel::Normal);
      setStatusMessage("Cannot verify card without WiFi. Connect and scan again.");
      hideUserLoading("wifi_blocked");
      refreshUi();
      return;
    }
    if (!cloudLookupAttempted || !cloudVerifiedMissing) {
      logEvent("RFID",
               String("cloud verify failed uid=") + uid +
                   " http=" + String(firebaseService_.getLastHttpStatusCode()) +
                   " err=" + firebaseService_.getLastErrorSummary(),
               LogLevel::Normal);
      setStatusMessage("Cloud check failed. Try scanning again.");
      hideUserLoading("cloud_failed");
      refreshUi();
      return;
    }
  }

  if (activeUser_ == nullptr && createIfMissing &&
      (!reconcileCloud || !cloudEnabled_ || cloudVerifiedMissing)) {
    logEvent("RFID", String("creating local profile uid=") + uid, LogLevel::Normal);
    UserProfile profile;
    profile.rfidUid = uid;
    profile.displayName = "";
    profile.hasBasicData = false;
    profile.gender = UserGender::Unspecified;
    profile.goal = TrainingGoal::Hypertrophy;
    if (userRegistry_.upsertProfile(profile)) {
      activeUser_ = userRegistry_.findByUid(uid);
      saveUsers();
    }
  }

  if (activeUser_ == nullptr) {
    logEvent("RFID", String("scan unresolved uid=") + uid, LogLevel::Normal);
    setStatusMessage("Card not recognized.");
    hideUserLoading("unresolved");
    refreshUi();
    return;
  }
  activeUserProfileDirty_ = false;
  activeUserProfileDirtyReason_ = "";
  logEvent("WEIGHT",
           String("machine pin preserved kg=") + String(selectedWeightKg_, 1) +
               " reason=user_switch",
           LogLevel::Normal);
  resolvedRecommendation_ = ResolvedRecommendation{};

  if (uid.equalsIgnoreCase("D6-FA-A5-05") || uid.equalsIgnoreCase("7E-BA-1E-06")) {
    if (!activeUser_->hasBasicData) {
      if (uid.equalsIgnoreCase("D6-FA-A5-05")) {
        activeUser_->displayName = "Usuario1";
        activeUser_->weightKg = 78.0f;
        activeUser_->age = 22;
        activeUser_->heightCm = 175.0f;
      } else {
        activeUser_->displayName = "Usuario2";
        activeUser_->weightKg = 72.0f;
        activeUser_->age = 24;
        activeUser_->heightCm = 172.0f;
      }
      activeUser_->hasBasicData = true;
      activeUser_->goal = TrainingGoal::Hypertrophy;
    }
    if (userRegistry_.findCalibration(*activeUser_, "lat_pulldown") == nullptr) {
      userRegistry_.upsertCalibration(*activeUser_,
                                      "lat_pulldown",
                                      selectedWeightKg_ > 0.0f ? selectedWeightKg_ : 25.0f,
                                      90.0f);
    }
    saveUsers();
  }

  markUserActivity(nowMs);
  idleOverlaySuppressedUntilMs_ = nowMs + kIdleSuppressAfterRfidMs;
  idleShowCandidateMs_ = 0;
  idleHideCandidateMs_ = 0;
  idleWakeCandidateMs_ = 0;

  pendingScanReconcile_ = reconcileCloud;
  pendingScanReconcileEarliestMs_ = reconcileCloud ? (nowMs + kScanReconcileDelayMs) : 0;
  pendingScanReconcileStartedMs_ = reconcileCloud ? nowMs : 0;
  pendingScanReconcileStep_ = 0;
  updateUserLoadingPopupStage(uid, "recommendation");
  refreshActiveCalibration();
  state_ = State::Idle;
  if (!activeUser_->hasBasicData) {
    logEvent("RFID", String("profile incomplete uid=") + uid, LogLevel::Normal);
    setStatusMessage("New card detected. Complete your profile.");
    hideUserLoading("profile_incomplete");
    openProfileEditor(true);
    return;
  }
  logEvent("RFID", String("user loaded uid=") + uid + " name=" + activeUser_->displayName, LogLevel::Normal);
  if (reconcileCloud && cloudEnabled_ && !cloudProfileFound &&
      (!firebaseService_.isWifiConnected() || cloudLookupFailed)) {
    setStatusMessage("User loaded locally. Cloud refresh pending.");
  } else {
    setStatusMessage(reconcileCloud ? "User loaded." : "Test user loaded locally.");
  }
  if (resolvedRecommendation_.hasRecommendation && resolvedRecommendation_.kg > 0.0f) {
    const String source = resolvedRecommendation_.source.isEmpty()
                              ? String("calibration")
                              : resolvedRecommendation_.source;
    const String reason = resolvedRecommendation_.reason.isEmpty()
                              ? String("")
                              : (String(" - ") + resolvedRecommendation_.reason);
    setStatusMessage("Recommended load: " + String(resolvedRecommendation_.kg, 1) +
                     " kg (" + source + ")" + reason);
  }
  if (reconcileCloud && cloudEnabled_) {
    logEvent("USER_SYNC",
             String("profile ok uid=") + uid,
             LogLevel::Normal);
  } else {
    logEvent("USER_SYNC",
             String("recommendation resolved kg=") +
                 String(resolvedRecommendation_.hasRecommendation ? resolvedRecommendation_.kg : 0.0f, 1) +
                 " source=" +
                 (resolvedRecommendation_.hasRecommendation ? resolvedRecommendation_.source : String("fallback")),
             LogLevel::Normal);
    if (activeCalibration_ != nullptr && activeCalibration_->hasCalibration &&
        activeCalibration_->userTopPct > activeCalibration_->userBottomPct) {
      logEvent("USER_SYNC",
               String("rom applied source=user_calibration bottom=") +
                   String(activeCalibration_->userBottomPct, 1) +
                   " top=" + String(activeCalibration_->userTopPct, 1),
               LogLevel::Normal);
    } else {
      logEvent("USER_SYNC", "rom applied source=machine_default", LogLevel::Normal);
    }
    hideUserLoading("sync_complete");
    showUiScreen(UiScreenMode::Main);
  }
  refreshUi();
}

void SmartGymTouchApp::activateAnonymousMode() {
  anonymousMode_ = true;
  activeUser_ = nullptr;
  activeCalibration_ = nullptr;
  lastScannedUid_ = "Anonymous";
  state_ = State::Idle;
  sessionMotionTemplateLatched_ = false;
  resolvedRecommendation_ = ResolvedRecommendation{};
  logEvent("WEIGHT",
           String("machine pin preserved kg=") + String(selectedWeightKg_, 1) +
               " reason=anonymous_mode",
           LogLevel::Normal);
  setStatusMessage("Anonymous mode active.");
  refreshUi();
}

void SmartGymTouchApp::refreshActiveCalibration() {
  activeCalibration_ = nullptr;
  if (activeUser_ == nullptr || machineProfile_ == nullptr) {
    configureRepDetectorThresholds();
    return;
  }

  activeCalibration_ = userRegistry_.findCalibration(*activeUser_, machineProfile_->machineTypeId);
  if (activeCalibration_ != nullptr && activeCalibration_->hasCalibration) {
    updateSelectedWeightFromRecommendation();
  } else {
    resolvedRecommendation_ = resolveRecommendedLoadForActiveUserMachine();
    if (selectedWeightKg_ <= 0.0f && machineProfile_ != nullptr) {
      selectedWeightKg_ = machineProfile_->defaultCalibrationWeightKg;
      logEvent("WEIGHT",
               String("machine pin init kg=") + String(selectedWeightKg_, 1) +
                   " source=machine_default",
               LogLevel::Normal);
    } else {
      logEvent("WEIGHT",
               String("machine pin preserved kg=") + String(selectedWeightKg_, 1) +
                   " reason=calibration_refresh",
               LogLevel::Normal);
    }
  }
  configureRepDetectorThresholds();
}

void SmartGymTouchApp::configureRepDetectorThresholds() {
  String romKey = "machine_default";
  if (activeCalibration_ != nullptr && activeCalibration_->hasCalibration &&
      activeCalibration_->userTopPct > activeCalibration_->userBottomPct) {
    const String uid = activeUser_ != nullptr ? activeUser_->rfidUid : String("none");
    const String machineId = machineProfile_ != nullptr ? machineProfile_->machineTypeId : String("unknown");
    romKey = "user:" + uid + ":" + machineId + ":" +
             String(activeCalibration_->userBottomPct, 1) + ":" +
             String(activeCalibration_->userTopPct, 1);
    if (romKey != lastRomMapLogKey_) {
      lastRomMapLogKey_ = romKey;
      logEvent("ROM",
               String("calibration applied uid=") + uid +
                   " machine=" + machineId +
                   " bottom=" + String(activeCalibration_->userBottomPct, 1) +
                   " top=" + String(activeCalibration_->userTopPct, 1) +
                   " range=" + String(activeCalibration_->userTopPct - activeCalibration_->userBottomPct, 1),
               LogLevel::Normal);
      logEvent("GRAPH", "target source=user_calibration top=100 bottom=0", LogLevel::Normal);
    }
  } else if (romKey != lastRomMapLogKey_) {
    lastRomMapLogKey_ = romKey;
    logEvent("GRAPH", "target source=machine_default top=100 bottom=0", LogLevel::Normal);
  }

  repDetector_.begin(kActiveRomBottomReachedPct, kActiveRomTopReachedPct, kActiveRomMinValidRangePct);
}

void SmartGymTouchApp::applyMachineSensorCalibration() {
  if (!hardwareSensorEnabled_ || machineProfile_ == nullptr) {
    return;
  }

  // If encoder limits calibration is available, prefer that physical
  // reference distance so reported position is real-world distance.
  const float effectiveStrokeMm =
      deviceEncoderCalibrationValid_ ? encoderReferenceDistanceMm_ : machineProfile_->strokeLengthMm;
  sensorManager_.setStrokeLengthMm(effectiveStrokeMm);

  if (deviceEncoderCalibrationValid_) {
    const int minValue = min(static_cast<int>(deviceEncoderZeroRaw_),
                             static_cast<int>(deviceEncoderFullRaw_));
    const int maxValue = max(static_cast<int>(deviceEncoderZeroRaw_),
                             static_cast<int>(deviceEncoderFullRaw_));
    sensorManager_.setCalibrationRange(minValue, maxValue);
    sensorManager_.setInvertDirection(deviceEncoderInvertDirection_);
    sensorManager_.enableAutoRange(false);
  } else {
    sensorManager_.setCalibrationRange(1200, 2800);
    sensorManager_.setInvertDirection(false);
    sensorManager_.enableAutoRange(true);
  }
  configureRepDetectorThresholds();
}

bool SmartGymTouchApp::shouldSuppressOptionalCloudRead(const char* kind, uint32_t nowMs) const {
  if (state_ == State::Calibration) {
    if (kind != nullptr) {
      Serial.printf("[CloudRead] skipped reason=calibration_active kind=%s\n", kind);
    }
    return true;
  }
  const char* busyReason = nullptr;
  uint16_t queueCount = 0;
  if (isSessionUploadBusy(nowMs, &busyReason, &queueCount)) {
    if (kind != nullptr) {
      Serial.printf("[CloudRead] skipped reason=upload_active kind=%s queueCount=%u busyReason=%s phase=%u\n",
                    kind,
                    static_cast<unsigned>(queueCount),
                    busyReason != nullptr ? busyReason : "unknown",
                    static_cast<unsigned>(pendingWebDetailPhase_));
    }
    return true;
  }
  if (lastWebUploadPhaseCompleteMs_ != 0 &&
      (nowMs - lastWebUploadPhaseCompleteMs_) < kCloudReadPostUploadCooldownMs) {
    if (kind != nullptr) {
      Serial.printf("[CloudRead] skipped reason=post_upload_cooldown kind=%s\n", kind);
    }
    return true;
  }
  return false;
}

bool SmartGymTouchApp::isSessionUploadBusy(uint32_t nowMs,
                                           const char** reasonOut,
                                           uint16_t* queueCountOut) const {
  const uint16_t queueCount = localPersistenceStore_.getPendingUploadCount();
  if (queueCountOut != nullptr) {
    *queueCountOut = queueCount;
  }
  if (queueCount > 0) {
    if (reasonOut != nullptr) {
      *reasonOut = "queue_not_empty";
    }
    return true;
  }
  if (pendingWebDetailUpload_ || pendingWebDetailPhase_ != 0) {
    if (reasonOut != nullptr) {
      *reasonOut = "web_phase_active";
    }
    return true;
  }
  if (pendingSessionQueueEnqueue_ || pendingSessionDetailQueueEnqueue_) {
    if (reasonOut != nullptr) {
      *reasonOut = "session_enqueue_pending";
    }
    return true;
  }
  if (!pendingSessionQueueRecord_.sessionId.isEmpty()) {
    if (reasonOut != nullptr) {
      *reasonOut = "session_record_pending";
    }
    return true;
  }
  if (uploadRetryHoldoffUntilMs_ != 0 && nowMs < uploadRetryHoldoffUntilMs_) {
    if (reasonOut != nullptr) {
      *reasonOut = "upload_retry_holdoff";
    }
    return true;
  }
  if (reasonOut != nullptr) {
    *reasonOut = "idle";
  }
  return false;
}

const char* SmartGymTouchApp::uploadMemoryModeText(UploadMemoryMode mode) const {
  switch (mode) {
    case UploadMemoryMode::Normal:
      return "normal";
    case UploadMemoryMode::Constrained:
      return "constrained";
    case UploadMemoryMode::Critical:
    default:
      return "critical";
  }
}

SmartGymTouchApp::UploadMemoryMode SmartGymTouchApp::getUploadMemoryMode(
    uint32_t& internalHeapOut, uint32_t& largestBlockOut) const {
  internalHeapOut = internalFree8BitHeap();
  largestBlockOut = internalLargestFree8BitBlock();
  if (internalHeapOut >= 43000UL && largestBlockOut >= 22000UL) {
    return UploadMemoryMode::Normal;
  }
  if (internalHeapOut >= 23000UL && largestBlockOut >= 9000UL) {
    return UploadMemoryMode::Constrained;
  }
  return UploadMemoryMode::Critical;
}

bool SmartGymTouchApp::shouldSkipRecentCloudRead(const char* kind,
                                                 const String& uid,
                                                 uint32_t nowMs) {
  if (uid.isEmpty()) {
    return false;
  }
  if (kind != nullptr && String(kind) == "profile") {
    if (lastCloudProfileReadUid_ == uid &&
        lastCloudProfileReadMs_ != 0 &&
        (nowMs - lastCloudProfileReadMs_) < kCloudReadRecentWindowMs) {
      logEvent("CloudRead",
               String("skipped reason=recent kind=profile uid=") + uid,
               LogLevel::Normal);
      return true;
    }
    lastCloudProfileReadUid_ = uid;
    lastCloudProfileReadMs_ = nowMs;
    return false;
  }
  if (kind != nullptr && String(kind) == "history") {
    if (lastCloudHistoryReadUid_ == uid &&
        lastCloudHistoryReadMs_ != 0 &&
        (nowMs - lastCloudHistoryReadMs_) < kCloudReadRecentWindowMs) {
      logEvent("CloudRead",
               String("skipped reason=recent kind=history uid=") + uid,
               LogLevel::Normal);
      return true;
    }
    lastCloudHistoryReadUid_ = uid;
    lastCloudHistoryReadMs_ = nowMs;
    return false;
  }
  return false;
}

bool SmartGymTouchApp::refreshDeviceCalibrationFromCloud() {
  if (!cloudEnabled_) {
    return false;
  }
  CloudLockGuard cloudLock(cloudMutex_, 0);
  if (!cloudLock) {
    requestCloudSync(true);
    return false;
  }

  const String deviceId = firebaseService_.getDeviceId();
  if (deviceId.isEmpty()) {
    return false;
  }

  String json;
  if (!firebaseService_.fetchDeviceCalibration(deviceId, json)) {
    const int httpStatus = firebaseService_.getLastHttpStatusCode();
    String trimmed = json;
    trimmed.trim();
    if (httpStatus == 200 && (trimmed.isEmpty() || trimmed == "null")) {
      logEvent("SYNC",
               "no cloud device restore found for this device; boot restore complete",
               LogLevel::Normal);
      return true;
    }

    static uint32_t sLastRestoreFailLogMs = 0;
    const uint32_t nowMs = millis();
    if (sLastRestoreFailLogMs == 0 || (nowMs - sLastRestoreFailLogMs) >= 15000U) {
      sLastRestoreFailLogMs = nowMs;
      logEvent("SYNC",
               String("boot restore fetch failed http=") + String(httpStatus) +
                   " err=" + firebaseService_.getLastErrorSummary(),
               LogLevel::Normal);
    }
    return false;
  }

  json.trim();
  if (json.isEmpty() || json == "null") {
    logEvent("SYNC",
             "no cloud device restore found for this device; boot restore complete",
             LogLevel::Normal);
    return true;
  }

  bool changed = false;

  String cloudMachineId;
  if (FirebaseService::jsonStringValue(json, "machineId", cloudMachineId) && !cloudMachineId.isEmpty()) {
    const MachineProfile* selected = machineRegistry_.findById(cloudMachineId);
    if (selected != nullptr &&
        (machineProfile_ == nullptr || !String(machineProfile_->machineId).equalsIgnoreCase(cloudMachineId))) {
      machineProfile_ = selected;
      deviceConfigStore_.saveMachineId(cloudMachineId);
      if (selectedWeightKg_ <= 0.0f) {
        selectedWeightKg_ = machineProfile_->defaultCalibrationWeightKg;
        logEvent("WEIGHT",
                 String("machine pin init kg=") + String(selectedWeightKg_, 1) +
                     " source=boot_restore",
                 LogLevel::Normal);
      }
      logEvent("SYNC", String("boot restore machineId=") + cloudMachineId, LogLevel::Normal);
      changed = true;
    }
  }

  bool cloudValid = false;
  uint32_t cloudZero = 0;
  uint32_t cloudFull = 0;
  float cloudRefMm = 1000.0f;
  bool cloudInvert = false;
  FirebaseService::jsonBoolValue(json, "encoderCalibrationValid", cloudValid);
  if (!cloudValid) {
    if (changed) {
      applyMachineSensorCalibration();
    }
    return changed;
  }
  FirebaseService::jsonUIntValue(json, "encoderZeroRaw", cloudZero);
  FirebaseService::jsonUIntValue(json, "encoderFullRaw", cloudFull);
  FirebaseService::jsonFloatValue(json, "encoderReferenceDistanceMm", cloudRefMm);
  FirebaseService::jsonBoolValue(json, "encoderInvertDirection", cloudInvert);

  deviceEncoderCalibrationValid_ = true;
  deviceEncoderZeroRaw_ = cloudZero;
  deviceEncoderFullRaw_ = cloudFull;
  deviceEncoderInvertDirection_ = cloudInvert;
  encoderReferenceDistanceMm_ = cloudRefMm;
  encoderZeroCaptured_ = true;
  encoderFullCaptured_ = true;
  encoderZeroRaw_ = cloudZero;
  encoderFullRaw_ = cloudFull;
  encoderCalibrationReady_ = true;
  encoderCalibrationSummary_ =
      "Encoder limits restored from cloud: min " + String(encoderZeroRaw_) +
      ", max " + String(encoderFullRaw_) +
      ", distance " + String(encoderReferenceDistanceMm_, 0) + "mm";

  deviceConfigStore_.saveEncoderCalibration(true,
                                            deviceEncoderZeroRaw_,
                                            deviceEncoderFullRaw_,
                                            encoderReferenceDistanceMm_,
                                            deviceEncoderInvertDirection_);
  applyMachineSensorCalibration();
  changed = true;
  return changed;
}

MachineCloudConfig SmartGymTouchApp::buildCurrentMachineConfig() const {
  MachineCloudConfig cloudConfig;
  if (machineProfile_ == nullptr) {
    return cloudConfig;
  }

  cloudConfig.machineId = machineProfile_->machineId;
  cloudConfig.exerciseCategory = machineProfile_->exerciseCategory;
  cloudConfig.primaryMuscleGroup = machineProfile_->primaryMuscleGroup;
  cloudConfig.secondaryMuscleGroup = machineProfile_->secondaryMuscleGroup;
  cloudConfig.version = machineProfile_->cloudVersion;
  cloudConfig.updatedAtEpoch = cloudEnabled_ ? firebaseService_.getCurrentEpoch() : timeService_.getEpoch();
  cloudConfig.strokeLengthMm = machineProfile_->strokeLengthMm;
  cloudConfig.idealRomPercent = machineProfile_->idealRomPercent;
  cloudConfig.defaultCalibrationWeightKg = machineProfile_->defaultCalibrationWeightKg;
  cloudConfig.targetRepsPerSet = machineProfile_->targetRepsPerSet;
  for (uint8_t i = 0; i < 5; ++i) {
    cloudConfig.recommendations[i] = machineProfile_->recommendations[i];
  }
  cloudConfig.valid = true;
  return cloudConfig;
}

bool SmartGymTouchApp::pushCurrentMachineConfigToCloud() {
  if (!cloudEnabled_ || machineProfile_ == nullptr) {
    return false;
  }
  CloudLockGuard cloudLock(cloudMutex_, 0);
  if (!cloudLock) {
    pendingMachineConfigUpload_ = true;
    requestCloudSync(true);
    return false;
  }

  const MachineCloudConfig cloudConfig = buildCurrentMachineConfig();
  if (!firebaseService_.pushMachineConfig(cloudConfig)) {
    pendingMachineConfigUpload_ = true;
    return false;
  }

  pendingMachineConfigUpload_ = false;
  return true;
}

void SmartGymTouchApp::captureEncoderZero() {
  const uint32_t capturedRaw = hardwareSensorEnabled_
                                   ? static_cast<uint32_t>(max(0, lastSensorReading_.raw))
                                   : static_cast<uint32_t>(simulatedRomPercent_ * 40.95f);
  encoderZeroRaw_ = capturedRaw;
  encoderZeroCaptured_ = true;
  encoderCalibrationSummary_ = "Machine MIN captured at raw " + String(encoderZeroRaw_);
  setStatusMessage(hardwareSensorEnabled_ ? "Captured machine MIN position."
                                          : "Simulated machine MIN captured.");
  refreshUi();
}

void SmartGymTouchApp::captureEncoderFull() {
  const uint32_t capturedRaw = hardwareSensorEnabled_
                                   ? static_cast<uint32_t>(max(0, lastSensorReading_.raw))
                                   : static_cast<uint32_t>(simulatedRomPercent_ * 40.95f);
  encoderFullRaw_ = capturedRaw;
  encoderFullCaptured_ = true;
  encoderCalibrationSummary_ = "Machine MAX captured at raw " + String(encoderFullRaw_);
  setStatusMessage(hardwareSensorEnabled_ ? "Captured machine MAX position."
                                          : "Simulated machine MAX captured.");
  refreshUi();
}

void SmartGymTouchApp::applyEncoderCalibration() {
  if (!encoderZeroCaptured_ || !encoderFullCaptured_ || encoderZeroRaw_ == encoderFullRaw_) {
    setStatusMessage("Capture both encoder points first.");
    refreshUi();
    return;
  }

  if (machineProfile_ == nullptr) {
    setStatusMessage("Machine profile missing.");
    refreshUi();
    return;
  }

  deviceEncoderCalibrationValid_ = true;
  deviceEncoderZeroRaw_ = encoderZeroRaw_;
  deviceEncoderFullRaw_ = encoderFullRaw_;
  // SensorManager normalizes using sorted min/max raw values. To keep ROM=0 at
  // the captured MIN point and ROM=100 at captured MAX point, invert only when
  // the MIN capture has a higher raw value than MAX capture.
  deviceEncoderInvertDirection_ = encoderZeroRaw_ > encoderFullRaw_;
  deviceConfigStore_.saveEncoderCalibration(true,
                                            deviceEncoderZeroRaw_,
                                            deviceEncoderFullRaw_,
                                            encoderReferenceDistanceMm_,
                                            deviceEncoderInvertDirection_);
  applyMachineSensorCalibration();
  if (cloudEnabled_) {
    syncDeviceIdentityToCloud(millis(), true);
  }

  encoderCalibrationReady_ = true;
  const float spanCounts = static_cast<float>(abs(static_cast<int32_t>(deviceEncoderFullRaw_) -
                                                   static_cast<int32_t>(deviceEncoderZeroRaw_)));
  const float countsPerCm = spanCounts / (encoderReferenceDistanceMm_ / 10.0f);
  encoderCalibrationSummary_ =
      "Machine limits saved: min " + String(deviceEncoderZeroRaw_) +
      ", max " + String(deviceEncoderFullRaw_) +
      ", " + String(countsPerCm, 2) + " counts/cm";
  setStatusMessage(cloudEnabled_ ? "Machine limits saved to device + Firebase."
                                  : "Machine limits saved locally on device.");
  refreshUi();
}

void SmartGymTouchApp::resetEncoderCalibration() {
  encoderZeroCaptured_ = false;
  encoderFullCaptured_ = false;
  encoderZeroRaw_ = 0;
  encoderFullRaw_ = 0;
  encoderCalibrationReady_ = false;
  encoderCalibrationSummary_ = "Encoder calibration: not captured.";
  deviceEncoderCalibrationValid_ = false;
  deviceEncoderZeroRaw_ = 0;
  deviceEncoderFullRaw_ = 0;
  deviceEncoderInvertDirection_ = false;
  deviceConfigStore_.saveEncoderCalibration(false, 0, 0, encoderReferenceDistanceMm_, false);
  applyMachineSensorCalibration();
  if (cloudEnabled_) {
    syncDeviceIdentityToCloud(millis(), true);
  }

  setStatusMessage("Encoder calibration reset.");
  refreshUi();
}

void SmartGymTouchApp::captureUserRomBottom() {
  userRomBottomCapturePct_ = constrain(buildDisplayedLiveRomPercent(), 0.0f, 100.0f);
  userRomBottomCaptured_ = true;
  setStatusMessage("Captured user ROM bottom at " + String(userRomBottomCapturePct_, 1) + "%.");
  refreshUi();
}

void SmartGymTouchApp::captureUserRomTop() {
  userRomTopCapturePct_ = constrain(buildDisplayedLiveRomPercent(), 0.0f, 100.0f);
  userRomTopCaptured_ = true;
  setStatusMessage("Captured user ROM top at " + String(userRomTopCapturePct_, 1) + "%.");
  refreshUi();
}

void SmartGymTouchApp::applyUserRomCalibration() {
  if (activeUser_ == nullptr || machineProfile_ == nullptr) {
    setStatusMessage("Load a user and machine first.");
    refreshUi();
    return;
  }
  if (!userRomBottomCaptured_ || !userRomTopCaptured_) {
    setStatusMessage("Capture U.BOTTOM and U.TOP first.");
    refreshUi();
    return;
  }
  const float bottom = min(userRomBottomCapturePct_, userRomTopCapturePct_);
  const float top = max(userRomBottomCapturePct_, userRomTopCapturePct_);
  if (top - bottom < 8.0f) {
    setStatusMessage("User ROM span too short. Capture wider range.");
    refreshUi();
    return;
  }
  const float span = top - bottom;
  const float suggestedWeight = activeCalibration_ != nullptr && activeCalibration_->hasCalibration
                                    ? activeCalibration_->suggestedWeightKg
                                    : selectedWeightKg_;
  activeCalibration_ = userRegistry_.upsertCalibration(*activeUser_,
                                                       machineProfile_->machineTypeId,
                                                       suggestedWeight,
                                                       span,
                                                       bottom,
                                                       top);
  if (activeCalibration_ != nullptr) {
    activeCalibration_->updatedAtEpoch = cloudEnabled_ ? firebaseService_.getCurrentEpoch() : timeService_.getEpoch();
    activeCalibration_->updatedAtIso = buildTimeText();
    saveUsers();
    syncActiveCalibrationToCloud();
    configureRepDetectorThresholds();
    setStatusMessage("User ROM saved: " + String(bottom, 0) + "-" + String(top, 0) + "%.");
  } else {
    setStatusMessage("Failed to save user ROM calibration.");
  }
  refreshUi();
}

void SmartGymTouchApp::setMachineById(const String& machineId) {
  const MachineProfile* selected = machineRegistry_.findById(machineId);
  if (selected == nullptr) {
    return;
  }

  machineProfile_ = selected;
  deviceConfigStore_.saveMachineId(machineId);
  applyMachineSensorCalibration();
  refreshActiveCalibrationFromCloud();
  refreshActiveCalibration();
  configureRepDetectorThresholds();
  setStatusMessage("Machine changed.");
  refreshUi();
}

const GoalRecommendation* SmartGymTouchApp::getRecommendation() const {
  if (machineProfile_ == nullptr) {
    return nullptr;
  }

  TrainingGoal goal = activeUser_ != nullptr ? activeUser_->goal : TrainingGoal::Hypertrophy;
  const bool allowTestGoal = isDebugGoalSelectionEnabled();
  if (goal == TrainingGoal::Test && !allowTestGoal) {
    goal = TrainingGoal::Hypertrophy;
  } else if (goal != TrainingGoal::Strength &&
             goal != TrainingGoal::Hypertrophy &&
             goal != TrainingGoal::Endurance &&
             goal != TrainingGoal::Test) {
    goal = TrainingGoal::Hypertrophy;
  }
  return machineRegistry_.findGoalRecommendation(*machineProfile_, goal);
}

uint8_t SmartGymTouchApp::getTargetSets() const {
  if ((state_ == State::Training || state_ == State::Summary) && activeSessionTargetSets_ > 0) {
    return activeSessionTargetSets_;
  }
  const GoalRecommendation* recommendation = getRecommendation();
  return recommendation != nullptr ? recommendation->targetSets : 3;
}

uint8_t SmartGymTouchApp::getTargetReps() const {
  if ((state_ == State::Training || state_ == State::Summary) && activeSessionTargetRepsMax_ > 0) {
    return activeSessionTargetRepsMax_;
  }
  const GoalRecommendation* recommendation = getRecommendation();
  return recommendation != nullptr ? recommendation->repsMax : 10;
}

const char* SmartGymTouchApp::stateToText() const {
  switch (state_) {
    case State::Idle:
      return "READY";
    case State::Calibration:
      return "CALIBRATING";
    case State::Training:
      return "TRAINING";
    case State::Summary:
      return "SUMMARY";
  }
  return "UNKNOWN";
}

String SmartGymTouchApp::buildUserText() const {
  if (anonymousMode_) {
    return "USER  Anonymous";
  }
  if (activeUser_ == nullptr) {
    return "USER  None";
  }
  return "USER  " + activeUser_->displayName;
}

String SmartGymTouchApp::buildMachineText() const {
  return machineProfile_ != nullptr ? machineProfile_->displayName : String("No machine");
}

String SmartGymTouchApp::buildSuggestionText() const {
  const GoalRecommendation* recommendation = getRecommendation();
  const uint8_t targetSets = activeSessionTargetSets_ > 0 ? activeSessionTargetSets_ :
                              (recommendation != nullptr ? recommendation->targetSets : 3);
  const uint8_t targetRepsMin = activeSessionTargetRepsMin_ > 0 ? activeSessionTargetRepsMin_ :
                                 (recommendation != nullptr ? recommendation->repsMin : 0);
  const uint8_t targetRepsMax = activeSessionTargetRepsMax_ > 0 ? activeSessionTargetRepsMax_ :
                                 (recommendation != nullptr ? recommendation->repsMax : 10);
  const uint16_t restSeconds = activeSessionRestSeconds_ > 0 ? activeSessionRestSeconds_ :
                                (recommendation != nullptr ? recommendation->restSeconds : 45);

  String repTarget;
  if (targetRepsMin > 0 && targetRepsMin != targetRepsMax) {
    repTarget = String(targetRepsMin) + "-" + String(targetRepsMax);
  } else {
    repTarget = String(targetRepsMax);
  }

  String text = "Plan ";
  text += String(targetSets);
  text += " x ";
  text += repTarget;
  text += " | Rest ";
  text += String(restSeconds);
  text += "s";
  if (activeCalibration_ != nullptr && activeCalibration_->nextRecommendedWeightKg > 0.0f) {
    text += " | Recommended ";
    text += String(activeCalibration_->nextRecommendedWeightKg, 1);
    text += "kg";
  }
  return text;
}

String SmartGymTouchApp::buildSessionTimerText(uint32_t nowMs) const {
  if (sessionStartMs_ == 0 || !sessionRecorder_.isActive()) {
    return "--:--";
  }

  const uint32_t elapsedMs = nowMs >= sessionStartMs_ ? nowMs - sessionStartMs_ : 0;
  const uint32_t elapsedSeconds = elapsedMs / 1000U;
  const uint32_t elapsedMinutes = elapsedSeconds / 60U;
  const uint32_t elapsedRemainder = elapsedSeconds % 60U;

  String text;
  if (elapsedMinutes < 10) {
    text += "0";
  }
  text += String(elapsedMinutes);
  text += ":";
  if (elapsedRemainder < 10) {
    text += "0";
  }
  text += String(elapsedRemainder);

  if (state_ == State::Training && activeSessionTargetSets_ > 0) {
    const bool betweenSets = completedSets_ > 0 && currentSetRepCount_ == 0 && completedSets_ < activeSessionTargetSets_;
    if (betweenSets && lastSetCompletedMs_ != 0 && activeSessionRestSeconds_ > 0) {
      const uint32_t restElapsed = nowMs >= lastSetCompletedMs_ ? nowMs - lastSetCompletedMs_ : 0;
      const uint32_t restRemaining = activeSessionRestSeconds_ * 1000UL > restElapsed
                                         ? (activeSessionRestSeconds_ * 1000UL - restElapsed)
                                         : 0;
      text += " | REST ";
      if (restRemaining > 0) {
        text += String((restRemaining + 999UL) / 1000UL);
        text += "s";
      } else {
        text += "GO";
      }
    }
  }

  return text;
}

String SmartGymTouchApp::buildRestCountdownText(uint32_t nowMs) const {
  uint32_t remainingMs = 0;
  if (!isRestCountdownActive(nowMs, &remainingMs)) {
    return String("");
  }
  return String((remainingMs + 999UL) / 1000UL) + "s";
}

float SmartGymTouchApp::buildIdealMotionPercent(uint32_t nowMs) {
  if (idealGraphStartMs_ == 0) {
    return 0.0f;
  }
  if (nowMs < idealGraphStartMs_) {
    return 0.0f;
  }

  MotionGuideTemplate tpl{};
  if ((state_ == State::Training || state_ == State::Summary) && sessionMotionTemplateLatched_) {
    tpl.riseMs = sessionTplRiseMs_;
    tpl.topPauseMs = sessionTplTopPauseMs_;
    tpl.lowerMs = sessionTplLowerMs_;
    tpl.bottomPauseMs = sessionTplBottomPauseMs_;
  } else {
    MotionTargetConfig motionTargets;
    if (deriveMotionTargetsForActiveMachine(
            motionTargets,
            activeUser_ != nullptr ? activeUser_->goal : TrainingGoal::Hypertrophy,
            isDebugGoalSelectionEnabled(),
            "motion_curve")) {
      tpl.riseMs = static_cast<uint16_t>(motionTargets.riseTimeSecDefault * 1000.0f);
      tpl.topPauseMs = static_cast<uint16_t>(motionTargets.topPauseSec * 1000.0f);
      tpl.lowerMs = static_cast<uint16_t>(motionTargets.lowerTimeSecDefault * 1000.0f);
      tpl.bottomPauseMs = static_cast<uint16_t>(motionTargets.bottomPauseSec * 1000.0f);
    } else {
      const GoalRecommendation* recommendation = getRecommendation();
      tpl = buildMotionGuideTemplate(machineProfile_, recommendation);
    }
  }
  const uint32_t cycleMs = max<uint32_t>(1200UL, motionGuideCycleMs(tpl));

  const int32_t delta = static_cast<int32_t>(nowMs) - static_cast<int32_t>(idealPhaseLastTickMs_);
  int32_t samplePhase = static_cast<int32_t>(idealPhaseMs_) + delta;
  samplePhase %= static_cast<int32_t>(cycleMs);
  if (samplePhase < 0) {
    samplePhase += static_cast<int32_t>(cycleMs);
  }

  // The target curve is in active normalized ROM space: bottom=0, top=100.
  const float minRom = 0.0f;
  const float maxRom = 100.0f;

  const uint32_t riseEnd = tpl.riseMs;
  const uint32_t topEnd = riseEnd + tpl.topPauseMs;
  const uint32_t lowerEnd = topEnd + tpl.lowerMs;
  const uint32_t phaseMs = static_cast<uint32_t>(samplePhase);

  auto smoothStep01 = [](float t) -> float {
    t = constrain(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
  };

  if (phaseMs < riseEnd) {
    const float t = smoothStep01(static_cast<float>(phaseMs) / max<uint32_t>(1, tpl.riseMs));
    const float v = minRom + (maxRom - minRom) * t;
    if (v <= 0.8f) return 0.0f;
    if (v >= 99.2f) return 100.0f;
    return v;
  }
  if (phaseMs < topEnd) {
    return maxRom >= 99.2f ? 100.0f : maxRom;
  }
  if (phaseMs < lowerEnd) {
    const uint32_t dt = phaseMs - topEnd;
    const float t = smoothStep01(static_cast<float>(dt) / max<uint32_t>(1, tpl.lowerMs));
    const float v = maxRom + (minRom - maxRom) * t;
    if (v <= 0.8f) return 0.0f;
    if (v >= 99.2f) return 100.0f;
    return v;
  }
  return 0.0f;
}

String SmartGymTouchApp::buildRepPhaseText() const {
  if (state_ == State::Calibration) {
    return "CALIB";
  }

  if (state_ == State::Training && currentSetRepCount_ == 0 && lastSetCompletedMs_ != 0 &&
      activeSessionRestSeconds_ > 0) {
    return "REST";
  }

  const float absVelocity = fabsf(simulatedVelocityPctPerSec_);
  if (absVelocity < 8.0f) {
    return "HOLD";
  }
  if (simulatedVelocityPctPerSec_ > 0.0f) {
    return "LIFT";
  }
  if (simulatedVelocityPctPerSec_ < 0.0f) {
    return "LOWER";
  }
  return "READY";
}

String SmartGymTouchApp::buildSpeedSummaryText() const {
  String summary = "Timing";
  uint16_t riseMs = sessionTplRiseMs_;
  uint16_t lowerMs = sessionTplLowerMs_;
  uint16_t topPauseMs = sessionTplTopPauseMs_;
  uint16_t bottomPauseMs = sessionTplBottomPauseMs_;
  if (!sessionMotionTemplateLatched_) {
    const GoalRecommendation* recommendation = getRecommendation();
    const MotionGuideTemplate tpl = buildMotionGuideTemplate(machineProfile_, recommendation);
    riseMs = tpl.riseMs;
    lowerMs = tpl.lowerMs;
    topPauseMs = tpl.topPauseMs;
    bottomPauseMs = tpl.bottomPauseMs;
  }
  summary += " R";
  summary += String(static_cast<float>(riseMs) / 1000.0f, 1);
  summary += " L";
  summary += String(static_cast<float>(lowerMs) / 1000.0f, 1);
  summary += " T";
  summary += String(static_cast<float>(topPauseMs) / 1000.0f, 1);
  summary += " B";
  summary += String(static_cast<float>(bottomPauseMs) / 1000.0f, 1);
  return summary;
}

String SmartGymTouchApp::buildLiveFeedbackText(uint32_t nowMs) const {
  if (state_ == State::Idle && activeUser_ == nullptr && !anonymousMode_) {
    return "Scan your band or start moving to begin.";
  }

  if (state_ == State::Calibration) {
    switch (calibrationFlowState_) {
      case CalibrationFlowState::Intro:
      case CalibrationFlowState::SetBottomRom:
        return "Step 1 of 5: Move to the bottom position, then tap Set Bottom.";
      case CalibrationFlowState::SetTopRom:
        return "Step 2 of 5: Move to the top position, then tap Set Top.";
      case CalibrationFlowState::RecommendStartWeight:
      case CalibrationFlowState::ConfirmStartWeight:
        return "Step 3 of 5: Set machine load, then tap Weight set.";
      case CalibrationFlowState::CollectSet:
      case CalibrationFlowState::AnalyzeSet:
        return "Step 4 of 5: Perform 3 to 5 smooth reps.";
      case CalibrationFlowState::AskNextSet:
        return "Step 4 of 5: Next set suggested. Tap Next set or Finish.";
      case CalibrationFlowState::Result:
      case CalibrationFlowState::Saving:
      case CalibrationFlowState::Saved:
        return "Step 5 of 5: Review recommendation and tap Save.";
      default:
        return "Calibration in progress.";
    }
  }

  if (state_ == State::Training) {
    if (completedSets_ >= activeSessionTargetSets_ && activeSessionTargetSets_ > 0) {
      return "Workout complete. Tap Finish when ready.";
    }

    if (currentSetRepCount_ == 0 && lastSetCompletedMs_ != 0 && activeSessionRestSeconds_ > 0) {
      const String restText = buildRestCountdownText(nowMs);
      if (!restText.isEmpty()) {
        return "Rest " + restText + ", then start the next set.";
      }
    }
  }

  if (state_ == State::Summary) {
    return lastSessionSummary_.isEmpty()
               ? String("Session complete. Tap Start when you are ready for another set.")
               : lastSessionSummary_ + " ? Tap START to go again.";
  }

  if (hasLastCompletedRep_) {
    return lastRepSummary_ + " ? " + buildQualityText();
  }

  return "Follow the path. Smooth reps win.";
}

String SmartGymTouchApp::buildWorkoutDetailText(uint32_t nowMs) const {
  if (state_ == State::Idle && activeUser_ == nullptr && !anonymousMode_) {
    return "Plan " + String(getTargetSets()) + "x" + String(getTargetReps()) +
           " ? Rest " + String(activeSessionRestSeconds_) + "s";
  }

  if (state_ == State::Calibration) {
    return "Calibration set " + String(max<uint8_t>(1, calibrationCurrentSetIndex_)) +
           " ? valid " + String(calibrationValidRepsInSet_) + "/" + String(calibrationTargetRepsPerSet_) +
           " ? ROM " + String(simulatedRomPercent_, 0) + "%";
  }

  if (state_ == State::Training) {
    if (completedSets_ >= activeSessionTargetSets_ && activeSessionTargetSets_ > 0) {
      return "Done ? " + String(completedSets_ * getTargetReps()) + " reps";
    }

    if (currentSetRepCount_ == 0 && lastSetCompletedMs_ != 0 && activeSessionRestSeconds_ > 0) {
      const String restText = buildRestCountdownText(nowMs);
      if (!restText.isEmpty()) {
        return "Rest " + restText + " ? Reset grip";
      }
    }
    if (!hasLastCompletedRep_) {
      return "Last rep\nWaiting for first rep";
    }
    if (lastCompletedRep_.valid) {
      return "Last rep: counted\nROM " + String(lastCompletedRep_.romPercent, 0) +
             "% ? Tempo good";
    }
    return "Last rep: check\n" + buildInvalidReason(lastCompletedRep_);
  }

  if (state_ == State::Summary) {
    return lastSessionSummary_.isEmpty()
               ? String("Complete ? ") + String(completedSets_ * getTargetReps()) + " reps"
               : lastSessionSummary_;
  }

  if (hasLastCompletedRep_) {
    return lastRepSummary_ + " ? " + buildQualityText();
  }

  return "Ready ? " + String(getTargetSets()) + "x" + String(getTargetReps()) +
         " ? Rest " + String(activeSessionRestSeconds_) + "s";
}

String SmartGymTouchApp::buildSessionMetaText(uint32_t nowMs) const {
  const String bestSpeed = sessionBestRepPeakVelocityPctPerSec_ > 0.0f
                               ? String(sessionBestRepPeakVelocityPctPerSec_, 0)
                               : String("--");
  const SessionHistoryRecord& sessionRecord = sessionRecorder_.getRecord();
  String qualityText = "Q --";
  if (sessionRecord.validReps > 0) {
    const float safeTargetSets = sessionRecord.targetSets > 0 ? static_cast<float>(sessionRecord.targetSets) : 1.0f;
    const float plannedReps =
        safeTargetSets * static_cast<float>(sessionRecord.targetRepsMax > 0 ? sessionRecord.targetRepsMax : 1);
    const float repRatio =
        constrain(static_cast<float>(sessionRecord.validReps) / max(1.0f, plannedReps), 0.0f, 1.2f);
    const float totalReps = max(1.0f, static_cast<float>(sessionRecord.validReps + sessionRecord.invalidReps));
    const float validRatio = constrain(static_cast<float>(sessionRecord.validReps) / totalReps, 0.0f, 1.0f);
    const float romScore = constrain(sessionRecord.avgRomPercent / 95.0f, 0.0f, 1.0f) * 100.0f;
    const float repScore = constrain(repRatio, 0.0f, 1.0f) * 100.0f;
    const float validScore = validRatio * 100.0f;
    const float velScore = constrain(sessionRecord.avgPeakVelocityPctPerSec / 120.0f, 0.0f, 1.0f) * 100.0f;
    const float qualityScore =
        (romScore * 0.35f) + (repScore * 0.25f) + (validScore * 0.25f) + (velScore * 0.15f);
    const char* tier = qualityScore >= 85.0f ? "EXC"
                      : (qualityScore >= 70.0f ? "GOOD" : (qualityScore >= 55.0f ? "OK" : "BAD"));
    qualityText = "Q " + String(tier) + " " + String(qualityScore, 0);
  }

  if (state_ == State::Idle && activeUser_ == nullptr && !anonymousMode_) {
    return "Timing R" + String(static_cast<float>(sessionTplRiseMs_) / 1000.0f, 1) +
           " L" + String(static_cast<float>(sessionTplLowerMs_) / 1000.0f, 1) +
           " ? Load " + String(selectedWeightKg_, 1) + "kg";
  }

  if (state_ == State::Calibration) {
    return "Calib set " + String(calibrationCurrentSetIndex_) +
           " ? ROM " + String(simulatedRomPercent_, 0) +
           "% ? Weight " + String(selectedWeightKg_, 1) + "kg";
  }

  if (state_ == State::Training) {
    if (completedSets_ >= activeSessionTargetSets_ && activeSessionTargetSets_ > 0) {
      return "Done ? " + String(completedSets_ * getTargetReps()) + " reps";
    }

    if (currentSetRepCount_ == 0 && lastSetCompletedMs_ != 0 && activeSessionRestSeconds_ > 0) {
      const String restText = buildRestCountdownText(nowMs);
      if (!restText.isEmpty()) {
        return "Rest " + restText + "\nNext set " +
               String(completedSets_ + 1) + "/" + String(activeSessionTargetSets_);
      }
    }
    const SessionHistoryRecord& rec = sessionRecorder_.getRecord();
    const uint32_t totalAttempts =
        static_cast<uint32_t>(rec.validReps) + static_cast<uint32_t>(rec.invalidReps);
    if (totalAttempts == 0) {
      return "Session\nGoal " + String(activeSessionTargetSets_) + "x" +
             String(getTargetReps()) + " ? Set " + String(completedSets_ + 1);
    }
    if (rec.invalidReps > 0) {
      return "Session: " + String(rec.validReps) + " good ? " +
             String(rec.invalidReps) + " check\nAvg ROM " +
             String(rec.avgRomPercent, 0) + "% ? Best rep " + bestSpeed;
    }
    return "Session: " + String(rec.validReps) + " good\nAvg ROM " +
           String(rec.avgRomPercent, 0) + "% ? Best rep " + bestSpeed;
  }

  if (state_ == State::Summary) {
    return lastSessionSummary_.isEmpty()
               ? String("Complete ? tap START")
               : lastSessionSummary_;
  }

  if (hasLastCompletedRep_) {
    return qualityText + " ? Best " + bestSpeed;
  }

  return "Timing R" + String(static_cast<float>(sessionTplRiseMs_) / 1000.0f, 1) +
         " L" + String(static_cast<float>(sessionTplLowerMs_) / 1000.0f, 1) +
         " ? Load " + String(selectedWeightKg_, 1) + "kg";
}

String SmartGymTouchApp::buildStatusText() const {
  if (state_ == State::Idle && activeUser_ == nullptr && !anonymousMode_) {
    return "Scan RFID or move machine to start.";
  }
  if (state_ == State::Calibration) {
    return statusMessage_.isEmpty()
               ? String("Calibration in progress. Perform full controlled reps through the range of motion.")
               : statusMessage_;
  }
  if (state_ == State::Training) {
    return statusMessage_.isEmpty() ? String("Training active.") : statusMessage_;
  }
  if (state_ == State::Summary) {
    return "Session complete. Review result and start again when ready.";
  }
  return statusMessage_.isEmpty() ? String("Choose your load, follow the path, and press Start.") : statusMessage_;
}

String SmartGymTouchApp::buildQualityText() const {
  if (state_ == State::Calibration) {
    return "CALIBRATION";
  }

  const float absVelocity = fabsf(simulatedVelocityPctPerSec_);
  if (state_ == State::Training && currentSetRepCount_ == 0 && lastSetCompletedMs_ != 0 && activeSessionRestSeconds_ > 0) {
    return "REST";
  }

  if (absVelocity < 8.0f) {
    return "SETTLE";
  }
  if (absVelocity < 24.0f) {
    return "SLOW";
  }
  if (absVelocity <= 60.0f) {
    return "SPEED OK";
  }
  return "TOO FAST";
}

String SmartGymTouchApp::buildInvalidReason(const RepMetrics& rep) const {
  String reason;
  if (rep.invalidFlags & RepInvalidShortRom) {
    reason += "short ROM";
  }
  if (rep.invalidFlags & RepInvalidTooFast) {
    if (!reason.isEmpty()) {
      reason += " ? ";
    }
    reason += "too fast";
  }
  if (rep.invalidFlags & RepInvalidTopNotReached) {
    if (!reason.isEmpty()) {
      reason += " ? ";
    }
    reason += "top not reached";
  }
  if (rep.invalidFlags & RepInvalidNoConcentricPhase) {
    if (!reason.isEmpty()) {
      reason += " ? ";
    }
    reason += "no concentric phase";
  }
  return reason.isEmpty() ? String("invalid pattern") : reason;
}

String SmartGymTouchApp::buildSessionId() const {
  if (cloudEnabled_ && firebaseService_.getCurrentEpoch() != 0) {
    return "session_" + String(firebaseService_.getCurrentEpoch()) + "_" + String(millis());
  }
  return "session_" + String(millis());
}

String SmartGymTouchApp::buildTimeText() const {
  if (timeService_.hasValidTime()) {
    time_t now = static_cast<time_t>(timeService_.getEpoch());
    struct tm localTime {};
    localtime_r(&now, &localTime);
    char buffer[40] = {0};
    strftime(buffer, sizeof(buffer), "%a %d %b?%I:%M %p", &localTime);
    return String(buffer);
  }
  return "Time syncing?--:--";
}

String SmartGymTouchApp::buildDebugStatusText() const {
  String text;
  text.reserve(420);
  text += "Runtime ";
  text += sensorSimulationEnabled_ ? "SIM" : "LIVE";
  text += " ? Cloud ";
  text += cloudEnabled_ ? "ON" : "OFF";
  text += " ? WiFi ";
  text += (cloudEnabled_ && firebaseService_.isWifiConnected()) ? "OK" : "OFF";
  const uint16_t pendingCompactJobs = pendingSessionQueueEnqueue_ ? 1U : 0U;
  const uint16_t pendingTotal =
      static_cast<uint16_t>(localPersistenceStore_.getPendingUploadCount() +
                            pendingCompactJobs);
  text += " ? Queue ";
  text += String(pendingTotal);
  if (pendingTotal > 0 || pendingSessionDetailQueueEnqueue_ || pendingWebDetailUpload_) {
    text += " (nvs ";
    text += String(localPersistenceStore_.getPendingUploadCount());
    text += " prep ";
    text += String(pendingCompactJobs);
    if (pendingSessionDetailQueueEnqueue_) {
      text += " detail wait";
    }
    if (pendingWebDetailUpload_) {
      text += " web detail";
    }
    text += ")";
  }
  text += "\nMachine ";
  text += machineProfile_ != nullptr ? machineProfile_->displayName : "None";
  text += " ? User ";
  text += activeUser_ != nullptr ? activeUser_->displayName : (anonymousMode_ ? "Anonymous" : "None");
  text += " ? ROM ";
  text += String(liveRomPercent_, 1);
  text += "% ? Pos ";
  text += String(lastSensorReading_.positionMm, 0);
  text += "mm";
  text += "\nEncoder ";
  text += deviceEncoderCalibrationValid_ ? "READY" : (encoderZeroCaptured_ || encoderFullCaptured_ ? "CAPTURING" : "NOT SET");
  if (encoderZeroCaptured_ || encoderFullCaptured_ || deviceEncoderCalibrationValid_) {
    text += " ? MIN ";
    text += String(deviceEncoderCalibrationValid_ ? deviceEncoderZeroRaw_ : encoderZeroRaw_);
    text += " ? MAX ";
    text += String(deviceEncoderCalibrationValid_ ? deviceEncoderFullRaw_ : encoderFullRaw_);
  }
  text += " ? UROM ";
  if (activeCalibration_ != nullptr && activeCalibration_->hasCalibration) {
    if (fabsf(activeCalibration_->userTopPct - activeCalibration_->userBottomPct) >= 5.0f) {
      text += String(activeCalibration_->userBottomPct, 0);
      text += "-";
      text += String(activeCalibration_->userTopPct, 0);
      text += "%";
    } else if (activeCalibration_->userRomPercent > 1.0f) {
      text += "0-";
      text += String(activeCalibration_->userRomPercent, 0);
      text += "% legacy";
    } else {
      text += "not set";
    }
  } else {
    text += "not set";
  }
  if (cloudEnabled_) {
    text += "\nHTTP ";
    text += String(firebaseService_.getLastHttpStatusCode());
    const String err = firebaseService_.getLastErrorSummary();
    if (!err.isEmpty()) {
      text += " ? ";
      text += err;
    }
  }
  return text;
}

String SmartGymTouchApp::buildHardwareStatusText() const {
  const bool wifiOk = !cloudEnabled_ || firebaseService_.isWifiConnected();
  const bool rfidOk = !hardwareRfidEnabled_ || rfidInitialized_;
  const bool encoderOk = !hardwareSensorEnabled_ || deviceEncoderCalibrationValid_;
  const bool cloudAuthSet = !cloudEnabled_ || !String(kFirebaseAuthToken).isEmpty();
  const bool queueHealthy = localPersistenceStore_.getPendingUploadCount() < 14;

  String health = "HEALTH ";
  health += wifiOk ? "WiFi OK" : "WiFi FAIL";
  health += " ? ";
  health += rfidOk ? "RFID OK" : "RFID FAIL";
  health += " ? ";
  health += encoderOk ? "ENC OK" : "ENC PENDING";
  health += " ? ";
  health += cloudAuthSet ? "AUTH OK" : "AUTH OPEN";
  health += " ? ";
  health += queueHealthy ? "QUEUE OK" : "QUEUE HIGH";

  String text;
  text.reserve(220);
  text += health;
  text += "\nWiFi ";
  if (!cloudEnabled_) {
    text += "off";
  } else if (firebaseService_.isWifiConnected()) {
    text += "ok";
    text += " RSSI ";
    text += String(WiFi.RSSI());
    text += "dBm";
    const String ssid = firebaseService_.getActiveWifiSsid();
    if (!ssid.isEmpty()) {
      text += " (";
      text += ssid;
      text += ")";
    }
  } else {
    const String summary = firebaseService_.getLastErrorSummary();
    text += (!summary.isEmpty() && summary.indexOf("WiFi") >= 0) ? "searching" : "connecting";
  }

  text += " ? RFID ";
  if (!hardwareRfidEnabled_) {
    text += "sim";
  } else {
    text += rfidInitialized_ ? "live" : "waiting";
  }
  if (hardwareRfidEnabled_) {
    text += " v=0x";
    const uint8_t ver = rfidService_.readerVersion();
    if (ver < 0x10) {
      text += "0";
    }
    text += String(ver, HEX);
    text.toUpperCase();
    text += "";
  }
  if (hardwareRfidEnabled_ && rfidInitialized_) {
    text += " ? uid ";
    text += lastScannedUid_.isEmpty() ? "none" : lastScannedUid_;
  }

  text += "\nENC ";
  text += hardwareSensorEnabled_ ? "live" : "sim";
  text += " ? cal ";
  text += deviceEncoderCalibrationValid_ ? "ready" : "pending";
  text += " ? dir ";
  text += deviceEncoderInvertDirection_ ? "inverted" : "normal";
  text += " ? U.ROM cap ";
  text += userRomBottomCaptured_ ? "B" : "-";
  text += "/";
  text += userRomTopCaptured_ ? "T" : "-";
  return text;
}

const char* SmartGymTouchApp::goalDisplayName(TrainingGoal goal) const {
  switch (goal) {
    case TrainingGoal::Strength:
      return "Strength";
    case TrainingGoal::Hypertrophy:
      return "Hypertrophy";
    case TrainingGoal::Endurance:
      return "Endurance";
    case TrainingGoal::Test:
      return isDebugGoalSelectionEnabled() ? "Test" : "Hypertrophy";
    case TrainingGoal::General:
    default:
      return "Hypertrophy";
  }
}

const char* SmartGymTouchApp::genderDisplayName(UserGender gender) const {
  switch (gender) {
    case UserGender::Female:
      return "Female";
    case UserGender::Male:
      return "Male";
    case UserGender::NonBinary:
      return "Non-binary";
    case UserGender::Unspecified:
    default:
      return "Unspecified";
  }
}

String SmartGymTouchApp::nextProfileName(const String& current, int direction) const {
  static const char* kNames[] = {"Alex", "Sam", "Jordan", "Chris", "Taylor", "Morgan", "Casey"};
  constexpr int kNameCount = static_cast<int>(sizeof(kNames) / sizeof(kNames[0]));
  int index = 0;
  for (int i = 0; i < kNameCount; ++i) {
    if (current.equalsIgnoreCase(kNames[i])) {
      index = i;
      break;
    }
  }
  index += direction;
  if (index < 0) {
    index = kNameCount - 1;
  } else if (index >= kNameCount) {
    index = 0;
  }
  return String(kNames[index]);
}

void SmartGymTouchApp::setStatusMessage(const String& message) {
  statusMessage_ = message;
  logEvent("UI", message, LogLevel::Normal);
}

void SmartGymTouchApp::saveUsers() {
  localPersistenceStore_.saveUsers(userRegistry_);
}

void SmartGymTouchApp::pulseTo(float romTarget) {
  autoMotionEnabled_ = false;
  autoMotionPhase_ = AutoMotionPhase::Idle;
  simulatedRomPercent_ = constrain(romTarget, 0.0f, 100.0f);
  liveRomPercent_ = simulatedRomPercent_;
  syncRomDebugSliderIfPresent();
  refreshUi();
}

void SmartGymTouchApp::syncCloudNow() {
  if (!cloudEnabled_) {
    setStatusMessage("Cloud sync is disabled in this build.");
    refreshUi();
    return;
  }

  const uint32_t nowMs = millis();
  const SessionStage stage = getSessionStage(nowMs);
  if (stage == SessionStage::Train) {
    setStatusMessage("Sync deferred: active set in progress (runs on rest/idle).");
    refreshUi();
    return;
  }
  if (kUseCloudSyncWorker) {
    requestCloudSync(true);
  } else {
    updateCloudSync(nowMs, true);
  }
  const uint16_t queueNow = localPersistenceStore_.getPendingUploadCount();
  logEvent("SYNC",
           String("manual sync requested queue=") + String(queueNow) +
               " pendingSessionQueueEnqueue=" + (pendingSessionQueueEnqueue_ ? "1" : "0") +
               " pendingWebDetailUpload=" + (pendingWebDetailUpload_ ? "1" : "0"),
           LogLevel::Normal);
  setStatusMessage("Sync requested ? NVS queue " + String(queueNow));
  refreshUi();
}

void SmartGymTouchApp::openDebugMenu() {
  if (debugModal_ == nullptr || debugPanel_ == nullptr) {
    buildDebugModal(screen_);
  }

  if (debugModal_ == nullptr || debugPanel_ == nullptr) {
    return;
  }

  showUiScreen(UiScreenMode::Debug);
  markUserActivity(millis());
  refreshDebugPanel(millis());
  refreshUi();
}

void SmartGymTouchApp::closeDebugMenu() {
  if (debugModal_ == nullptr || debugPanel_ == nullptr) {
    return;
  }

  markUserActivity(millis());
  refreshIdleOverlay(millis());
  if (currentUiScreen_ == UiScreenMode::Debug) {
    showUiScreen(shouldShowIdleOverlay(millis()) ? UiScreenMode::Idle : UiScreenMode::Main);
  }
  refreshUi();
}

void SmartGymTouchApp::openProfileEditor(bool forceCreateMode) {
  if (activeUser_ == nullptr) {
    setStatusMessage("Scan a card first.");
    refreshUi();
    return;
  }
  if (profileScreen_ == nullptr) {
    buildProfileScreen(screen_);
  }
  profileCreateMode_ = forceCreateMode || !activeUser_->hasBasicData;
  profileEditName_ = activeUser_->displayName.isEmpty() ? String("Alex") : activeUser_->displayName;
  profileEditAge_ = activeUser_->age > 0 ? activeUser_->age : 25;
  profileEditWeightKg_ = activeUser_->weightKg > 0.0f ? activeUser_->weightKg : 70.0f;
  profileEditHeightCm_ = activeUser_->heightCm > 0.0f ? activeUser_->heightCm : 170.0f;
  profileEditGoal_ = normalizeGoalId(activeUser_->goal,
                                     "profile_editor",
                                     isDebugGoalSelectionEnabled());
  profileEditGender_ = activeUser_->gender;
  showUiScreen(UiScreenMode::Profile);
  refreshProfileScreen(millis());
}

void SmartGymTouchApp::closeProfileEditor(bool discardChanges) {
  if (!discardChanges) {
    saveProfileEditor();
  }
  const uint32_t nowMs = millis();
  // Prevent accidental "SAVE tap-through" from triggering TRAIN immediately
  // after leaving profile editor on touch panels.
  trainingStartBlockedUntilMs_ = nowMs + 2200UL;
  lastTickMs_ = 0;
  lastSimulatedRomPercent_ = liveRomPercent_;
  filteredVelocityPctPerSec_ = 0.0f;
  simulatedVelocityPctPerSec_ = 0.0f;
  velocityStillLatched_ = true;
  idleWakeCandidateMs_ = 0;
  setPauseCandidateMs_ = 0;
  repDetector_.reset();
  showUiScreen(shouldShowIdleOverlay(nowMs) ? UiScreenMode::Idle : UiScreenMode::Main);
  refreshUi();
}

bool SmartGymTouchApp::saveProfileEditor() {
  if (activeUser_ == nullptr) {
    return false;
  }
  activeUser_->updatedAtEpoch = firebaseService_.getCurrentEpoch();
  activeUser_->updatedAtIso = firebaseService_.getCurrentIso();
  activeUser_->displayName = profileEditName_;
  activeUser_->age = profileEditAge_;
  activeUser_->weightKg = profileEditWeightKg_;
  activeUser_->heightCm = profileEditHeightCm_;
  activeUser_->goal = normalizeGoalId(profileEditGoal_, "profile_edit", isDebugGoalSelectionEnabled());
  activeUser_->gender = profileEditGender_;
  activeUser_->hasBasicData = true;
  activeUserProfileDirty_ = true;
  activeUserProfileDirtyReason_ = "explicit_edit";
  saveUsers();
  syncActiveUserToCloud();
  setStatusMessage("Profile saved.");
  logEvent("PROFILE", "saved no_calibration_start=1", LogLevel::Normal);
  return true;
}

void SmartGymTouchApp::onButtonEvent(lv_event_t* event) {
  auto* app = static_cast<SmartGymTouchApp*>(lv_event_get_user_data(event));
  if (app == nullptr) {
    return;
  }

  lv_obj_t* target = lv_event_get_target(event);
  const lv_event_code_t code = lv_event_get_code(event);

  if (target == app->profileKeyboard_) {
    if (code == LV_EVENT_READY) {
      if (app->profileNameTa_ != nullptr) {
        app->profileEditName_ = lv_textarea_get_text(app->profileNameTa_);
      }
      if (app->profileKeyboard_ != nullptr) {
        lv_obj_add_flag(app->profileKeyboard_, LV_OBJ_FLAG_HIDDEN);
      }
      if (app->profileNameTa_ != nullptr) {
        lv_obj_add_flag(app->profileNameTa_, LV_OBJ_FLAG_HIDDEN);
      }
      app->refreshProfileScreen(millis());
    } else if (code == LV_EVENT_CANCEL) {
      if (app->profileKeyboard_ != nullptr) {
        lv_obj_add_flag(app->profileKeyboard_, LV_OBJ_FLAG_HIDDEN);
      }
      if (app->profileNameTa_ != nullptr) {
        lv_obj_add_flag(app->profileNameTa_, LV_OBJ_FLAG_HIDDEN);
      }
    }
    return;
  }

  const uint32_t nowMs = millis();
  app->markUserActivity(nowMs);
  constexpr uint32_t kGlobalButtonDebounceMs = 120UL;
  if (code == LV_EVENT_SHORT_CLICKED) {
    if ((nowMs - app->lastButtonEventMs_) < kGlobalButtonDebounceMs) {
      return;
    }
    app->lastButtonEventMs_ = nowMs;
  }
  if (app->userLoading_) {
    app->logEvent("UI", "input blocked reason=user_loading", LogLevel::Normal);
    return;
  }

  if (target == app->btnCalibrationWeightMinusLarge_) {
    if (!app->acceptButtonAction("calibration_pin_-5", 350UL)) return;
    app->adjustCurrentLoad(-5.0f, "calibration_button");
  } else if (target == app->btnCalibrationWeightMinus_) {
    if (!app->acceptButtonAction("calibration_pin_-2.5", 350UL)) return;
    app->adjustCurrentLoad(-kWeightStepKg, "calibration_button");
  } else if (target == app->btnCalibrationWeightPlus_) {
    if (!app->acceptButtonAction("calibration_pin_+2.5", 350UL)) return;
    app->adjustCurrentLoad(kWeightStepKg, "calibration_button");
  } else if (target == app->btnCalibrationWeightPlusLarge_) {
    if (!app->acceptButtonAction("calibration_pin_+5", 350UL)) return;
    app->adjustCurrentLoad(5.0f, "calibration_button");
  } else if (target == app->btnCalibrationPrimary_) {
    if (!app->acceptButtonAction("calibration_primary", 500UL)) return;
    app->updateCalibrationFromButtonPress(false);
  } else if (target == app->btnCalibrationSecondary_) {
    if (!app->acceptButtonAction("calibration_secondary", 500UL)) return;
    app->updateCalibrationFromButtonPress(true);
  } else if (target == app->btnCalibrationCancel_) {
    if (!app->acceptButtonAction("calibration_cancel", 500UL)) return;
    app->cancelCalibration("user_cancel");
  } else if (target == app->btnWeightMinusLarge_) {
    if (!app->acceptButtonAction("pin_-5", 250UL)) return;
    app->adjustCurrentLoad(-5.0f, "button");
  } else if (target == app->btnWeightMinus_) {
    if (!app->acceptButtonAction("pin_-2.5", 250UL)) return;
    app->adjustCurrentLoad(-kWeightStepKg, "button");
  } else if (target == app->btnWeightPlus_) {
    if (!app->acceptButtonAction("pin_+2.5", 250UL)) return;
    app->adjustCurrentLoad(kWeightStepKg, "button");
  } else if (target == app->btnWeightPlusLarge_) {
    if (!app->acceptButtonAction("pin_+5", 250UL)) return;
    app->adjustCurrentLoad(5.0f, "button");
  } else if (target == app->btnCalibrate_ || target == app->btnDebugCalibrate_) {
    if (!app->acceptButtonAction("calibrate", 900UL)) return;
    if (app->state_ == State::Calibration) {
      app->updateCalibrationFromButtonPress(false);
      return;
    }
    if (app->state_ == State::Training || app->state_ == State::Summary) {
      app->setStatusMessage("Calibration unavailable during active/summary session.");
      return;
    }
    app->startCalibration("button_calibrate");
  } else if (target == app->btnTrain_) {
    if (!app->acceptButtonAction("train", 500UL)) return;
    if (app->state_ == State::Calibration) {
      app->updateCalibrationFromButtonPress(true);
      return;
    }
    const SessionStage stage = app->getSessionStage(nowMs);
    const bool canStart = app->canStartTrainingNow(nowMs);
    const bool canFinish = app->canFinishNow(nowMs);
    app->logEvent("BTN",
                  String("train pressed state=") + app->stateToText() +
                      " screen=" + app->uiScreenToText(app->currentUiScreen_) +
                      " recorderActive=" + (app->sessionRecorder_.isActive() ? "1" : "0") +
                      " canStart=" + (canStart ? "1" : "0") +
                      " canFinish=" + (canFinish ? "1" : "0") +
                      " stage=" + app->sessionStageToText(stage),
                  LogLevel::Normal);
    if (canFinish) {
      app->finishRequestPending_ = true;
      app->finishRequestLogoutNow_ = false;
      app->logEvent("BTN", "finish requested source=manual logoutNow=0", LogLevel::Normal);
      app->setStatusMessage("Ending session...");
      return;
    }
    if (!canStart) {
      app->logEvent("BTN",
                    String("finish blocked recorderActive=") +
                        (app->sessionRecorder_.isActive() ? "1" : "0") +
                        " stage=" + app->sessionStageToText(stage),
                    LogLevel::Normal);
      app->setStatusMessage("Training start ignored in current state.");
      return;
    }
    app->logEvent("BTN", "start requested", LogLevel::Normal);
    app->startTraining();
  } else if (target == app->btnSummarySkip_) {
    if (!app->acceptButtonAction("summary_skip", 700UL)) return;
    if (app->state_ == State::Summary) {
      const uint32_t nowMs = millis();
      const bool uploadBusy = app->isSessionUploadBusy(nowMs, nullptr, nullptr);
      app->logEvent("SUMMARY",
                    String("leaving reason=skip uploadBusy=") + (uploadBusy ? "1" : "0"),
                    LogLevel::Normal);
      if (uploadBusy) {
        app->logEvent("SYNC",
                      "summary skipped while upload busy, continuing phased upload",
                      LogLevel::Normal);
      }
      app->logoutActiveUser("Session ended.");
    }
  } else if (target == app->btnUserSwitchCancel_) {
    app->closeUserSwitchPrompt();
    app->setStatusMessage("Cambio de usuario cancelado.");
    app->refreshUi();
  } else if (target == app->btnUserSwitchConfirm_) {
    const String nextUid = app->pendingSwitchUid_;
    app->closeUserSwitchPrompt();
    if (nextUid.isEmpty()) {
      app->setStatusMessage("No hay tarjeta pendiente.");
      app->refreshUi();
      return;
    }
    if (app->state_ == State::Training) {
      app->finishTraining("Session switched to another user.", true);
    }
    app->activateUserByUid(nextUid, true);
  } else if (target == app->btnAutoRep_) {
    const lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_SHORT_CLICKED) {
      app->sensorSimulationEnabled_ = true;
      app->autoMotionEnabled_ = true;
      app->autoMotionPhase_ = AutoMotionPhase::Up;
      app->autoRepContinuous_ = false;
      app->autoRepStopAfterCycle_ = true;
      app->uiCache_.lastAutoMotionMs = millis();
      app->setStatusMessage("Auto rep one cycle.");
      app->refreshUi();
    } else if (code == LV_EVENT_LONG_PRESSED) {
      app->sensorSimulationEnabled_ = true;
      app->autoMotionEnabled_ = true;
      app->autoMotionPhase_ = AutoMotionPhase::Up;
      app->autoRepContinuous_ = true;
      app->autoRepStopAfterCycle_ = false;
      app->uiCache_.lastAutoMotionMs = millis();
      app->setStatusMessage("Auto rep running while held.");
      app->refreshUi();
    } else if (code == LV_EVENT_RELEASED) {
      if (app->autoRepContinuous_) {
        app->autoMotionEnabled_ = false;
        app->autoMotionPhase_ = AutoMotionPhase::Idle;
        app->autoRepContinuous_ = false;
        app->autoRepStopAfterCycle_ = false;
        app->setStatusMessage("Auto rep stopped.");
        app->refreshUi();
      }
    }
  } else if (target == app->btnCalibrationGateCalibrate_) {
    if (!app->acceptButtonAction("calibration_gate_calibrate", 900UL)) return;
    app->pendingWorkoutAfterCalibration_ = true;
    app->startCalibration("button_calibrate");
  } else if (target == app->btnCalibrationGateSkip_) {
    if (!app->acceptButtonAction("calibration_gate_skip", 700UL)) return;
    app->startTraining(true);
  } else if (target == app->btnService_) {
    app->openDebugMenu();
  } else if (target == app->btnAvatar_) {
    app->openProfileEditor(false);
  } else if (target == app->btnCloseDebug_) {
    app->closeDebugMenu();
  } else if (target == app->btnProfileCancel_) {
    if (!app->acceptButtonAction("profile_cancel", 500UL)) return;
    app->closeProfileEditor(true);
  } else if (target == app->btnProfileSave_) {
    if (!app->acceptButtonAction("profile_save", 700UL)) return;
    app->closeProfileEditor(false);
  } else if (target == app->btnProfileEditName_) {
    if (app->profileNameTa_ != nullptr && app->profileKeyboard_ != nullptr) {
      lv_textarea_set_text(app->profileNameTa_, app->profileEditName_.c_str());
      lv_obj_clear_flag(app->profileNameTa_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(app->profileKeyboard_, LV_OBJ_FLAG_HIDDEN);
      lv_keyboard_set_textarea(app->profileKeyboard_, app->profileNameTa_);
    }
    app->refreshProfileScreen(millis());
  } else if (target == app->btnProfileAgeMinus_) {
    const uint32_t now = millis();
    if (now - app->profileAdjustDebounceMs_ < 220) {
      return;
    }
    app->profileAdjustDebounceMs_ = now;
    app->profileEditAge_ = max<uint8_t>(12, static_cast<uint8_t>(app->profileEditAge_ - 1));
    app->refreshProfileScreen(millis());
  } else if (target == app->btnProfileAgePlus_) {
    const uint32_t now = millis();
    if (now - app->profileAdjustDebounceMs_ < 220) {
      return;
    }
    app->profileAdjustDebounceMs_ = now;
    app->profileEditAge_ = min<uint8_t>(90, static_cast<uint8_t>(app->profileEditAge_ + 1));
    app->refreshProfileScreen(millis());
  } else if (target == app->btnProfileWeightMinus_) {
    const uint32_t now = millis();
    if (now - app->profileAdjustDebounceMs_ < 220) {
      return;
    }
    app->profileAdjustDebounceMs_ = now;
    app->profileEditWeightKg_ = max(35.0f, app->profileEditWeightKg_ - 0.5f);
    app->refreshProfileScreen(millis());
  } else if (target == app->btnProfileWeightPlus_) {
    const uint32_t now = millis();
    if (now - app->profileAdjustDebounceMs_ < 220) {
      return;
    }
    app->profileAdjustDebounceMs_ = now;
    app->profileEditWeightKg_ = min(220.0f, app->profileEditWeightKg_ + 0.5f);
    app->refreshProfileScreen(millis());
  } else if (target == app->btnProfileHeightMinus_) {
    const uint32_t now = millis();
    if (now - app->profileAdjustDebounceMs_ < 220) {
      return;
    }
    app->profileAdjustDebounceMs_ = now;
    app->profileEditHeightCm_ = max(120.0f, app->profileEditHeightCm_ - 1.0f);
    app->refreshProfileScreen(millis());
  } else if (target == app->btnProfileHeightPlus_) {
    const uint32_t now = millis();
    if (now - app->profileAdjustDebounceMs_ < 220) {
      return;
    }
    app->profileAdjustDebounceMs_ = now;
    app->profileEditHeightCm_ = min(230.0f, app->profileEditHeightCm_ + 1.0f);
    app->refreshProfileScreen(millis());
  } else if (target == app->btnProfileGoalPrev_) {
    const uint32_t now = millis();
    if (now - app->profileAdjustDebounceMs_ < 220) {
      return;
    }
    app->profileAdjustDebounceMs_ = now;
    const bool allowTestGoal = app->isDebugGoalSelectionEnabled();
    app->profileEditGoal_ = app->cycleProfileGoal(app->profileEditGoal_, -1, allowTestGoal);
    app->refreshProfileScreen(millis());
  } else if (target == app->btnProfileGoalNext_) {
    const uint32_t now = millis();
    if (now - app->profileAdjustDebounceMs_ < 220) {
      return;
    }
    app->profileAdjustDebounceMs_ = now;
    const bool allowTestGoal = app->isDebugGoalSelectionEnabled();
    app->profileEditGoal_ = app->cycleProfileGoal(app->profileEditGoal_, 1, allowTestGoal);
    app->refreshProfileScreen(millis());
  } else if (target == app->btnProfileGenderPrev_) {
    const uint32_t now = millis();
    if (now - app->profileAdjustDebounceMs_ < 220) {
      return;
    }
    app->profileAdjustDebounceMs_ = now;
    int gender = static_cast<int>(app->profileEditGender_) - 1;
    if (gender < static_cast<int>(UserGender::Unspecified)) {
      gender = static_cast<int>(UserGender::NonBinary);
    }
    app->profileEditGender_ = static_cast<UserGender>(gender);
    app->refreshProfileScreen(millis());
  } else if (target == app->btnProfileGenderNext_) {
    const uint32_t now = millis();
    if (now - app->profileAdjustDebounceMs_ < 220) {
      return;
    }
    app->profileAdjustDebounceMs_ = now;
    int gender = static_cast<int>(app->profileEditGender_) + 1;
    if (gender > static_cast<int>(UserGender::NonBinary)) {
      gender = static_cast<int>(UserGender::Unspecified);
    }
    app->profileEditGender_ = static_cast<UserGender>(gender);
    app->refreshProfileScreen(millis());
  } else if (target == app->btnUser1_) {
    app->activateUserByUid("D6-FA-A5-05", true, true);
  } else if (target == app->btnUser2_) {
    app->activateUserByUid("7E-BA-1E-06", true, true);
  } else if (target == app->btnNewUser_) {
    app->activateUserByUid("SIM-NEW-01", true, true);
  } else if (target == app->btnAnonymous_) {
    app->activateAnonymousMode();
  } else if (target == app->btnRestSkip_) {
    if (app->isRestCountdownActive(millis(), nullptr)) {
      app->sessionRecorder_.endRest(millis());
      app->lastSetCompletedMs_ = 0;
      app->setStatusMessage("Rest skipped.");
      app->refreshUi();
    }
  } else if (target == app->btnLogLevel_) {
    switch (app->logLevel_) {
      case LogLevel::Quiet:
        app->logLevel_ = LogLevel::Normal;
        break;
      case LogLevel::Normal:
        app->logLevel_ = LogLevel::Verbose;
        break;
      case LogLevel::Verbose:
      default:
        app->logLevel_ = LogLevel::Quiet;
        break;
    }
    app->setStatusMessage(String("Log level: ") + app->logLevelText());
    app->refreshDebugPanel(millis());
  } else if (target == app->btnResetMotion_) {
    app->pulseTo(0.0f);
    app->repDetector_.reset();
    app->setStatusMessage("Motion reset.");
    app->refreshUi();
  } else if (target == app->btnJumpTop_) {
    app->sensorSimulationEnabled_ = true;
    app->pulseTo(100.0f);
  } else if (target == app->btnJumpBottom_) {
    app->sensorSimulationEnabled_ = true;
    app->pulseTo(0.0f);
  } else if (target == app->btnToggleSensorMode_) {
    if (app->hardwareSensorEnabled_) {
      app->sensorSimulationEnabled_ = !app->sensorSimulationEnabled_;
      app->setStatusMessage(app->sensorSimulationEnabled_
                                ? "Simulation input enabled."
                                : "Live sensor input enabled.");
    } else {
      app->sensorSimulationEnabled_ = true;
      app->setStatusMessage("Live sensor disabled in firmware constants. Using simulation.");
    }
    app->refreshUi();
  } else if (target == app->btnEncoderZero_) {
    app->captureEncoderZero();
  } else if (target == app->btnEncoderFull_) {
    app->captureEncoderFull();
  } else if (target == app->btnEncoderApply_) {
    app->applyEncoderCalibration();
  } else if (target == app->btnEncoderReset_) {
    app->resetEncoderCalibration();
  } else if (target == app->btnUserRomBottom_) {
    app->captureUserRomBottom();
  } else if (target == app->btnUserRomTop_) {
    app->captureUserRomTop();
  } else if (target == app->btnUserRomApply_) {
    app->applyUserRomCalibration();
  } else if (target == app->btnSyncCloud_) {
    app->syncCloudNow();
  }
}

void SmartGymTouchApp::onIdleOverlayEvent(lv_event_t* event) {
  auto* app = static_cast<SmartGymTouchApp*>(lv_event_get_user_data(event));
  if (app == nullptr) {
    return;
  }
  const uint32_t nowMs = millis();
  app->idleStartupLock_ = false;
  app->markUserActivity(nowMs);
  if (app->state_ == State::Summary) {
    if (app->summaryWaitingForSync_) {
      app->setStatusMessage("Saving session... please wait.");
      app->refreshUi();
      return;
    }
    if (app->sessionSummaryMinVisibleUntilMs_ != 0 &&
        nowMs < app->sessionSummaryMinVisibleUntilMs_) {
      return;
    }
    app->logoutActiveUser("Session ended.");
    return;
  }
  app->idleOverlaySuppressedUntilMs_ = nowMs + kIdleSuppressAfterActivityMs;
  app->idleShowCandidateMs_ = 0;
  app->idleHideCandidateMs_ = 0;
  app->uiCache_.lastIdleVisible = false;
  if (app->currentUiScreen_ == UiScreenMode::Idle) {
    app->showUiScreen(UiScreenMode::Main);
  }
  app->setStatusMessage("Screen touched. Scan RFID or move the machine.");
  app->refreshUi();
}

void SmartGymTouchApp::onSliderEvent(lv_event_t* event) {
  auto* app = static_cast<SmartGymTouchApp*>(lv_event_get_user_data(event));
  if (app == nullptr) {
    return;
  }
  app->markUserActivity(millis());

  app->sensorSimulationEnabled_ = true;
  app->autoMotionEnabled_ = false;
  app->autoMotionPhase_ = AutoMotionPhase::Idle;
  app->simulatedRomPercent_ = static_cast<float>(lv_slider_get_value(lv_event_get_target(event)));
  app->liveRomPercent_ = app->simulatedRomPercent_;
  app->refreshUi();
}

void SmartGymTouchApp::onMachineChanged(lv_event_t* event) {
  auto* app = static_cast<SmartGymTouchApp*>(lv_event_get_user_data(event));
  if (app == nullptr) {
    return;
  }
  app->markUserActivity(millis());

  const uint16_t index = lv_dropdown_get_selected(lv_event_get_target(event));
  if (index < kMachineCount) {
    app->setMachineById(kMachineIds[index]);
  }
  app->refreshUi();
}

void SmartGymTouchApp::onTick(lv_timer_t* timer) {
  auto* app = static_cast<SmartGymTouchApp*>(timer->user_data);
  if (app == nullptr) {
    return;
  }
  app->updateRuntime();
}
