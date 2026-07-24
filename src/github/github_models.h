#pragma once

#include <Arduino.h>

#include <cstdint>
#include <vector>

struct FollowerProfile {
  uint64_t id = 0;
  String login;
  String name;
  String avatarUrl;
  String htmlUrl;
  String bio;
  uint32_t publicRepos = 0;

  String effectiveName() const { return name.isEmpty() ? login : name; }
  String shortUrl() const { return "github.com/" + login; }
};

using FollowerProfiles = std::vector<FollowerProfile>;
