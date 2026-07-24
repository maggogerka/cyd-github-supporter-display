#include "display_manager.h"

#include <JPEGDEC.h>
#include <LittleFS.h>
#ifdef INTELSHORT
#undef INTELSHORT
#undef INTELLONG
#undef MOTOSHORT
#undef MOTOLONG
#endif
#include <PNGdec.h>
#include <qrcode.h>
#include <U8g2_for_TFT_eSPI.h>
#include <time.h>

#include "app_config.h"
#include "cyd_display_config.h"
#include "icons.h"

namespace {
TFT_eSPI* gTft = nullptr;
int16_t gImageX = 0;
int16_t gImageY = 0;
PNG gPng;
JPEGDEC gJpeg;
File gPngFile;
File gJpegFile;
uint16_t gPngLine[320];
uint8_t gQrData[512];
U8g2_for_TFT_eSPI gUtf8;

int drawJpegBlock(JPEGDRAW* block) {
  if (gTft == nullptr || block->x >= config::kAvatarSize ||
      block->y >= config::kAvatarSize) {
    return 1;
  }
  const int width =
      min(block->iWidth, config::kAvatarSize - static_cast<int>(block->x));
  const int height =
      min(block->iHeight, config::kAvatarSize - static_cast<int>(block->y));
  if (width <= 0 || height <= 0) {
    return 1;
  }
  if (width == block->iWidth) {
    gTft->pushImage(gImageX + block->x, gImageY + block->y, width, height,
                    block->pPixels);
  } else {
    for (int row = 0; row < height; ++row) {
      gTft->pushImage(gImageX + block->x, gImageY + block->y + row, width, 1,
                      block->pPixels + row * block->iWidth);
    }
  }
  yield();
  return 1;
}

void* openJpegFile(const char* filename, int32_t* size) {
  gJpegFile = LittleFS.open(filename, FILE_READ);
  if (!gJpegFile) {
    *size = 0;
    return nullptr;
  }
  *size = gJpegFile.size();
  return &gJpegFile;
}

void closeJpegFile(void*) {
  if (gJpegFile) {
    gJpegFile.close();
  }
}

int32_t readJpegFile(JPEGFILE*, uint8_t* buffer, int32_t length) {
  return gJpegFile ? gJpegFile.read(buffer, length) : 0;
}

int32_t seekJpegFile(JPEGFILE*, int32_t position) {
  return gJpegFile && gJpegFile.seek(position) ? 1 : 0;
}

void* openPngFile(const char* filename, int32_t* size) {
  gPngFile = LittleFS.open(filename, FILE_READ);
  if (!gPngFile) {
    *size = 0;
    return nullptr;
  }
  *size = gPngFile.size();
  return &gPngFile;
}

void closePngFile(void*) {
  if (gPngFile) {
    gPngFile.close();
  }
}

int32_t readPngFile(PNGFILE*, uint8_t* buffer, int32_t length) {
  return gPngFile ? gPngFile.read(buffer, length) : 0;
}

int32_t seekPngFile(PNGFILE*, int32_t position) {
  return gPngFile && gPngFile.seek(position) ? 1 : 0;
}

int drawPngLine(PNGDRAW* line) {
  if (gTft == nullptr || line->y >= config::kAvatarSize) {
    return 1;
  }
  const int width = min(line->iWidth, config::kAvatarSize);
  gPng.getLineAsRGB565(line, gPngLine, PNG_RGB565_BIG_ENDIAN, 0x161B22);
  gTft->pushImage(gImageX, gImageY + line->y, width, 1, gPngLine);
  yield();
  return 1;
}
}  // namespace

