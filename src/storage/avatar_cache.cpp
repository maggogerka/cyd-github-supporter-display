#include "avatar_cache.h"

#include <HTTPClient.h>
#include <LittleFS.h>
#include <WiFiClientSecure.h>

#include <algorithm>

#include "app_config.h"
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

String resizedAvatarUrl(const String& original) {
  if (original.isEmpty()) {
    return original;
  }
  return original + (original.indexOf('?') >= 0 ? "&s=96" : "?s=96");
}
}  // namespace

bool AvatarCache::begin() {
  available_ = LittleFS.begin(false);
  if (!available_) {
    Serial.println("[cache] LittleFS mount failed; attempting format");
    available_ = LittleFS.begin(true);
  }
  if (!available_) {
    setError("LittleFS unavailable");
    return false;
  }
  if (!LittleFS.exists("/avatars")) {
    LittleFS.mkdir("/avatars");
  }
  Serial.printf("[cache] LittleFS ready: used=%u total=%u\n",
                static_cast<unsigned>(LittleFS.usedBytes()),
                static_cast<unsigned>(LittleFS.totalBytes()));
  return true;
}

bool AvatarCache::available() const { return available_; }

bool AvatarCache::ensure(const FollowerProfile& profile) {
  if (!available_ || profile.id == 0 || profile.avatarUrl.isEmpty()) {
    return false;
  }
  const String path = pathFor(profile.id);
  if (formatOf(path) == Format::Jpeg || formatOf(path) == Format::Png) {
    return true;
  }

  if (LittleFS.exists(path)) {
    LittleFS.remove(path);
  }
  if (!download(profile, path)) {
    return false;
  }
  if (totalBytes() > config::kMaximumAvatarCacheBytes) {
    LittleFS.remove(path);
    setError("Avatar cache size limit reached");
    return false;
  }
  return true;
}

