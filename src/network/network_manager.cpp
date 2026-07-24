#include "network_manager.h"

#include <time.h>
#include <WiFiManager.h>
#include <Preferences.h>

#include "app_config.h"
#include "wifi_credentials.h"

namespace {
bool systemClockIsValid() {
  time_t now = time(nullptr);
  return now > 1700000000;
}
Preferences networkPreferences;
}  // namespace

NetworkManager::NetworkManager() {
}

void NetworkManager::prepare() {
  if (prepared_) return;
  const uint32_t chip = static_cast<uint32_t>(ESP.getEfuseMac());
  char suffix[5];
  snprintf(suffix, sizeof(suffix), "%04X", chip & 0xFFFF);
  portalName_ = "CYD-GitHub-" + String(suffix);
  networkPreferences.begin("cyd-network", false);
  configuredMarker_ = networkPreferences.getBool("configured", false);
  prepared_ = true;
}

void NetworkManager::begin() {
  prepare();
  pinMode(0, INPUT_PULLUP);
  const bool forcePortal = digitalRead(0) == LOW;
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  stateStartedAtMs_ = millis();

#if CYD_HAS_WIFI_SECRETS
  if (!forcePortal && !hasSavedCredentials() && WIFI_SSID[0] != '\0') {
    Serial.println("[network] Migrating local Wi-Fi credentials to NVS");
    Serial.printf("[network] Connecting to SSID '%s' (password hidden)\n",
                  WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    const uint32_t started = millis();
    while (WiFi.status() != WL_CONNECTED &&
           static_cast<uint32_t>(millis() - started) < 15000) {
      delay(50);
    }
  }
#endif
  if (!forcePortal && WiFi.status() == WL_CONNECTED) {
    configuredMarker_ = true;
    networkPreferences.putBool("configured", true);
    state_ = State::Connected;
    return;
  }

  WiFiManager manager;
  manager.setDebugOutput(false);
  manager.setClass("invert");
  manager.setConnectTimeout(config::kWifiConnectTimeoutMs / 1000);
  manager.setConfigPortalTimeout(240);
  if (!hasSavedCredentials()) {
    state_ = State::Provisioning;
    Serial.printf("[network] Starting captive portal '%s' at 192.168.4.1\n",
                  portalName_.c_str());
  } else {
    state_ = State::Connecting;
  }
  const bool connected =
      forcePortal ? manager.startConfigPortal(portalName_.c_str())
                  : manager.autoConnect(portalName_.c_str());
  if (connected) {
    configuredMarker_ = true;
    networkPreferences.putBool("configured", true);
    state_ = State::Connected;
    Serial.printf("[network] Connected, SSID='%s', IP=%s, RSSI=%d dBm\n",
                  WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(),
                  WiFi.RSSI());
  } else {
    Serial.println("[network] Captive portal/connection timed out");
    state_ = State::Failed;
  }
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

bool NetworkManager::resetCredentials() {
  prepare();
  WiFiManager manager;
  manager.resetSettings();
  configuredMarker_ = false;
  networkPreferences.putBool("configured", false);
  Serial.println("[network] Stored Wi-Fi credentials cleared");
  return true;
}

bool NetworkManager::hasSavedCredentials() const {
  return configuredMarker_;
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
    case State::Provisioning:
      return "Wi-Fi setup portal";
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

String NetworkManager::ssid() const {
  return wifiConnected() ? WiFi.SSID() : String("-");
}

const String& NetworkManager::portalName() const { return portalName_; }