bool DisplayManager::begin() {
  pinMode(cyd::kBacklight, OUTPUT);
  digitalWrite(cyd::kBacklight, LOW);
  tft_.init();
  tft_.setRotation(cyd::kRotation);
  // This CYD panel revision has inverted physical polarity: INVON produces
  // normal visible colors, while INVOFF produces a photographic negative.
  tft_.invertDisplay(true);
  delay(10);
  tft_.setSwapBytes(true);
  gUtf8.begin(tft_);
  ledcSetup(0, 5000, 8);
  ledcAttachPin(cyd::kBacklight, 0);

  background_ = tft_.color565(13, 17, 23);
  surface_ = tft_.color565(22, 27, 34);
  text_ = tft_.color565(240, 246, 252);
  muted_ = tft_.color565(139, 148, 158);
  blue_ = tft_.color565(47, 129, 247);
  green_ = tft_.color565(63, 185, 80);
  heart_ = tft_.color565(248, 81, 115);

  tft_.fillScreen(background_);
  digitalWrite(cyd::kBacklight, HIGH);
  Serial.printf("[display] TFT initialized: %dx%d landscape\n", tft_.width(),
                tft_.height());
  return tft_.width() == cyd::kWidth && tft_.height() == cyd::kHeight;
}

void DisplayManager::setBrightness(uint8_t percent) {
  percent = constrain(percent, 20, 100);
  ledcWrite(0, static_cast<uint32_t>(percent) * 255 / 100);
}

void DisplayManager::showBoot() {
  tft_.fillScreen(background_);
  tft_.fillCircle(160, 69, 31, surface_);
  icons::drawHeart(tft_, 142, 49, 36, heart_);
  tft_.setTextDatum(MC_DATUM);
  tft_.setTextColor(text_, background_);
  tft_.drawString("GitHub Supporter", 160, 122, 4);
  tft_.drawString("Display", 160, 151, 4);
  tft_.setTextColor(blue_, background_);
  tft_.drawString("@maggogerka", 160, 184, 2);
  tft_.setTextColor(muted_, background_);
  tft_.drawString("v" + String(config::kFirmwareVersion), 160, 218, 2);
  tft_.setTextDatum(TL_DATUM);
}

void DisplayManager::showProgress(const String& stage, const String& detail) {
  tft_.fillScreen(background_);
  tft_.fillRoundRect(12, 18, 296, 204, 12, surface_);
  tft_.setTextColor(text_, surface_);
  tft_.setTextDatum(TC_DATUM);
  tft_.drawString("GitHub Supporter Display", 160, 39, 4);
  tft_.setTextColor(blue_, surface_);
  tft_.drawString("@maggogerka", 160, 72, 2);

  tft_.drawRoundRect(34, 116, 252, 18, 9, muted_);
  const int progress = stage.indexOf("GitHub") >= 0
                           ? 4
                           : stage.indexOf("time") >= 0
                                 ? 3
                                 : stage.indexOf("Wi-Fi") >= 0 ? 2 : 1;
  tft_.fillRoundRect(37, 119, progress * 61, 12, 6, green_);
  tft_.setTextColor(text_, surface_);
  tft_.drawString(asciiSafe(stage, 38), 160, 151, 2);
  if (!detail.isEmpty()) {
    tft_.setTextColor(muted_, surface_);
    tft_.drawString(asciiSafe(detail, 45), 160, 180, 2);
  }
  tft_.setTextDatum(TL_DATUM);
}

void DisplayManager::showError(const String& title, const String& detail,
                               uint32_t retryInSeconds) {
  tft_.fillScreen(background_);
  tft_.fillRoundRect(12, 22, 296, 196, 12, surface_);
  tft_.fillCircle(160, 65, 24, heart_);
  tft_.setTextDatum(MC_DATUM);
  tft_.setTextColor(background_, heart_);
  tft_.drawString("!", 160, 65, 4);
  tft_.setTextColor(text_, surface_);
  tft_.drawString(asciiSafe(title, 34), 160, 108, 4);
  tft_.setTextColor(muted_, surface_);
  tft_.drawString(asciiSafe(detail, 46), 160, 144, 2);
  tft_.setTextColor(blue_, surface_);
  tft_.drawString("Retry in " + String(retryInSeconds) + " s", 160, 184, 2);
  tft_.setTextDatum(TL_DATUM);
}

