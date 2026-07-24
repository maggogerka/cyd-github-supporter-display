#include "application.h"

#include <time.h>
#include <LittleFS.h>

#include "app_config.h"
#include "core_logic.h"

Application::Application()
    : retry_(config::kInitialRetryMs, config::kMaximumRetryMs) {}

void Application::begin() {
  Serial.begin(config::kSerialBaud);
  delay(50);
  Serial.println();
  Serial.printf("[app] GitHub Supporter Display v%s starting, reset=%d\n",
                config::kFirmwareVersion, esp_reset_reason());
  Serial.printf("[app] Initial free heap=%u, min=%u, flash=%u\n",
                ESP.getFreeHeap(), ESP.getMinFreeHeap(),
                ESP.getFlashChipSize());

  transition(State::DISPLAY_INITIALIZING);
  const bool dimensionsOk = display_.begin();
  display_.showBoot();
  if (!dimensionsOk) {
    Serial.println(
        "[display] WARNING: reported dimensions differ from 320x240; "
        "verify the board variant and rotation");
  }

  settingsStore_.begin();
  display_.setBrightness(settingsStore_.brightness());
  touch_.begin();
  if (avatarCache_.begin()) {
    profileStore_.load(profiles_, lastUpdatedAt_, listEtag_);
  }
  if (!profiles_.empty()) {
    showCurrentFollower(true);
  }
  delay(900);
  network_.prepare();
  pinMode(0, INPUT_PULLUP);
  if (!network_.hasSavedCredentials() || digitalRead(0) == LOW) {
    display_.showProvisioning(network_.portalName());
  } else {
    display_.showProgress("Display OK", "Starting network");
  }
  network_.begin();
  if (network_.wifiConnected()) {
    transition(State::WIFI_CONNECTING);
  } else {
    enterRetry("Wi-Fi setup required",
               "Open the captive portal to configure a network");
  }
}

void Application::update() {
  network_.update();
  handleTouch();

  switch (state_) {
    case State::BOOT:
    case State::DISPLAY_INITIALIZING:
      break;

    case State::WIFI_CONNECTING:
      if (network_.state() == NetworkManager::State::Connected) {
        display_.showProgress("Syncing time", network_.ipAddress());
        network_.startTimeSync();
        transition(State::TIME_SYNCING);
      } else if (network_.state() == NetworkManager::State::Failed) {
        enterRetry("Wi-Fi unavailable",
                   "Check include/secrets.h and 2.4 GHz Wi-Fi");
      }
      break;

    case State::TIME_SYNCING:
      if (network_.timeReady()) {
        startGitHubRefresh();
      } else if (network_.state() == NetworkManager::State::Failed) {
        enterRetry("Time sync failed",
                   "NTP is required before secure HTTPS");
      }
      break;

    case State::GITHUB_LOADING:
      if (!network_.wifiConnected()) {
        enterRetry("Connection lost", "Keeping the last successful data");
        break;
      }
      github_.update();
      if (profiles_.empty() && github_.busy() &&
          lastProgressCount_ != github_.loadedProfiles()) {
        lastProgressCount_ = github_.loadedProfiles();
        const String detail =
            github_.totalProfiles() == 0
                ? "Reading followers list"
                : String(github_.loadedProfiles()) + " / " +
                      String(github_.totalProfiles()) + " profiles";
        display_.showProgress("Loading GitHub followers", detail);
      }
      if (github_.complete()) {
        finishGitHubRefresh();
      } else if (github_.failed()) {
        enterRetry("GitHub update failed", github_.error());
      }
      break;

    case State::CAROUSEL:
      if (!network_.wifiConnected()) {
        enterRetry("Wi-Fi lost", "Showing cached follower cards");
      } else if (millis() - lastRefreshAtMs_ >=
                 config::kRefreshIntervalMs) {
        startGitHubRefresh();
      } else {
        if (screen_ == Screen::Carousel) updateCarousel(false);
      }
      break;

    case State::OFFLINE_CACHE:
      if (screen_ == Screen::Carousel) updateCarousel(true);
      if (retry_.ready(millis())) {
        network_.reconnect();
        display_.showProgress("Connecting to Wi-Fi",
                              "Cached cards remain available");
        transition(State::WIFI_CONNECTING);
      }
      break;

    case State::ERROR_RETRY:
      if (millis() - lastRetryScreenAtMs_ >= 1000) {
        lastRetryScreenAtMs_ = millis();
        const int32_t remaining =
            static_cast<int32_t>(retry_.nextAttemptAt() - millis());
        display_.showError(retryTitle_, retryDetail_,
                           remaining > 0 ? remaining / 1000 + 1 : 0);
      }
      if (retry_.ready(millis())) {
        network_.reconnect();
        display_.showProgress("Connecting to Wi-Fi", "Retrying");
        transition(State::WIFI_CONNECTING);
      }
      break;
  }

  delay(2);
}

