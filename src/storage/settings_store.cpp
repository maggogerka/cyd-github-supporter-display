#include "settings_store.h"

#include <Preferences.h>

#include "app_config.h"

namespace {
Preferences preferences;
}

void SettingsStore::begin() {
  preferences.begin("cyd-support", false);
  brightness_ =
      preferences.getUChar("brightness", config::kDefaultBrightness);
  touch_.minX = preferences.getUShort("touchMinX", touch_.minX);
  touch_.maxX = preferences.getUShort("touchMaxX", touch_.maxX);
  touch_.minY = preferences.getUShort("touchMinY", touch_.minY);
  touch_.maxY = preferences.getUShort("touchMaxY", touch_.maxY);
  touch_.swapXY = preferences.getBool("touchSwap", touch_.swapXY);
  touch_.invertX = preferences.getBool("touchInvX", touch_.invertX);
  touch_.invertY = preferences.getBool("touchInvY", touch_.invertY);
  const uint8_t storedVersion = preferences.getUChar("touchVer", 0);
  touch_.valid =
      preferences.getBool("touchValid", false) &&
      storedVersion == config::kTouchCalibrationVersion;
}

uint8_t SettingsStore::brightness() const { return brightness_; }

void SettingsStore::setBrightness(uint8_t percent) {
  brightness_ = constrain(percent, 20, 100);
  preferences.putUChar("brightness", brightness_);
}

TouchCalibration SettingsStore::touchCalibration() const { return touch_; }

void SettingsStore::setTouchCalibration(
    const TouchCalibration& calibration) {
  touch_ = calibration;
  touch_.valid = true;
  preferences.putUShort("touchMinX", touch_.minX);
  preferences.putUShort("touchMaxX", touch_.maxX);
  preferences.putUShort("touchMinY", touch_.minY);
  preferences.putUShort("touchMaxY", touch_.maxY);
  preferences.putBool("touchSwap", touch_.swapXY);
  preferences.putBool("touchInvX", touch_.invertX);
  preferences.putBool("touchInvY", touch_.invertY);
  preferences.putUChar("touchVer", config::kTouchCalibrationVersion);
  preferences.putBool("touchValid", true);
}

void SettingsStore::clearTouchCalibration() {
  touch_.valid = false;
  preferences.putBool("touchValid", false);
}