void DisplayManager::showEmpty(bool connected, time_t updatedAt) {
  tft_.fillScreen(background_);
  drawHeader(connected, false, 0, 0);
  tft_.fillRoundRect(10, 34, 300, 164, 12, surface_);
  icons::drawHeart(tft_, 138, 57, 44, heart_);
  tft_.setTextDatum(MC_DATUM);
  tft_.setTextColor(text_, surface_);
  tft_.drawString("The first follower", 160, 125, 4);
  tft_.drawString("is still ahead", 160, 153, 4);
  tft_.setTextColor(blue_, surface_);
  tft_.drawString("github.com/maggogerka", 160, 181, 2);
  tft_.setTextDatum(TL_DATUM);
  drawFooter(updatedAt);
  drawNavButtons();
}

void DisplayManager::showFollower(const FollowerProfile& profile, size_t index,
                                  size_t total, bool connected, bool offline,
                                  time_t updatedAt, AvatarCache& cache) {
  tft_.fillScreen(background_);
  drawHeader(connected, offline, index + 1, total);
  tft_.fillRoundRect(8, 31, 304, 169, 12, surface_);

  const String avatarPath = cache.pathFor(profile.id);
  const AvatarCache::Format avatarFormat = cache.formatOf(avatarPath);
  if (!drawAvatar(avatarPath, avatarFormat, 18, 45)) {
    icons::drawAvatarPlaceholder(tft_, 18, 45, config::kAvatarSize,
                                 profile.login, background_, blue_, text_);
  } else {
    tft_.drawRoundRect(18, 45, config::kAvatarSize, config::kAvatarSize, 10,
                       blue_);
  }

  const int textX = 126;
  tft_.setTextDatum(TL_DATUM);
  tft_.setTextColor(text_, surface_);
  tft_.drawString(asciiSafe(profile.effectiveName(), 20), textX, 45, 4);
  tft_.setTextColor(blue_, surface_);
  tft_.drawString("@" + asciiSafe(profile.login, 24), textX, 76, 2);
  tft_.setTextColor(muted_, surface_);
  tft_.drawString(asciiSafe(profile.shortUrl(), 28), textX, 97, 2);
  tft_.drawString(String(profile.publicRepos) + " public repos", textX, 118, 2);

  icons::drawHeart(tft_, 128, 145, 15, heart_);
  icons::drawHeart(tft_, 151, 149, 11, heart_);
  icons::drawHeart(tft_, 169, 145, 15, heart_);
  gUtf8.setFontMode(1);
  gUtf8.setFontDirection(0);
  gUtf8.setForegroundColor(text_);
  gUtf8.setBackgroundColor(surface_);
  gUtf8.setFont(u8g2_font_6x12_t_cyrillic);
  gUtf8.drawUTF8(188, 151, "Спасибо за подписку!");
  gUtf8.setForegroundColor(muted_);
  gUtf8.drawUTF8(58, 187, "Спасибо за поддержку моих проектов!");
  tft_.setTextDatum(TL_DATUM);
  drawFooter(updatedAt);
  drawNavButtons();
}

void DisplayManager::showProvisioning(const String& accessPoint) {
  tft_.fillScreen(background_);
  tft_.setTextDatum(MC_DATUM);
  tft_.setTextColor(text_, background_);
  tft_.drawString("Wi-Fi setup", 160, 38, 4);
  tft_.setTextColor(blue_, background_);
  tft_.drawString(accessPoint, 160, 88, 4);
  tft_.setTextColor(text_, background_);
  tft_.drawString("Connect with phone or computer", 160, 128, 2);
  tft_.drawString("Open 192.168.4.1", 160, 154, 4);
  tft_.setTextColor(muted_, background_);
  tft_.drawString("Select Wi-Fi and enter its password", 160, 196, 2);
  tft_.setTextDatum(TL_DATUM);
}

