#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace core {

std::string truncateUtf8(const std::string& text, size_t maximumBytes);
std::string shortGitHubUrl(const std::string& login);
std::string displayName(const std::string& name, const std::string& login);
std::string optionalBio(const std::string& bio, size_t maximumBytes);
size_t limitedProfileCount(size_t received, size_t maximum);
uint32_t retryDelay(uint8_t attempt, uint32_t initialMs, uint32_t maximumMs);

}  // namespace core
