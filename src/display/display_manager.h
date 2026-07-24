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
  void showProvisioning(const String& accessPoint);
  void showSettings(const String& ssid, int rssi, const String& ip,
                    size_t followers, int rateRemaining, size_t fsUsed,
                    size_t fsTotal, uint32_t heap, uint8_t brightness);
  void showStatistics(size_t followers, size_t avatars, time_t updatedAt,
                      int rssi, int httpStatus);
  void showQr(const FollowerProfile& profile);
  void showBrightness(uint8_t selected);
  void showCalibration(uint8_t step);
  void showConfirmation(const String& title, const String& detail);
  void setBrightness(uint8_t percent);

 private:
  bool drawAvatar(const String& path, AvatarCache::Format format, int16_t x,
                  int16_t y);
  void drawHeader(bool connected, bool offline, size_t index, size_t total);
  void drawFooter(time_t updatedAt);
  void drawNavButtons();
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