void DisplayManager::showSettings(const String& ssid, int rssi,
                                  const String& ip, size_t followers,
                                  int rateRemaining, size_t fsUsed,
                                  size_t fsTotal, uint32_t heap,
                                  uint8_t brightness) {
  tft_.fillScreen(background_);
  tft_.setTextColor(text_, background_);
  tft_.drawString("Settings  v" + String(config::kFirmwareVersion), 10, 8, 4);
  tft_.setTextColor(muted_, background_);
  tft_.drawString(asciiSafe(ssid, 20) + "  " + String(rssi) + " dBm", 10, 39,
                  2);
  tft_.drawString(ip + "  Followers: " + String(followers), 10, 57, 2);
  tft_.drawString("API: " + String(rateRemaining) + "  FS: " +
                      String(fsUsed / 1024) + "/" + String(fsTotal / 1024) +
                      "K  Heap: " + String(heap / 1024) + "K",
                  10, 75, 2);
  const char* labels[] = {"Refresh", "Brightness", "Statistics",
                          "Calibrate", "Clear avatars", "Reset Wi-Fi"};
  for (int i = 0; i < 6; ++i) {
    const int x = (i % 2) * 155 + 7;
    const int y = 101 + (i / 2) * 38;
    tft_.fillRoundRect(x, y, 148, 31, 6, surface_);
    tft_.setTextColor(i >= 4 ? heart_ : text_, surface_);
    tft_.setTextDatum(MC_DATUM);
    tft_.drawString(labels[i], x + 74, y + 15, 2);
  }
  tft_.setTextDatum(MC_DATUM);
  tft_.setTextColor(blue_, background_);
  tft_.drawString("< Back     Brightness " + String(brightness) + "%", 160,
                  225, 2);
  tft_.setTextDatum(TL_DATUM);
}

void DisplayManager::showStatistics(size_t followers, size_t avatars,
                                    time_t updatedAt, int rssi,
                                    int httpStatus) {
  tft_.fillScreen(background_);
  tft_.setTextDatum(MC_DATUM);
  tft_.setTextColor(text_, background_);
  tft_.drawString("@maggogerka statistics", 160, 28, 4);
  tft_.setTextColor(blue_, background_);
  tft_.drawString(String(followers), 80, 88, 4);
  tft_.drawString(String(avatars), 240, 88, 4);
  tft_.setTextColor(muted_, background_);
  tft_.drawString("followers", 80, 118, 2);
  tft_.drawString("avatars", 240, 118, 2);
  tft_.setTextColor(text_, background_);
  tft_.drawString("Wi-Fi " + String(rssi) + " dBm   HTTP " +
                      String(httpStatus),
                  160, 157, 2);
  tft_.drawString("Updated " + formatTime(updatedAt), 160, 182, 2);
  tft_.setTextColor(blue_, background_);
  tft_.drawString("< Back", 160, 222, 2);
  tft_.setTextDatum(TL_DATUM);
}

void DisplayManager::showQr(const FollowerProfile& profile) {
  tft_.fillScreen(background_);
  const String url = "https://github.com/" + profile.login;
  QRCode qr;
  qrcode_initText(&qr, gQrData, 5, ECC_LOW, url.c_str());
  const int scale = min(3, 180 / qr.size);
  const int size = qr.size * scale;
  const int x0 = (320 - size) / 2;
  const int y0 = 9;
  tft_.fillRect(x0 - 6, y0 - 6, size + 12, size + 12, TFT_WHITE);
  for (uint8_t y = 0; y < qr.size; ++y)
    for (uint8_t x = 0; x < qr.size; ++x)
      if (qrcode_getModule(&qr, x, y))
        tft_.fillRect(x0 + x * scale, y0 + y * scale, scale, scale, TFT_BLACK);
  tft_.setTextDatum(MC_DATUM);
  tft_.setTextColor(blue_, background_);
  tft_.drawString("@" + asciiSafe(profile.login, 32), 160, 205, 2);
  tft_.setTextColor(text_, background_);
  tft_.drawString("< Back", 160, 226, 2);
  tft_.setTextDatum(TL_DATUM);
}

