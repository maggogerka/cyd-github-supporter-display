#pragma once

#include <Arduino.h>
#include <FS.h>

#include "github/github_models.h"

class AvatarCache {
 public:
  enum class Format { Missing, Jpeg, Png, Gif, Unknown };

  bool begin();
  bool available() const;
  bool ensure(const FollowerProfile& profile);
  void prune(const FollowerProfiles& currentProfiles);

  String pathFor(uint64_t githubId) const;
  Format formatOf(const String& path) const;
  size_t totalBytes() const;
  const String& lastError() const;

 private:
  bool download(const FollowerProfile& profile, const String& destination);
  bool isCurrentAvatar(const String& filename,
                       const FollowerProfiles& profiles) const;
  void setError(const String& error);

  bool available_ = false;
  String lastError_;
};
