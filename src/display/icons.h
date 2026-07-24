#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

namespace icons {
void drawHeart(TFT_eSPI& tft, int16_t x, int16_t y, int16_t size,
               uint16_t color);
void drawWifi(TFT_eSPI& tft, int16_t x, int16_t y, bool connected,
              uint16_t activeColor, uint16_t inactiveColor);
void drawAvatarPlaceholder(TFT_eSPI& tft, int16_t x, int16_t y, int16_t size,
                           const String& login, uint16_t surface,
                           uint16_t accent, uint16_t text);
}  // namespace icons
