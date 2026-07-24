#pragma once

#include <Arduino.h>

struct TouchCalibration {
  uint16_t minX = 240;
  uint16_t maxX = 3800;
  uint16_t minY = 240;
  uint16_t maxY = 3800;
  bool swapXY = true;
  bool invertX = false;
  bool invertY = true;
  bool valid = false;
};

class SettingsStore {
 public:
  void begin();
  uint8_t brightness() const;
  void setBrightness(uint8_t percent);
  TouchCalibration touchCalibration() const;
  void setTouchCalibration(const TouchCalibration& calibration);
  void clearTouchCalibration();

 private:
  uint8_t brightness_ = 80;
  TouchCalibration touch_;
};
