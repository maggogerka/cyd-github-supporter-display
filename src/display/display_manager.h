#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

#include "github/github_models.h"
#include "storage/avatar_cache.h"

class DisplayManager {
 public:
  bool begin();

  void showBoot();
  void showProgress(const String& stage, const String& detail = "");
  void showError(const String& title, const String& detail,
                 uint32_t retryInSeconds);
  void showEmpty(bool connected, time_t updatedAt);
  void showFollower(const FollowerProfile& profile, size_t index, size_t total,
                    bool connected, bool offline, time_t updatedAt,
                    AvatarCache& cache);

 private:
  bool drawAvatar(const String& path, AvatarCache::Format format, int16_t x,
                  int16_t y);
  void drawHeader(bool connected, bool offline, size_t index, size_t total);
  void drawFooter(time_t updatedAt);
  String asciiSafe(const String& value, size_t maximumChars) const;
  String formatTime(time_t value) const;

  TFT_eSPI tft_;
  uint16_t background_ = 0;
  uint16_t surface_ = 0;
  uint16_t text_ = 0;
  uint16_t muted_ = 0;
  uint16_t blue_ = 0;
  uint16_t green_ = 0;
  uint16_t heart_ = 0;
};
