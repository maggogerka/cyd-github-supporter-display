#include "core_logic.h"

#include <algorithm>
#include <limits>

namespace core {

std::string truncateUtf8(const std::string& text, size_t maximumBytes) {
  if (text.size() <= maximumBytes) {
    return text;
  }
  if (maximumBytes <= 3) {
    return text.substr(0, maximumBytes);
  }

  size_t end = maximumBytes - 3;
  while (end > 0 &&
         (static_cast<unsigned char>(text[end]) & 0xC0U) == 0x80U) {
    --end;
  }
  return text.substr(0, end) + "...";
}

std::string shortGitHubUrl(const std::string& login) {
  return login.empty() ? "github.com" : "github.com/" + login;
}

std::string displayName(const std::string& name, const std::string& login) {
  return name.empty() ? login : name;
}

std::string optionalBio(const std::string& bio, size_t maximumBytes) {
  return bio.empty() ? std::string{} : truncateUtf8(bio, maximumBytes);
}

size_t limitedProfileCount(size_t received, size_t maximum) {
  return std::min(received, maximum);
}

uint32_t retryDelay(uint8_t attempt, uint32_t initialMs,
                    uint32_t maximumMs) {
  if (initialMs >= maximumMs) {
    return maximumMs;
  }
  const uint8_t safeAttempt = std::min<uint8_t>(attempt, 31);
  const uint64_t scaled = static_cast<uint64_t>(initialMs) << safeAttempt;
  return static_cast<uint32_t>(
      std::min<uint64_t>(scaled, static_cast<uint64_t>(maximumMs)));
}

}  // namespace core
