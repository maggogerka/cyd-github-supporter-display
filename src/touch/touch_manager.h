#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>

struct TouchPoint {
  int16_t x = 0;
  int16_t y = 0;
  uint16_t pressure = 0;
};

class TouchManager {
 public:
  TouchManager();
  bool begin();
  bool poll(TouchPoint& point);
  bool available() const;

 private:
  bool pollRaw(TouchPoint& point);
  SPIClass spi_;
  XPT2046_Touchscreen touch_;
  bool available_ = false;
  bool wasPressed_ = false;
  bool trackingPress_ = false;
  uint32_t pressStartedAtMs_ = 0;
  int16_t candidateX_ = 0;
  int16_t candidateY_ = 0;
  uint32_t lastEventAtMs_ = 0;
};
