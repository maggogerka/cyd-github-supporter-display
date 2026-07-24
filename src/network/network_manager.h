#pragma once

#include <Arduino.h>
#include <WiFi.h>

class NetworkManager {
 public:
  enum class State { Idle, Connecting, Connected, TimeSyncing, Ready, Failed };

  void begin();
  void update();
  void startTimeSync();
  void reconnect();

  State state() const;
  bool wifiConnected() const;
  bool timeReady() const;
  String statusText() const;
  String ipAddress() const;

 private:
  State state_ = State::Idle;
  uint32_t stateStartedAtMs_ = 0;
};
