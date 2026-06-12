#pragma once

#include <Arduino.h>

// Servicio chico para sincronizar hora real por NTP y exponer timestamps
// legibles para Firebase. El firmware local sigue funcionando sin internet,
// pero cuando la hora existe los eventos ganan mucho mas contexto historico.
class TimeService {
 public:
  void begin(const String& timezoneRule);
  void update(bool wifiConnected);
  bool hasValidTime() const;
  uint32_t getEpoch() const;
  String getIso8601() const;

 private:
  String timezoneRule_ = "UTC0";
  uint32_t lastSyncAttemptMs_ = 0;
  bool ntpStarted_ = false;
};
