#include "touch_manager.h"

#include <algorithm>

#include "app_config.h"
#include "core_logic.h"

TouchManager::TouchManager()
    : spi_(VSPI), touch_(config::kTouchCs, config::kTouchIrq) {}

bool TouchManager::begin() {
  spi_.begin(config::kTouchSclk, config::kTouchMiso, config::kTouchMosi,
             config::kTouchCs);
  available_ = touch_.begin(spi_);
  touch_.setRotation(0);
  Serial.printf("[touch] XPT2046 %s, fixed CYD mapping\n",
                available_ ? "ready" : "unavailable");
  return available_;
}

bool TouchManager::poll(TouchPoint& point) {
  TouchPoint raw;
  if (!pollRaw(raw)) return false;
  int32_t a = raw.x;
  int32_t b = raw.y;
  std::swap(a, b);
  point.x = core::mapTouch(a, 240, 3800, 320, false);
  point.y = core::mapTouch(b, 240, 3800, 240, true);
  point.pressure = raw.pressure;
  Serial.printf("[touch] x=%d y=%d z=%u\n", point.x, point.y, point.pressure);
  return true;
}

bool TouchManager::pollRaw(TouchPoint& point) {
  if (!available_) return false;
  const bool pressed = touch_.touched();
  if (!pressed) {
    wasPressed_ = false;
    trackingPress_ = false;
    return false;
  }
  if (wasPressed_ || static_cast<uint32_t>(millis() - lastEventAtMs_) < 180) {
    return false;
  }
  TS_Point raw = touch_.getPoint();
  if (raw.z < 450) {
    trackingPress_ = false;
    return false;
  }
  if (!trackingPress_) {
    trackingPress_ = true;
    pressStartedAtMs_ = millis();
    candidateX_ = raw.x;
    candidateY_ = raw.y;
    return false;
  }
  if (abs(raw.x - candidateX_) > 140 || abs(raw.y - candidateY_) > 140) {
    pressStartedAtMs_ = millis();
    candidateX_ = raw.x;
    candidateY_ = raw.y;
    return false;
  }
  if (static_cast<uint32_t>(millis() - pressStartedAtMs_) < 70) return false;
  wasPressed_ = true;
  trackingPress_ = false;
  lastEventAtMs_ = millis();
  point.x = raw.x;
  point.y = raw.y;
  point.pressure = raw.z;
  return true;
}

bool TouchManager::available() const { return available_; }
