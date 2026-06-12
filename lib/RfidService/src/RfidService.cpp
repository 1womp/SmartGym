#include "RfidService.h"

// Encapsula el lector RFID para que AppController solo reciba UIDs ya limpios.
void RfidService::begin(uint8_t ssPin, uint8_t rstPin, uint8_t sckPin, uint8_t misoPin,
                        uint8_t mosiPin) {
  ssPin_ = ssPin;
  rstPin_ = rstPin;
  sckPin_ = sckPin;
  misoPin_ = misoPin;
  mosiPin_ = mosiPin;
  tryInit();
}

void RfidService::tryInit() {
  lastInitAttemptMs_ = millis();
  // En algunas placas los pines SPI no son los default, por eso los fijamos
  // explicitamente en vez de asumir el bus por defecto del core.
  spi_->begin(sckPin_, misoPin_, mosiPin_, ssPin_);
  reader_ = MFRC522(ssPin_, rstPin_);
  reader_.PCD_Init();
  delay(4);
  readerVersion_ = reader_.PCD_ReadRegister(MFRC522::VersionReg);
  initialized_ = !(readerVersion_ == 0x00 || readerVersion_ == 0xFF);
}

bool RfidService::readCard(String& uid) {
  if (!initialized_) {
    const uint32_t nowMs = millis();
    if ((nowMs - lastInitAttemptMs_) >= kReinitRetryMs) {
      tryInit();
    }
    if (!initialized_) {
      return false;
    }
  }

  if (readerVersion_ == 0x00 || readerVersion_ == 0xFF) {
    initialized_ = false;
    return false;
  }

  if (!reader_.PICC_IsNewCardPresent()) {
    return false;
  }

  if (!reader_.PICC_ReadCardSerial()) {
    return false;
  }

  const String currentUid = formatUid();
  const uint32_t nowMs = millis();

  reader_.PICC_HaltA();
  reader_.PCD_StopCrypto1();

  if (currentUid == lastUid_ && (nowMs - lastReadMs_) < kDuplicateReadWindowMs) {
    return false;
  }

  lastUid_ = currentUid;
  lastReadMs_ = nowMs;
  uid = currentUid;
  return true;
}

String RfidService::formatUid() const {
  String uid;
  uid.reserve(reader_.uid.size * 3);

  // Formato canonico del proyecto: bytes hex en mayusculas separados por "-".
  // Eso simplifica comparar UIDs en consola, NVS y Firebase.
  for (byte i = 0; i < reader_.uid.size; i++) {
    if (i > 0) {
      uid += '-';
    }

    if (reader_.uid.uidByte[i] < 0x10) {
      uid += '0';
    }

    uid += String(reader_.uid.uidByte[i], HEX);
  }

  uid.toUpperCase();
  return uid;
}
