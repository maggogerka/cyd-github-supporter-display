#pragma once

#include <Arduino.h>

namespace config {

inline constexpr char kProjectName[] = "GitHub Supporter Display";
inline constexpr char kGitHubUser[] = "maggogerka";
inline constexpr char kGitHubApiHost[] = "api.github.com";
inline constexpr char kApiVersion[] = "2026-03-10";
inline constexpr char kUserAgent[] = "cyd-github-supporter-display/0.1.0";
inline constexpr char kNtpServer1[] = "pool.ntp.org";
inline constexpr char kNtpServer2[] = "time.google.com";
inline constexpr char kTimezone[] = "MSK-3";

inline constexpr uint32_t kSerialBaud = 115200;
inline constexpr uint8_t kBacklightPin = 21;
inline constexpr uint32_t kWifiConnectTimeoutMs = 30000;
inline constexpr uint32_t kTimeSyncTimeoutMs = 25000;
inline constexpr uint32_t kCarouselIntervalMs = 10000;
inline constexpr uint32_t kRefreshIntervalMs = 15UL * 60UL * 1000UL;
inline constexpr uint32_t kInitialRetryMs = 15000;
inline constexpr uint32_t kMaximumRetryMs = 15UL * 60UL * 1000UL;

inline constexpr size_t kMaximumFollowers = 100;
inline constexpr size_t kMaximumAvatarBytes = 96UL * 1024UL;
inline constexpr size_t kMaximumAvatarCacheBytes = 1400UL * 1024UL;
inline constexpr int kAvatarSize = 96;

}  // namespace config
