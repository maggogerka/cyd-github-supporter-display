#include "application.h"

#include <time.h>

#include "app_config.h"

Application::Application()
    : retry_(config::kInitialRetryMs, config::kMaximumRetryMs) {}

void Application::begin() {
  Serial.begin(config::kSerialBaud);
  delay(50);
  Serial.println();
  Serial.println("[app] GitHub Supporter Display v0.1.0 starting");
  Serial.printf("[app] Initial free heap=%u, flash=%u\n", ESP.getFreeHeap(),
                ESP.getFlashChipSize());

  transition(State::DISPLAY_INITIALIZING);
  const bool dimensionsOk = display_.begin();
  display_.showBoot();
  if (!dimensionsOk) {
    Serial.println(
        "[display] WARNING: reported dimensions differ from 320x240; "
        "verify the board variant and rotation");
  }

  avatarCache_.begin();
  delay(900);
  display_.showProgress("Display OK", "Starting network");
  network_.begin();
  transition(State::WIFI_CONNECTING);
}

void Application::update() {
  network_.update();

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
        updateCarousel(false);
      }
      break;

    case State::OFFLINE_CACHE:
      updateCarousel(true);
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
  github_.reset();
  retryTitle_ = title;
  retryDetail_ = detail;
  const uint32_t delayMs = retry_.schedule(millis());
  Serial.printf("[app] Retry scheduled in %u ms: %s / %s\n", delayMs,
                title.c_str(), detail.c_str());

  if (!profiles_.empty()) {
    transition(State::OFFLINE_CACHE);
    showCurrentFollower(true);
  } else {
    transition(State::ERROR_RETRY);
    display_.showError(title, detail, delayMs / 1000);
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
}

void Application::startGitHubRefresh() {
  lastProgressCount_ = SIZE_MAX;
  github_.beginRefresh();
  if (profiles_.empty()) {
    display_.showProgress("Loading GitHub followers", "Reading first page");
  }
  transition(State::GITHUB_LOADING);
}

void Application::finishGitHubRefresh() {
  firstPageLimited_ = github_.firstPageLimited();
  const bool partialDetails = github_.partialDetails();
  FollowerProfiles refreshed = github_.takeProfiles();
  avatarCache_.prune(refreshed);
  profiles_ = std::move(refreshed);
  followerIndex_ =
      profiles_.empty() ? 0 : followerIndex_ % profiles_.size();
  lastUpdatedAt_ = time(nullptr);
  lastRefreshAtMs_ = millis();
  retry_.reset();

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

void Application::logState(State state) const {
  static constexpr const char* kNames[] = {
      "BOOT",          "DISPLAY_INITIALIZING", "WIFI_CONNECTING",
      "TIME_SYNCING",  "GITHUB_LOADING",       "CAROUSEL",
      "OFFLINE_CACHE", "ERROR_RETRY"};
  Serial.printf("[app] State -> %s\n", kNames[static_cast<size_t>(state)]);
}
