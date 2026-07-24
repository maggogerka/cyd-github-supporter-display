#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>

#include "storage/settings_store.h"

struct TouchPoint {
  int16_t x = 0;
  int16_t y = 0;
  uint16_t pressure = 0;
};

class TouchManager {
 public:
  TouchManager();
  bool begin(const TouchCalibration& calibration);
  bool poll(TouchPoint& point);
  bool pollRaw(TouchPoint& point);
  void setCalibration(const TouchCalibration& calibration);
  bool available() const;

 private:
  SPIClass spi_;
  XPT2046_Touchscreen touch_;
  TouchCalibration calibration_;
  bool available_ = false;
  bool wasPressed_ = false;
  bool trackingPress_ = false;
  uint32_t pressStartedAtMs_ = 0;
  int16_t candidateX_ = 0;
  int16_t candidateY_ = 0;
  uint32_t lastEventAtMs_ = 0;
};
