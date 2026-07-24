#include "github_client.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "app_config.h"
#include "core_logic.h"
#include "tls_certificates.h"

namespace {
void configureTls(WiFiClientSecure& client) {
#ifdef CYD_ALLOW_INSECURE_TLS
  client.setInsecure();
#else
  client.setCACert(GITHUB_ROOT_CA_BUNDLE);
#endif
  client.setHandshakeTimeout(15);
}

String apiUrl(const String& path) {
  return "https://" + String(config::kGitHubApiHost) + path;
}
}  // namespace

void GitHubClient::beginRefresh() {
  profiles_.clear();
  profiles_.reserve(config::kMaximumFollowers);
  profileIndex_ = 0;
  page_ = 1;
  error_ = "";
  lastHttpStatus_ = 0;
  firstPageLimited_ = false;
  partialDetails_ = false;
  listComplete_ = false;
  notModified_ = false;
  state_ = State::LoadingList;
  Serial.printf("[github] Refresh started, free heap=%u\n",
                ESP.getFreeHeap());
}

void GitHubClient::setListEtag(const String& etag) { listEtag_ = etag; }
void GitHubClient::setCachedProfiles(const FollowerProfiles* profiles,
                                    time_t updatedAt) {
  cachedProfiles_ = profiles;
  cachedProfilesUpdatedAt_ = updatedAt;
}

void GitHubClient::update() {
  if (state_ == State::LoadingList) {
    if (fetchFollowerList()) {
      if (notModified_) {
        state_ = State::Complete;
      } else if (listComplete_) {
        state_ =
            profiles_.empty() ? State::Complete : State::LoadingProfiles;
      }
    }
    return;
  }

  if (state_ == State::LoadingProfiles) {
    if (rateLimitRemaining_ >= 0 &&
        rateLimitRemaining_ <= config::kLowRateLimitThreshold) {
      partialDetails_ = profileIndex_ < profiles_.size();
      state_ = State::Complete;
      return;
    }
    if (!fetchProfile(profileIndex_)) {
      // The anonymous API allows only 60 requests/hour. Preserve the complete
      // follower list and its summary fields when an individual detail request
      // fails instead of discarding every card.
      if (!profiles_.empty()) {
        partialDetails_ = true;
        state_ = State::Complete;
        Serial.printf(
            "[github] Detail loading stopped at %u/%u; using list data for "
            "remaining cards: %s\n",
            static_cast<unsigned>(profileIndex_),
            static_cast<unsigned>(profiles_.size()), error_.c_str());
      }
      return;
    }
    ++profileIndex_;
    if (profileIndex_ >= profiles_.size()) {
      state_ = State::Complete;
      Serial.printf("[github] Loaded %u profiles, free heap=%u\n",
                    static_cast<unsigned>(profiles_.size()),
                    ESP.getFreeHeap());
    }
  }
}

void GitHubClient::reset() {
  state_ = State::Idle;
  profiles_.clear();
  profileIndex_ = 0;
  error_ = "";
}

