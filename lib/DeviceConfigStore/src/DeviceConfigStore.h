#pragma once

#include <Arduino.h>
#include <Preferences.h>

// NVS minima del dispositivo. Solo guarda identidad fisica del equipo; el
// historial y la mayoria del estado viven fuera de aqui.
class DeviceConfigStore {
 public:
  void begin();
  String loadMachineId() const;
  bool saveMachineId(const String& machineId);
  bool loadEncoderCalibration(bool& valid, uint32_t& zeroRaw, uint32_t& fullRaw,
                              float& referenceDistanceMm, bool& invertDirection) const;
  bool saveEncoderCalibration(bool valid, uint32_t zeroRaw, uint32_t fullRaw,
                              float referenceDistanceMm, bool invertDirection);

 private:
  static constexpr const char* kNamespace = "smartgym";
  static constexpr const char* kMachineIdKey = "machine_id";
  static constexpr const char* kEncoderValidKey = "enc_valid";
  static constexpr const char* kEncoderZeroKey = "enc_zero";
  static constexpr const char* kEncoderFullKey = "enc_full";
  static constexpr const char* kEncoderRefMmKey = "enc_ref_mm";
  static constexpr const char* kEncoderInvertKey = "enc_invert";
};
