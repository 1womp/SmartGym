#include <Arduino.h>
#include <esp_display_panel.hpp>
#include <lvgl.h>
#include "SmartGymTouchApp.h"
#include "lvgl_v8_port.h"

using namespace esp_panel::drivers;
using namespace esp_panel::board;

namespace {
SmartGymTouchApp app;
constexpr uint16_t kRgbBounceBufferLines = 20;
constexpr int kBacklightBrightnessPercent = 100;
}

void setup() {
  Serial.begin(115200);
  delay(200);

  Board* board = new Board();
  board->init();
#if LVGL_PORT_AVOID_TEARING_MODE
  auto lcd = board->getLCD();
  lcd->configFrameBufferNumber(LVGL_PORT_DISP_BUFFER_NUM);
#if ESP_PANEL_DRIVERS_BUS_ENABLE_RGB && CONFIG_IDF_TARGET_ESP32S3
  auto lcd_bus = lcd->getBus();
  if (lcd_bus->getBasicAttributes().type == ESP_PANEL_BUS_TYPE_RGB) {
    // ESP32-S3 RGB panels can visibly drift/tear when PSRAM is busy with
    // WiFi/TLS. Viewe/Espressif recommend width*10 by default and width*20
    // when drift persists.
    static_cast<BusRGB*>(lcd_bus)->configRGB_BounceBufferSize(
        lcd->getFrameWidth() * kRgbBounceBufferLines);
  }
#endif
#endif
  const bool boardReady = board->begin();
  assert(boardReady);

  if (auto* backlight = board->getBacklight(); backlight != nullptr) {
    backlight->setBrightness(kBacklightBrightnessPercent);
  }

  lvgl_port_init(board->getLCD(), board->getTouch());

  lvgl_port_lock(-1);
  app.begin();
  lvgl_port_unlock();
}

void loop() {
  delay(1);
}
