#include "TimeService.h"

#include <time.h>

namespace {
// Si el epoch es menor a este umbral asumimos que el reloj aun no se
// sincronizo y no conviene etiquetar sesiones con una fecha basura.
constexpr uint32_t kMinValidEpoch = 1700000000UL;
constexpr uint32_t kSyncRetryMs = 60000UL;
}

void TimeService::begin(const String& timezoneRule) {
  timezoneRule_ = timezoneRule.isEmpty() ? "UTC0" : timezoneRule;
}

void TimeService::update(bool wifiConnected) {
  if (!wifiConnected) {
    return;
  }

  if (hasValidTime()) {
    return;
  }

  const uint32_t nowMs = millis();
  if (lastSyncAttemptMs_ != 0 && nowMs - lastSyncAttemptMs_ < kSyncRetryMs) {
    return;
  }

  lastSyncAttemptMs_ = nowMs;
  // configTzTime deja que el core haga la sincronizacion en background; luego
  // hasValidTime() solo valida cuando la fecha ya cruzo un umbral razonable.
  configTzTime(timezoneRule_.c_str(), "pool.ntp.org", "time.nist.gov");
  ntpStarted_ = true;
}

bool TimeService::hasValidTime() const {
  if (!ntpStarted_) {
    return false;
  }

  const time_t now = time(nullptr);
  return now >= static_cast<time_t>(kMinValidEpoch);
}

uint32_t TimeService::getEpoch() const {
  if (!hasValidTime()) {
    return 0;
  }

  return static_cast<uint32_t>(time(nullptr));
}

String TimeService::getIso8601() const {
  if (!hasValidTime()) {
    return "";
  }

  time_t now = time(nullptr);
  struct tm localTime {};
  localtime_r(&now, &localTime);
  char buffer[32] = {0};
  strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S%z", &localTime);
  return String(buffer);
}