void DisplayManager::showBrightness(uint8_t selected) {
  tft_.fillScreen(background_);
  tft_.setTextDatum(MC_DATUM);
  tft_.setTextColor(text_, background_);
  tft_.drawString("Brightness", 160, 35, 4);
  for (int i = 0; i < 5; ++i) {
    const int x = 12 + i * 62;
    const uint8_t value = config::kBrightnessLevels[i];
    tft_.fillRoundRect(x, 92, 50, 55, 7,
                       value == selected ? blue_ : surface_);
    tft_.setTextColor(text_, value == selected ? blue_ : surface_);
    tft_.drawString(String(value) + "%", x + 25, 119, 2);
  }
  tft_.setTextColor(blue_, background_);
  tft_.drawString("< Back", 160, 210, 2);
  tft_.setTextDatum(TL_DATUM);
}

void DisplayManager::showCalibration(uint8_t step) {
  static constexpr int16_t points[4][2] = {{22, 22}, {297, 22},
                                           {297, 217}, {22, 217}};
  tft_.fillScreen(background_);
  tft_.setTextDatum(MC_DATUM);
  tft_.setTextColor(text_, background_);
  tft_.drawString("Touch calibration", 160, 120, 2);
  const uint8_t index = min<uint8_t>(step, 3);
  tft_.drawCircle(points[index][0], points[index][1], 9, heart_);
  tft_.drawLine(points[index][0] - 12, points[index][1],
                points[index][0] + 12, points[index][1], heart_);
  tft_.drawLine(points[index][0], points[index][1] - 12,
                points[index][0], points[index][1] + 12, heart_);
  tft_.setTextDatum(TL_DATUM);
}

void DisplayManager::showConfirmation(const String& title,
                                      const String& detail) {
  tft_.fillScreen(background_);
  tft_.setTextDatum(MC_DATUM);
  tft_.setTextColor(heart_, background_);
  tft_.drawString(title, 160, 55, 4);
  tft_.setTextColor(text_, background_);
  tft_.drawString(asciiSafe(detail, 45), 160, 105, 2);
  tft_.fillRoundRect(25, 155, 120, 45, 8, surface_);
  tft_.fillRoundRect(175, 155, 120, 45, 8, heart_);
  tft_.drawString("Cancel", 85, 177, 2);
  tft_.setTextColor(background_, heart_);
  tft_.drawString("Confirm", 235, 177, 2);
  tft_.setTextDatum(TL_DATUM);
}

void DisplayManager::drawNavButtons() {
  tft_.setTextColor(blue_, background_);
  tft_.drawString("<", 2, 109, 4);
  tft_.setTextDatum(TR_DATUM);
  tft_.drawString(">", 318, 109, 4);
  tft_.setTextDatum(TL_DATUM);
  tft_.drawRoundRect(284, 2, 30, 26, 5, muted_);
  tft_.drawString("*", 294, 7, 2);
}