bool GitHubClient::fetchFollowerList() {
  JsonDocument filter;
  filter[0]["id"] = true;
  filter[0]["login"] = true;
  filter[0]["avatar_url"] = true;
  filter[0]["html_url"] = true;

  JsonDocument document;
  const String url = apiUrl("/users/" + String(config::kGitHubUser) +
                            "/followers?per_page=100&page=" + String(page_));
  const HttpResult result =
      getJson(url, document, filter, page_ == 1 ? listEtag_ : "");
  if (result == HttpResult::NotModified) {
    notModified_ = true;
    listComplete_ = true;
    return true;
  }
  if (result != HttpResult::Ok) {
    return false;
  }
  if (!document.is<JsonArray>()) {
    fail("GitHub returned an invalid followers JSON document");
    return false;
  }

  JsonArray list = document.as<JsonArray>();
  const size_t accepted =
      core::limitedProfileCount(list.size(), config::kMaximumFollowers);
  const size_t pageItems = list.size();
  for (size_t i = 0; i < accepted; ++i) {
    JsonObject item = list[i];
    const char* login = item["login"] | "";
    const uint64_t id = item["id"] | 0ULL;
    if (login[0] == '\0' || id == 0) {
      continue;
    }
    FollowerProfile profile;
    profile.id = id;
    profile.login = login;
    profile.avatarUrl = item["avatar_url"] | "";
    profile.htmlUrl = item["html_url"] | "";
    if (cachedProfiles_ != nullptr &&
        time(nullptr) - cachedProfilesUpdatedAt_ <
            config::kProfileMaximumAgeSeconds) {
      for (const auto& cached : *cachedProfiles_) {
        if (cached.id != profile.id) continue;
        profile.name = cached.name;
        profile.bio = cached.bio;
        profile.publicRepos = cached.publicRepos;
        profile.avatarChanged =
            !cached.avatarUrl.isEmpty() &&
            cached.avatarUrl != profile.avatarUrl;
        profile.detailCached = true;
        break;
      }
    }
    profiles_.push_back(std::move(profile));
  }

  Serial.printf("[github] Followers page %u: %u entries, total=%u\n",
                static_cast<unsigned>(page_),
                static_cast<unsigned>(pageItems),
                static_cast<unsigned>(profiles_.size()));
  const size_t next = core::nextPage(page_, pageItems, 100, profiles_.size(),
                                     config::kMaximumFollowers);
  if (next == 0) {
    listComplete_ = true;
    firstPageLimited_ = profiles_.size() >= config::kMaximumFollowers;
  } else {
    page_ = next;
  }
  return true;
}

bool GitHubClient::fetchProfile(size_t index) {
  if (index >= profiles_.size()) {
    return true;
  }

  JsonDocument filter;
  filter["id"] = true;
  filter["login"] = true;
  filter["name"] = true;
  filter["avatar_url"] = true;
  filter["html_url"] = true;
  filter["bio"] = true;
  filter["public_repos"] = true;

  JsonDocument document;
  FollowerProfile& profile = profiles_[index];
  if (profile.detailCached) {
    Serial.printf("[github] Profile %u/%u cache hit: @%s\n",
                  static_cast<unsigned>(index + 1),
                  static_cast<unsigned>(profiles_.size()),
                  profile.login.c_str());
    return true;
  }
  if (getJson(apiUrl("/users/" + profile.login), document, filter) !=
      HttpResult::Ok) {
    return false;
  }
  if (!document.is<JsonObject>()) {
    fail("GitHub returned an invalid profile JSON document");
    return false;
  }

  profile.name = static_cast<const char*>(document["name"] | "");
  profile.avatarUrl =
      static_cast<const char*>(document["avatar_url"] | profile.avatarUrl.c_str());
  profile.htmlUrl =
      static_cast<const char*>(document["html_url"] | profile.htmlUrl.c_str());
  profile.bio = static_cast<const char*>(document["bio"] | "");
  profile.publicRepos = document["public_repos"] | 0U;

  profile.name =
      core::truncateUtf8(profile.effectiveName().c_str(), 48).c_str();
  profile.bio = core::optionalBio(profile.bio.c_str(), 110).c_str();
  Serial.printf("[github] Profile %u/%u: @%s, free heap=%u\n",
                static_cast<unsigned>(index + 1),
                static_cast<unsigned>(profiles_.size()),
                profile.login.c_str(), ESP.getFreeHeap());
  return true;
}