void Application::transition(State nextState) {
  state_ = nextState;
  stateStartedAtMs_ = millis();
  logState(state_);
}

void Application::enterRetry(const String& title, const String& detail) {
  retryTitle_ = title;
  retryDetail_ = detail;
  uint32_t minimumDelayMs = 0;
  if (github_.rateLimitRemaining() == 0) {
    const time_t now = time(nullptr);
    const time_t reset = github_.rateLimitReset();
    if (now > 0 && reset > now && reset - now <= 2 * 60 * 60) {
      minimumDelayMs =
          static_cast<uint32_t>(reset - now + 5) * 1000UL;
      retryDetail_ = "Rate limit reached; showing cached cards";
    }
  }
  github_.reset();
  const uint32_t delayMs = retry_.schedule(millis(), minimumDelayMs);
  Serial.printf("[app] Retry scheduled in %u ms: %s / %s\n", delayMs,
                retryTitle_.c_str(), retryDetail_.c_str());

  if (!profiles_.empty()) {
    transition(State::OFFLINE_CACHE);
    showCurrentFollower(true);
  } else {
    transition(State::ERROR_RETRY);
    display_.showError(retryTitle_, retryDetail_, delayMs / 1000);
    lastRetryScreenAtMs_ = millis();
  }
}

void Application::updateCarousel(bool offline) {
  if (profiles_.empty()) {
    if (millis() - lastCardChangedAtMs_ >= config::kCarouselIntervalMs) {
      display_.showEmpty(network_.wifiConnected(), lastUpdatedAt_);
      lastCardChangedAtMs_ = millis();
    }
    return;
  }
  if (millis() - lastCardChangedAtMs_ >= config::kCarouselIntervalMs) {
    followerIndex_ = (followerIndex_ + 1) % profiles_.size();
    showCurrentFollower(offline);
  }
}

void Application::showCurrentFollower(bool offline) {
  if (profiles_.empty()) {
    display_.showEmpty(network_.wifiConnected(), lastUpdatedAt_);
    lastCardChangedAtMs_ = millis();
    return;
  }

  followerIndex_ %= profiles_.size();
  const FollowerProfile& profile = profiles_[followerIndex_];
  if (!offline && network_.wifiConnected()) {
    avatarCache_.ensure(profile);
  }
  display_.showFollower(profile, followerIndex_, profiles_.size(),
                        network_.wifiConnected(), offline, lastUpdatedAt_,
                        avatarCache_);
  lastCardChangedAtMs_ = millis();
  screen_ = Screen::Carousel;
}

void Application::startGitHubRefresh() {
  lastProgressCount_ = SIZE_MAX;
  github_.beginRefresh();
  github_.setListEtag(listEtag_);
  github_.setCachedProfiles(&profiles_, lastUpdatedAt_);
  if (profiles_.empty()) {
    display_.showProgress("Loading GitHub followers", "Reading first page");
  }
  transition(State::GITHUB_LOADING);
}

void Application::finishGitHubRefresh() {
  if (github_.notModified()) {
    listEtag_ = github_.listEtag();
    lastUpdatedAt_ = time(nullptr);
    lastRefreshAtMs_ = millis();
    retry_.reset();
    transition(State::CAROUSEL);
    showCurrentFollower(false);
    return;
  }
  firstPageLimited_ = github_.firstPageLimited();
  const bool partialDetails = github_.partialDetails();
  FollowerProfiles refreshed = github_.takeProfiles();
  for (const auto& profile : refreshed) {
    if (profile.avatarChanged) avatarCache_.invalidate(profile.id);
  }
  avatarCache_.prune(refreshed);
  profiles_ = std::move(refreshed);
  followerIndex_ =
      profiles_.empty() ? 0 : followerIndex_ % profiles_.size();
  lastUpdatedAt_ = time(nullptr);
  lastRefreshAtMs_ = millis();
  retry_.reset();
  listEtag_ = github_.listEtag();
  profileStore_.save(profiles_, lastUpdatedAt_, listEtag_);

  Serial.printf(
      "[app] Refresh committed: %u profiles, first-page-limited=%s, "
      "partial-details=%s, cache=%u bytes, free heap=%u\n",
      static_cast<unsigned>(profiles_.size()),
      firstPageLimited_ ? "yes" : "no",
      partialDetails ? "yes" : "no",
      static_cast<unsigned>(avatarCache_.totalBytes()), ESP.getFreeHeap());
  transition(State::CAROUSEL);
  showCurrentFollower(false);
}

