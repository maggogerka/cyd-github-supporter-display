#pragma once

#include <Arduino.h>

#include "github/github_models.h"

class ProfileStore {
 public:
  bool load(FollowerProfiles& profiles, time_t& updatedAt);
  bool save(const FollowerProfiles& profiles, time_t updatedAt);

 private:
  static constexpr const char* kPath = "/profiles.json";
  static constexpr const char* kTemporaryPath = "/profiles.tmp";
};