GitHubClient::HttpResult GitHubClient::getJson(
    const String& url, JsonDocument& document, JsonDocument& filter,
    const String& etag) {
  Serial.printf("[github] GET %s, free heap=%u\n", url.c_str(),
                ESP.getFreeHeap());
  WiFiClientSecure client;
  configureTls(client);

  HTTPClient http;
  http.setConnectTimeout(15000);
  http.setTimeout(20000);
  http.useHTTP10(true);
  if (!http.begin(client, url)) {
    fail("Could not initialize HTTPS request");
    return HttpResult::Error;
  }

  http.addHeader("Accept", "application/vnd.github+json");
  http.addHeader("User-Agent", config::kUserAgent);
  http.addHeader("X-GitHub-Api-Version", config::kApiVersion);
  if (!etag.isEmpty()) http.addHeader("If-None-Match", etag);
  const char* headerKeys[] = {"X-RateLimit-Remaining", "X-RateLimit-Reset",
                              "X-RateLimit-Limit", "ETag"};
  http.collectHeaders(headerKeys, 4);

  lastHttpStatus_ = http.GET();
  const String remainingHeader = http.header("X-RateLimit-Remaining");
  const String resetHeader = http.header("X-RateLimit-Reset");
  rateLimitRemaining_ =
      remainingHeader.isEmpty() ? -1 : remainingHeader.toInt();
  rateLimitReset_ =
      resetHeader.isEmpty() ? 0 : static_cast<time_t>(resetHeader.toInt());
  const String responseEtag = http.header("ETag");
  if (!responseEtag.isEmpty() && page_ == 1) listEtag_ = responseEtag;

  if (lastHttpStatus_ == HTTP_CODE_NOT_MODIFIED) {
    http.end();
    Serial.println("[github] HTTP 304, cached follower list is current");
    return HttpResult::NotModified;
  }

  if (lastHttpStatus_ != HTTP_CODE_OK) {
    String reason;
    if (lastHttpStatus_ == HTTP_CODE_FORBIDDEN) {
      reason = "GitHub API rate limit or access denied (HTTP 403)";
    } else if (lastHttpStatus_ == HTTP_CODE_NOT_FOUND) {
      reason = "GitHub resource not found (HTTP 404)";
    } else if (lastHttpStatus_ == 429) {
      reason = "GitHub API rate limit exceeded (HTTP 429)";
    } else if (lastHttpStatus_ < 0) {
      reason = "HTTPS/DNS/TLS error: " + http.errorToString(lastHttpStatus_);
    } else {
      reason = "Unexpected GitHub HTTP status " + String(lastHttpStatus_);
    }
    http.end();
    fail(reason);
    return HttpResult::Error;
  }

  const DeserializationError jsonError = deserializeJson(
      document, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (jsonError) {
    fail("Invalid or incomplete GitHub JSON: " + String(jsonError.c_str()));
    return HttpResult::Error;
  }
  return HttpResult::Ok;
}

void GitHubClient::fail(const String& message) {
  error_ = message;
  state_ = State::Failed;
  Serial.printf("[github] ERROR: %s; HTTP=%d; remaining=%d; free heap=%u\n",
                error_.c_str(), lastHttpStatus_, rateLimitRemaining_,
                ESP.getFreeHeap());
}

GitHubClient::State GitHubClient::state() const { return state_; }
bool GitHubClient::busy() const {
  return state_ == State::LoadingList || state_ == State::LoadingProfiles;
}
bool GitHubClient::complete() const { return state_ == State::Complete; }
bool GitHubClient::failed() const { return state_ == State::Failed; }
size_t GitHubClient::loadedProfiles() const { return profileIndex_; }
size_t GitHubClient::totalProfiles() const { return profiles_.size(); }
const FollowerProfiles& GitHubClient::profiles() const { return profiles_; }
FollowerProfiles GitHubClient::takeProfiles() {
  profileIndex_ = 0;
  state_ = State::Idle;
  return std::move(profiles_);
}
const String& GitHubClient::error() const { return error_; }
int GitHubClient::lastHttpStatus() const { return lastHttpStatus_; }
int GitHubClient::rateLimitRemaining() const { return rateLimitRemaining_; }
time_t GitHubClient::rateLimitReset() const { return rateLimitReset_; }
bool GitHubClient::firstPageLimited() const { return firstPageLimited_; }
bool GitHubClient::partialDetails() const { return partialDetails_; }
bool GitHubClient::notModified() const { return notModified_; }
const String& GitHubClient::listEtag() const { return listEtag_; }
