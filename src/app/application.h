#pragma once

#include <Arduino.h>

#include "display/display_manager.h"
#include "github/github_client.h"
#include "network/network_manager.h"
#include "storage/avatar_cache.h"
#include "storage/profile_store.h"
#include "storage/settings_store.h"
#include "touch/touch_manager.h"
#include "utils/retry_policy.h"

class Application {
 public:
  enum class State {
    BOOT,
    DISPLAY_INITIALIZING,
    WIFI_CONNECTING,
    TIME_SYNCING,
    GITHUB_LOADING,
    CAROUSEL,
    OFFLINE_CACHE,
    ERROR_RETRY
  };
  enum class Screen { Carousel, Qr, Settings, Statistics, Brightness,
                      Calibration, ConfirmResetWifi, ConfirmClearAvatars };

  Application();

  void begin();
  void update();

 private:
  void transition(State nextState);
  void enterRetry(const String& title, const String& detail);
  void updateCarousel(bool offline);
  void showCurrentFollower(bool offline);
  void startGitHubRefresh();
  void finishGitHubRefresh();
  void logState(State state) const;
  void handleTouch();
  void showSettings();
  void beginCalibration();
  void maybeBeginInitialCalibration();
  void updateCalibration();

  State state_ = State::BOOT;
  DisplayManager display_;
  NetworkManager network_;
  GitHubClient github_;
  AvatarCache avatarCache_;
  ProfileStore profileStore_;
  SettingsStore settingsStore_;
  TouchManager touch_;
  RetryPolicy retry_;
  FollowerProfiles profiles_;

  size_t followerIndex_ = 0;
  size_t lastProgressCount_ = SIZE_MAX;
  uint32_t stateStartedAtMs_ = 0;
  uint32_t lastCardChangedAtMs_ = 0;
  uint32_t lastRefreshAtMs_ = 0;
  uint32_t lastRetryScreenAtMs_ = 0;
  time_t lastUpdatedAt_ = 0;
  String retryTitle_;
  String retryDetail_;
  bool firstPageLimited_ = false;
  String listEtag_;
  Screen screen_ = Screen::Carousel;
  uint32_t lastManualRefreshAtMs_ = 0;
  uint8_t calibrationStep_ = 0;
  TouchPoint calibrationRaw_[4];
  bool calibrationPending_ = false;
};