bool DisplayManager::drawAvatar(const String& path, AvatarCache::Format format,
                                int16_t x, int16_t y) {
  gTft = &tft_;
  gImageX = x;
  gImageY = y;
  bool drawn = false;
  const uint32_t startedAtMs = millis();
  Serial.printf("[display] Decoding avatar %s (format=%u), free heap=%u\n",
                path.c_str(), static_cast<unsigned>(format),
                ESP.getFreeHeap());

  if (format == AvatarCache::Format::Jpeg) {
    if (gJpeg.open(path.c_str(), openJpegFile, closeJpegFile, readJpegFile,
                   seekJpegFile, drawJpegBlock)) {
      int options = 0;
      if (gJpeg.getWidth() > config::kAvatarSize * 4) {
        options = JPEG_SCALE_EIGHTH;
      } else if (gJpeg.getWidth() > config::kAvatarSize * 2) {
        options = JPEG_SCALE_QUARTER;
      } else if (gJpeg.getWidth() > config::kAvatarSize) {
        options = JPEG_SCALE_HALF;
      }
      drawn = gJpeg.decode(0, 0, options) != 0;
      gJpeg.close();
    }
  } else if (format == AvatarCache::Format::Png) {
    const int result = gPng.open(path.c_str(), openPngFile, closePngFile,
                                 readPngFile, seekPngFile, drawPngLine);
    Serial.printf(
        "[display] PNG open=%d, size=%dx%d, bpp=%d, type=%d, error=%d\n",
        result, result == PNG_SUCCESS ? gPng.getWidth() : 0,
        result == PNG_SUCCESS ? gPng.getHeight() : 0,
        result == PNG_SUCCESS ? gPng.getBpp() : 0,
        result == PNG_SUCCESS ? gPng.getPixelType() : 0,
        gPng.getLastError());
    if (result == PNG_SUCCESS) {
      if (gPng.getWidth() <= 320 && gPng.getHeight() <= 320) {
        const int decodeResult = gPng.decode(nullptr, 0);
        drawn = decodeResult == PNG_SUCCESS;
        Serial.printf("[display] PNG decode=%d, error=%d\n", decodeResult,
                      gPng.getLastError());
      } else {
        Serial.println("[display] PNG dimensions exceed the safe limit");
      }
      gPng.close();
    }
  }

  gTft = nullptr;
  Serial.printf("[display] Avatar decode %s in %u ms, free heap=%u\n",
                drawn ? "OK" : "failed", millis() - startedAtMs,
                ESP.getFreeHeap());
  return drawn;
}

void DisplayManager::drawHeader(bool connected, bool offline, size_t index,
                                size_t total) {
  tft_.setTextDatum(TL_DATUM);
  tft_.setTextColor(text_, background_);
  tft_.drawString("@maggogerka supporters", 9, 8, 2);
  if (offline) {
    tft_.setTextColor(heart_, background_);
    tft_.drawString("OFFLINE", 227, 8, 2);
  }
  icons::drawWifi(tft_, 300, 17, connected, green_, muted_);
  if (total > 0) {
    tft_.setTextDatum(TR_DATUM);
    tft_.setTextColor(muted_, background_);
    tft_.drawString(String(index) + " / " + String(total), offline ? 220 : 278,
                    8, 2);
    tft_.setTextDatum(TL_DATUM);
  }
}

void DisplayManager::drawFooter(time_t updatedAt) {
  tft_.setTextDatum(BL_DATUM);
  tft_.setTextColor(muted_, background_);
  tft_.drawString("Updated " + formatTime(updatedAt), 10, 232, 2);
  tft_.setTextDatum(BR_DATUM);
  tft_.setTextColor(blue_, background_);
  tft_.drawString("github.com/maggogerka", 310, 232, 2);
  tft_.setTextDatum(TL_DATUM);
}

String DisplayManager::asciiSafe(const String& value,
                                 size_t maximumChars) const {
  String result;
  result.reserve(maximumChars);
  bool replacementAdded = false;
  for (size_t i = 0; i < value.length() && result.length() < maximumChars;
       ++i) {
    const uint8_t c = static_cast<uint8_t>(value[i]);
    if (c >= 32 && c <= 126) {
      result += static_cast<char>(c);
      replacementAdded = false;
    } else if (!replacementAdded) {
      result += '?';
      replacementAdded = true;
    }
  }
  if (value.length() > maximumChars && result.length() >= 3) {
    result.remove(result.length() - 3);
    result += "...";
  }
  return result;
}

String DisplayManager::formatTime(time_t value) const {
  if (value <= 0) {
    return "--:--";
  }
  struct tm localTime {};
  localtime_r(&value, &localTime);
  char buffer[20];
  strftime(buffer, sizeof(buffer), "%d.%m %H:%M", &localTime);
  return String(buffer);
}
