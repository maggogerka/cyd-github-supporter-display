#pragma once

#include <Arduino.h>

class RetryPolicy {
 public:
  RetryPolicy(uint32_t initialDelayMs, uint32_t maximumDelayMs);

  void reset();
  uint32_t schedule(uint32_t nowMs, uint32_t minimumDelayMs = 0);
  bool ready(uint32_t nowMs) const;
  uint32_t nextAttemptAt() const;

 private:
  uint32_t initialDelayMs_;
  uint32_t maximumDelayMs_;
  uint32_t nextAttemptAtMs_ = 0;
  uint8_t attempt_ = 0;
};
