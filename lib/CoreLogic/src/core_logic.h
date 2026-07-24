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
int16_t mapTouch(int32_t raw, int32_t minimum, int32_t maximum,
                 int16_t extent, bool inverted);
size_t previousIndex(size_t current, size_t count);
size_t nextIndex(size_t current, size_t count);
bool cooldownReady(uint32_t now, uint32_t last, uint32_t cooldown);
size_t nextPage(size_t currentPage, size_t pageItems, size_t perPage,
                size_t totalItems, size_t maximumItems);

}  // namespace core