bool AvatarCache::download(const FollowerProfile& profile,
                           const String& destination) {
  const String url = resizedAvatarUrl(profile.avatarUrl);
  const String temporary = destination + ".tmp";
  LittleFS.remove(temporary);

  Serial.printf("[cache] Downloading @%s avatar, free heap=%u\n",
                profile.login.c_str(), ESP.getFreeHeap());
  WiFiClientSecure client;
  configureTls(client);
  HTTPClient http;
  http.setConnectTimeout(15000);
  http.setTimeout(20000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.useHTTP10(true);
  if (!http.begin(client, url)) {
    setError("Could not initialize avatar HTTPS request");
    return false;
  }
  const char* responseHeaders[] = {"Content-Type"};
  http.collectHeaders(responseHeaders, 1);
  const int status = http.GET();
  if (status != HTTP_CODE_OK) {
    setError(status < 0 ? "Avatar HTTPS/TLS error: " +
                              http.errorToString(status)
                        : "Avatar HTTP status " + String(status));
    http.end();
    return false;
  }

  const int contentLength = http.getSize();
  String contentType = http.header("Content-Type");
  const int separator = contentType.indexOf(';');
  if (separator >= 0) {
    contentType.remove(separator);
  }
  contentType.toLowerCase();
  if (contentLength > static_cast<int>(config::kMaximumAvatarBytes)) {
    setError("Avatar is larger than the configured download limit");
    http.end();
    return false;
  }

  File output = LittleFS.open(temporary, FILE_WRITE, true);
  if (!output) {
    setError("Could not create avatar cache file");
    http.end();
    return false;
  }

  WiFiClient* stream = http.getStreamPtr();
  uint8_t buffer[1024];
  size_t written = 0;
  uint32_t lastDataAt = millis();
  while ((http.connected() || stream->available()) &&
         (contentLength < 0 || written < static_cast<size_t>(contentLength))) {
    const size_t available = stream->available();
    if (available > 0) {
      const size_t requested = std::min(available, sizeof(buffer));
      const int count = stream->readBytes(buffer, requested);
      if (count <= 0 || written + static_cast<size_t>(count) >
                            config::kMaximumAvatarBytes) {
        setError("Avatar stream is invalid or too large");
        output.close();
        http.end();
        LittleFS.remove(temporary);
        return false;
      }
      if (output.write(buffer, count) != static_cast<size_t>(count)) {
        setError("LittleFS write failed");
        output.close();
        http.end();
        LittleFS.remove(temporary);
        return false;
      }
      written += count;
      lastDataAt = millis();
    } else {
      if (millis() - lastDataAt > 5000) {
        break;
      }
      delay(1);
    }
  }
  output.close();
  http.end();

  if (written == 0 ||
      (contentLength >= 0 && written != static_cast<size_t>(contentLength))) {
    setError("Avatar download was incomplete");
    LittleFS.remove(temporary);
    return false;
  }

  const Format format = formatOf(temporary);
  if (format != Format::Jpeg && format != Format::Png) {
    setError(format == Format::Gif ? "GIF avatars are not supported"
                                   : "Unknown avatar image format");
    LittleFS.remove(temporary);
    return false;
  }
  const bool contentTypeMatches =
      contentType.isEmpty() || contentType == "application/octet-stream" ||
      (format == Format::Jpeg &&
       (contentType == "image/jpeg" || contentType == "image/jpg")) ||
      (format == Format::Png && contentType == "image/png");
  if (!contentTypeMatches) {
    setError("Avatar Content-Type does not match its file signature");
    LittleFS.remove(temporary);
    return false;
  }
  LittleFS.remove(destination);
  if (!LittleFS.rename(temporary, destination)) {
    setError("Could not finalize avatar cache file");
    LittleFS.remove(temporary);
    return false;
  }

  Serial.printf("[cache] Stored %u bytes at %s, free heap=%u\n",
                static_cast<unsigned>(written), destination.c_str(),
                ESP.getFreeHeap());
  lastError_ = "";
  return true;
}

void AvatarCache::prune(const FollowerProfiles& currentProfiles) {
  if (!available_) {
    return;
  }
  File directory = LittleFS.open("/avatars");
  if (!directory || !directory.isDirectory()) {
    return;
  }
  File entry;
  while ((entry = directory.openNextFile())) {
    const String name = entry.name();
    const String fullPath =
        name.startsWith("/") ? name : String("/avatars/") + name;
    const bool temporary = fullPath.endsWith(".tmp");
    entry.close();
    if (temporary || !isCurrentAvatar(fullPath, currentProfiles)) {
      Serial.printf("[cache] Removing stale file %s\n", fullPath.c_str());
      if (!LittleFS.remove(fullPath)) {
        Serial.printf("[cache] WARNING: could not remove %s\n",
                      fullPath.c_str());
      }
    }
  }
  directory.close();
}

String AvatarCache::pathFor(uint64_t githubId) const {
  return "/avatars/" + String(static_cast<unsigned long long>(githubId)) +
         ".img";
}

AvatarCache::Format AvatarCache::formatOf(const String& path) const {
  if (!available_ || !LittleFS.exists(path)) {
    return Format::Missing;
  }
  File file = LittleFS.open(path, FILE_READ);
  if (!file) {
    return Format::Missing;
  }
  uint8_t signature[8] = {};
  const size_t count = file.read(signature, sizeof(signature));
  file.close();
  if (count >= 3 && signature[0] == 0xFF && signature[1] == 0xD8 &&
      signature[2] == 0xFF) {
    return Format::Jpeg;
  }
  static constexpr uint8_t kPngSignature[] = {0x89, 0x50, 0x4E, 0x47,
                                              0x0D, 0x0A, 0x1A, 0x0A};
  if (count == sizeof(kPngSignature) &&
      memcmp(signature, kPngSignature, sizeof(kPngSignature)) == 0) {
    return Format::Png;
  }
  if (count >= 6 && signature[0] == 'G' && signature[1] == 'I' &&
      signature[2] == 'F') {
    return Format::Gif;
  }
  return Format::Unknown;
}

size_t AvatarCache::totalBytes() const {
  if (!available_) {
    return 0;
  }
  size_t total = 0;
  File directory = LittleFS.open("/avatars");
  if (!directory || !directory.isDirectory()) {
    return 0;
  }
  File entry;
  while ((entry = directory.openNextFile())) {
    if (!entry.isDirectory()) {
      total += entry.size();
    }
    entry.close();
  }
  directory.close();
  return total;
}

size_t AvatarCache::fileCount() const {
  if (!available_) return 0;
  size_t count = 0;
  File directory = LittleFS.open("/avatars");
  File entry;
  while (directory && (entry = directory.openNextFile())) {
    if (!entry.isDirectory() && !String(entry.name()).endsWith(".tmp")) ++count;
    entry.close();
  }
  return count;
}

void AvatarCache::clear() {
  if (!available_) return;
  File directory = LittleFS.open("/avatars");
  File entry;
  while (directory && (entry = directory.openNextFile())) {
    String path = entry.name();
    if (!path.startsWith("/")) path = "/avatars/" + path;
    entry.close();
    LittleFS.remove(path);
  }
  Serial.println("[cache] Avatar cache cleared");
}

void AvatarCache::invalidate(uint64_t githubId) {
  const String path = pathFor(githubId);
  if (LittleFS.exists(path)) LittleFS.remove(path);
}

const String& AvatarCache::lastError() const { return lastError_; }

bool AvatarCache::isCurrentAvatar(
    const String& filename, const FollowerProfiles& profiles) const {
  const String fullPath =
      filename.startsWith("/") ? filename : String("/avatars/") + filename;
  for (const auto& profile : profiles) {
    if (fullPath == pathFor(profile.id)) {
      return true;
    }
  }
  return false;
}

void AvatarCache::setError(const String& error) {
  lastError_ = error;
  Serial.printf("[cache] ERROR: %s, free heap=%u\n", error.c_str(),
                ESP.getFreeHeap());
}
