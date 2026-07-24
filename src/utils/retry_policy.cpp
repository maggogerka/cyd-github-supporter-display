#include "retry_policy.h"

#include "core_logic.h"

RetryPolicy::RetryPolicy(uint32_t initialDelayMs, uint32_t maximumDelayMs)
    : initialDelayMs_(initialDelayMs), maximumDelayMs_(maximumDelayMs) {}

void RetryPolicy::reset() {
  attempt_ = 0;
  nextAttemptAtMs_ = 0;
}

uint32_t RetryPolicy::schedule(uint32_t nowMs, uint32_t minimumDelayMs) {
  const uint32_t backoffMs =
      core::retryDelay(attempt_, initialDelayMs_, maximumDelayMs_);
  const uint32_t delayMs = max(backoffMs, minimumDelayMs);
  if (attempt_ < UINT8_MAX) {
    ++attempt_;
  }
  nextAttemptAtMs_ = nowMs + delayMs;
  return delayMs;
}

bool RetryPolicy::ready(uint32_t nowMs) const {
  return nextAttemptAtMs_ == 0 ||
         static_cast<int32_t>(nowMs - nextAttemptAtMs_) >= 0;
}

uint32_t RetryPolicy::nextAttemptAt() const { return nextAttemptAtMs_; }
