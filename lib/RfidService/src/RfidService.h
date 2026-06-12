#pragma once

#include <Arduino.h>
#include <MFRC522.h>
#include <SPI.h>

// Adaptador chico alrededor del RC522. Se encarga de la inicializacion SPI y
// de filtrar lecturas duplicadas muy cercanas para no disparar dos eventos por
// una sola pasada de tarjeta.
class RfidService {
 public:
  void begin(uint8_t ssPin, uint8_t rstPin, uint8_t sckPin, uint8_t misoPin,
             uint8_t mosiPin);
  bool readCard(String& uid);
  bool isReady() const { return initialized_; }
  uint8_t readerVersion() const { return readerVersion_; }

 private:
  static constexpr uint32_t kDuplicateReadWindowMs = 1500;
  static constexpr uint32_t kReinitRetryMs = 1500;

  SPIClass* spi_ = &SPI;
  MFRC522 reader_{0, 0};
  bool initialized_ = false;
  uint8_t readerVersion_ = 0;
  uint8_t ssPin_ = 0;
  uint8_t rstPin_ = 0;
  uint8_t sckPin_ = 0;
  uint8_t misoPin_ = 0;
  uint8_t mosiPin_ = 0;
  uint32_t lastInitAttemptMs_ = 0;
  String lastUid_;
  uint32_t lastReadMs_ = 0;

  void tryInit();
  String formatUid() const;
};
