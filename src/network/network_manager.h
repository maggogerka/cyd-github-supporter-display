#pragma once

#include <Arduino.h>
#include <WiFi.h>

class NetworkManager {
 public:
  NetworkManager();
  enum class State {
    Idle,
    Connecting,
    Provisioning,
    Connected,
    TimeSyncing,
    Ready,
    Failed
  };

  void prepare();
  void begin();
  void update();
  void startTimeSync();
  void reconnect();
  bool resetCredentials();
  bool hasSavedCredentials() const;

  State state() const;
  bool wifiConnected() const;
  bool timeReady() const;
  String statusText() const;
  String ipAddress() const;
  String ssid() const;
  const String& portalName() const;

 private:
  State state_ = State::Idle;
  uint32_t stateStartedAtMs_ = 0;
  String portalName_;
  bool configuredMarker_ = false;
  bool prepared_ = false;
};
