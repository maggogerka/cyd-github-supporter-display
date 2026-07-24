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
}

uint8_t SettingsStore::brightness() const { return brightness_; }

void SettingsStore::setBrightness(uint8_t percent) {
  brightness_ = constrain(percent, 20, 100);
  preferences.putUChar("brightness", brightness_);
}
