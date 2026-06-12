#include "DeviceConfigStore.h"

void DeviceConfigStore::begin() {
  // Abrimos una vez en modo escritura para crear el namespace sin provocar el
  // warning de NOT_FOUND cuando el dispositivo arranca por primera vez.
  Preferences preferences;
  if (preferences.begin(kNamespace, false)) {
    preferences.end();
  }
}

String DeviceConfigStore::loadMachineId() const {
  Preferences preferences;
  if (!preferences.begin(kNamespace, true)) {
    return "";
  }

  const String machineId = preferences.isKey(kMachineIdKey)
                               ? preferences.getString(kMachineIdKey, "")
                               : String("");
  preferences.end();
  return machineId;
}

bool DeviceConfigStore::saveMachineId(const String& machineId) {
  Preferences preferences;
  if (!preferences.begin(kNamespace, false)) {
    return false;
  }

  const bool ok = preferences.putString(kMachineIdKey, machineId) > 0;
  preferences.end();
  return ok;
}

bool DeviceConfigStore::loadEncoderCalibration(bool& valid, uint32_t& zeroRaw, uint32_t& fullRaw,
                                               float& referenceDistanceMm, bool& invertDirection) const {
  Preferences preferences;
  if (!preferences.begin(kNamespace, true)) {
    return false;
  }

  valid = preferences.isKey(kEncoderValidKey) ? preferences.getBool(kEncoderValidKey, false) : false;
  zeroRaw = preferences.isKey(kEncoderZeroKey) ? preferences.getUInt(kEncoderZeroKey, 0) : 0;
  fullRaw = preferences.isKey(kEncoderFullKey) ? preferences.getUInt(kEncoderFullKey, 0) : 0;
  referenceDistanceMm = preferences.isKey(kEncoderRefMmKey)
                            ? preferences.getFloat(kEncoderRefMmKey, 1000.0f)
                            : 1000.0f;
  invertDirection = preferences.isKey(kEncoderInvertKey)
                        ? preferences.getBool(kEncoderInvertKey, false)
                        : false;
  preferences.end();
  return true;
}

bool DeviceConfigStore::saveEncoderCalibration(bool valid, uint32_t zeroRaw, uint32_t fullRaw,
                                               float referenceDistanceMm, bool invertDirection) {
  Preferences preferences;
  if (!preferences.begin(kNamespace, false)) {
    return false;
  }

  const bool ok = preferences.putBool(kEncoderValidKey, valid) > 0 &&
                  preferences.putUInt(kEncoderZeroKey, zeroRaw) > 0 &&
                  preferences.putUInt(kEncoderFullKey, fullRaw) > 0 &&
                  preferences.putFloat(kEncoderRefMmKey, referenceDistanceMm) > 0 &&
                  preferences.putBool(kEncoderInvertKey, invertDirection) > 0;
  preferences.end();
  return ok;
}
