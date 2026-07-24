#include "profile_store.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

#include "app_config.h"

bool ProfileStore::load(FollowerProfiles& profiles, time_t& updatedAt,
                        String& etag) {
  if (!LittleFS.exists(kPath)) {
    return false;
  }

  File input = LittleFS.open(kPath, FILE_READ);
  if (!input) {
    return false;
  }
  JsonDocument document;
  const DeserializationError error = deserializeJson(document, input);
  input.close();
  if (error || !document["profiles"].is<JsonArray>()) {
    Serial.printf("[cache] Stored profile list is invalid: %s\n",
                  error ? error.c_str() : "profiles array missing");
    return false;
  }
  const uint32_t schema = document["schema"] | 0U;
  if (schema != 0 && schema != config::kCacheSchemaVersion) {
    Serial.println("[cache] Profile cache schema mismatch");
    return false;
  }

  FollowerProfiles loaded;
  JsonArray array = document["profiles"].as<JsonArray>();
  loaded.reserve(min(array.size(), config::kMaximumFollowers));
  for (JsonObject item : array) {
    FollowerProfile profile;
    profile.id = item["id"] | 0ULL;
    profile.login = static_cast<const char*>(item["login"] | "");
    if (profile.id == 0 || profile.login.isEmpty()) {
      continue;
    }
    profile.name = static_cast<const char*>(item["name"] | "");
    profile.avatarUrl = static_cast<const char*>(item["avatar_url"] | "");
    profile.htmlUrl = static_cast<const char*>(item["html_url"] | "");
    profile.bio = static_cast<const char*>(item["bio"] | "");
    profile.publicRepos = item["public_repos"] | 0U;
    loaded.push_back(std::move(profile));
    if (loaded.size() >= config::kMaximumFollowers) {
      break;
    }
  }
  if (loaded.empty()) {
    return false;
  }

  updatedAt = static_cast<time_t>(document["updated_at"] | 0LL);
  etag = static_cast<const char*>(document["etag"] | "");
  profiles = std::move(loaded);
  Serial.printf("[cache] Restored %u follower profiles from LittleFS\n",
                static_cast<unsigned>(profiles.size()));
  return true;
}

bool ProfileStore::save(const FollowerProfiles& profiles, time_t updatedAt,
                        const String& etag) {
  JsonDocument document;
  document["schema"] = config::kCacheSchemaVersion;
  document["updated_at"] = static_cast<int64_t>(updatedAt);
  document["etag"] = etag;
  JsonArray array = document["profiles"].to<JsonArray>();
  for (const FollowerProfile& profile : profiles) {
    JsonObject item = array.add<JsonObject>();
    item["id"] = profile.id;
    item["login"] = profile.login;
    item["name"] = profile.name;
    item["avatar_url"] = profile.avatarUrl;
    item["html_url"] = profile.htmlUrl;
    item["bio"] = profile.bio;
    item["public_repos"] = profile.publicRepos;
  }

  if (LittleFS.exists(kTemporaryPath)) LittleFS.remove(kTemporaryPath);
  File output = LittleFS.open(kTemporaryPath, FILE_WRITE, true);
  if (!output) {
    Serial.println("[cache] WARNING: could not create profile cache");
    return false;
  }
  const size_t written = serializeJson(document, output);
  output.close();
  if (written == 0) {
    LittleFS.remove(kTemporaryPath);
    return false;
  }
  LittleFS.remove(kPath);
  if (!LittleFS.rename(kTemporaryPath, kPath)) {
    LittleFS.remove(kTemporaryPath);
    return false;
  }
  Serial.printf("[cache] Stored %u follower profiles\n",
                static_cast<unsigned>(profiles.size()));
  return true;
}
