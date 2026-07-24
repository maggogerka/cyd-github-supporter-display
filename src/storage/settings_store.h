#pragma once

#include <Arduino.h>

class SettingsStore {
 public:
  void begin();
  uint8_t brightness() const;
  void setBrightness(uint8_t percent);

 private:
  uint8_t brightness_ = 80;
};