void Application::handleTouch() {
  TouchPoint point;
  if (!touch_.poll(point)) return;
  if (screen_ == Screen::Carousel) {
    if (point.x > 278 && point.y < 38) {
      showSettings();
    } else if (point.x < 55) {
      followerIndex_ = core::previousIndex(followerIndex_, profiles_.size());
      showCurrentFollower(state_ == State::OFFLINE_CACHE);
    } else if (point.x > 265) {
      followerIndex_ = core::nextIndex(followerIndex_, profiles_.size());
      showCurrentFollower(state_ == State::OFFLINE_CACHE);
    } else if (!profiles_.empty() && point.y > 35 && point.y < 175) {
      screen_ = Screen::Qr;
      display_.showQr(profiles_[followerIndex_]);
    } else if (point.y < 35) {
      screen_ = Screen::Statistics;
      display_.showStatistics(profiles_.size(), avatarCache_.fileCount(),
                              lastUpdatedAt_, WiFi.RSSI(),
                              github_.lastHttpStatus());
    }
    return;
  }
  if (screen_ == Screen::Qr || screen_ == Screen::Statistics) {
    showCurrentFollower(state_ == State::OFFLINE_CACHE);
    return;
  }
  if (screen_ == Screen::Brightness) {
    if (point.y > 80 && point.y < 165) {
      const uint8_t index = min<uint8_t>(point.x / 64, 4);
      const uint8_t value = config::kBrightnessLevels[index];
      settingsStore_.setBrightness(value);
      display_.setBrightness(value);
      display_.showBrightness(value);
    } else if (point.y > 185) {
      showSettings();
    }
    return;
  }
  if (screen_ == Screen::ConfirmResetWifi ||
      screen_ == Screen::ConfirmClearAvatars) {
    if (point.y >= 150 && point.y <= 210 && point.x > 160) {
      if (screen_ == Screen::ConfirmResetWifi) {
        network_.resetCredentials();
        delay(250);
        ESP.restart();
      } else {
        avatarCache_.clear();
        showSettings();
      }
    } else {
      showSettings();
    }
    return;
  }
  if (screen_ == Screen::Settings) {
    if (point.y > 205) {
      showCurrentFollower(state_ == State::OFFLINE_CACHE);
    } else if (point.y >= 101 && point.y < 139) {
      if (point.x < 160) {
        if (core::cooldownReady(millis(), lastManualRefreshAtMs_,
                                config::kManualRefreshCooldownMs) &&
            !github_.busy()) {
          lastManualRefreshAtMs_ = millis();
          startGitHubRefresh();
        }
      } else {
        screen_ = Screen::Brightness;
        display_.showBrightness(settingsStore_.brightness());
      }
    } else if (point.y < 177 && point.y >= 139) {
      if (point.x < 160) {
        screen_ = Screen::Statistics;
        display_.showStatistics(profiles_.size(), avatarCache_.fileCount(),
                                lastUpdatedAt_, WiFi.RSSI(),
                                github_.lastHttpStatus());
      } else if (!profiles_.empty()) {
        screen_ = Screen::Qr;
        display_.showQr(profiles_[followerIndex_]);
      }
    } else if (point.y >= 177) {
      if (point.x < 160) {
        screen_ = Screen::ConfirmClearAvatars;
        display_.showConfirmation("Clear avatars?",
                                  "Profiles and Wi-Fi will be kept");
      } else {
        screen_ = Screen::ConfirmResetWifi;
        display_.showConfirmation(
            "Reset Wi-Fi?",
            "Delete saved network and open setup mode?");
      }
    }
  }
}

void Application::showSettings() {
  screen_ = Screen::Settings;
  display_.showSettings(network_.ssid(), WiFi.RSSI(), network_.ipAddress(),
                        profiles_.size(), github_.rateLimitRemaining(),
                        LittleFS.usedBytes(), LittleFS.totalBytes(),
                        ESP.getFreeHeap(), settingsStore_.brightness());
}

void Application::logState(State state) const {
  static constexpr const char* kNames[] = {
      "BOOT",          "DISPLAY_INITIALIZING", "WIFI_CONNECTING",
      "TIME_SYNCING",  "GITHUB_LOADING",       "CAROUSEL",
      "OFFLINE_CACHE", "ERROR_RETRY"};
  Serial.printf("[app] State -> %s\n", kNames[static_cast<size_t>(state)]);
}
