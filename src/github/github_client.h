#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include "github_models.h"

class GitHubClient {
 public:
  enum class State { Idle, LoadingList, LoadingProfiles, Complete, Failed };

  void beginRefresh();
  void setListEtag(const String& etag);
  void setCachedProfiles(const FollowerProfiles* profiles, time_t updatedAt);
  void update();
  void reset();

  State state() const;
  bool busy() const;
  bool complete() const;
  bool failed() const;
  size_t loadedProfiles() const;
  size_t totalProfiles() const;
  const FollowerProfiles& profiles() const;
  FollowerProfiles takeProfiles();
  const String& error() const;
  int lastHttpStatus() const;
  int rateLimitRemaining() const;
  time_t rateLimitReset() const;
  bool firstPageLimited() const;
  bool partialDetails() const;
  bool notModified() const;
  const String& listEtag() const;

 private:
  bool fetchFollowerList();
  bool fetchProfile(size_t index);
  enum class HttpResult { Ok, NotModified, Error };
  HttpResult getJson(const String& url, JsonDocument& document,
                     JsonDocument& filter, const String& etag = "");
  void fail(const String& message);

  State state_ = State::Idle;
  FollowerProfiles profiles_;
  size_t profileIndex_ = 0;
  size_t page_ = 1;
  String error_;
  int lastHttpStatus_ = 0;
  int rateLimitRemaining_ = -1;
  time_t rateLimitReset_ = 0;
  bool firstPageLimited_ = false;
  bool partialDetails_ = false;
  bool listComplete_ = false;
  bool notModified_ = false;
  String listEtag_;
  const FollowerProfiles* cachedProfiles_ = nullptr;
  time_t cachedProfilesUpdatedAt_ = 0;
};
