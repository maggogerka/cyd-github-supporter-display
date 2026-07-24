#include "network_manager.h"

#include <time.h>

#include "app_config.h"
#include "wifi_credentials.h"

namespace {
bool systemClockIsValid() {
  time_t now = time(nullptr);
  return now > 1700000000;
}
}  // namespace

void NetworkManager::begin() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  stateStartedAtMs_ = millis();

#if CYD_HAS_WIFI_SECRETS
  if (WIFI_SSID[0] != '\0') {
    Serial.printf("[network] Connecting to SSID '%s' (password hidden)\n",
                  WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    state_ = State::Connecting;
    return;
  }
#endif
  Serial.println("[network] include/secrets.h is missing or WIFI_SSID is empty");
  state_ = State::Failed;
}
void NetworkManager::update() {
  if (state_ == State::Connecting) {
    if (WiFi.status() == WL_CONNECTED) {
      state_ = State::Connected;
      stateStartedAtMs_ = millis();
      Serial.printf("[network] Connected, IP: %s, RSSI: %d dBm\n",
                    WiFi.localIP().toString().c_str(), WiFi.RSSI());
    } else if (millis() - stateStartedAtMs_ >
               config::kWifiConnectTimeoutMs) {
      Serial.printf("[network] Wi-Fi timeout, status=%d\n", WiFi.status());
      state_ = State::Failed;
    }
  } else if (state_ == State::TimeSyncing) {
    if (systemClockIsValid()) {
      state_ = State::Ready;
      Serial.printf("[network] NTP synchronized, epoch=%lld\n",
                    static_cast<long long>(time(nullptr)));
    } else if (millis() - stateStartedAtMs_ >
               config::kTimeSyncTimeoutMs) {
      Serial.println("[network] NTP synchronization timed out");
      state_ = State::Failed;
    }
  } else if ((state_ == State::Connected || state_ == State::Ready) &&
             WiFi.status() != WL_CONNECTED) {
    Serial.println("[network] Wi-Fi connection lost");
    state_ = State::Failed;
  }
}

void NetworkManager::startTimeSync() {
  if (!wifiConnected()) {
    state_ = State::Failed;
    return;
  }
  setenv("TZ", config::kTimezone, 1);
  tzset();
  configTime(0, 0, config::kNtpServer1, config::kNtpServer2);
  stateStartedAtMs_ = millis();
  state_ = State::TimeSyncing;
  Serial.println("[network] Synchronizing time via NTP");
}

void NetworkManager::reconnect() {
  WiFi.disconnect(false);
  begin();
}

NetworkManager::State NetworkManager::state() const { return state_; }

bool NetworkManager::wifiConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

bool NetworkManager::timeReady() const {
  return state_ == State::Ready && systemClockIsValid();
}

String NetworkManager::statusText() const {
  switch (state_) {
    case State::Idle:
      return "Wi-Fi idle";
    case State::Connecting:
      return "Connecting to Wi-Fi";
    case State::Connected:
      return "Wi-Fi connected";
    case State::TimeSyncing:
      return "Syncing time";
    case State::Ready:
      return "Network ready";
    case State::Failed:
      return "Network unavailable";
  }
  return "Unknown";
}

String NetworkManager::ipAddress() const {
  return wifiConnected() ? WiFi.localIP().toString() : String("-");
}
