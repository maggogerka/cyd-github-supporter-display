#include "icons.h"

namespace icons {

void drawHeart(TFT_eSPI& tft, int16_t x, int16_t y, int16_t size,
               uint16_t color) {
  const int16_t radius = size / 4;
  tft.fillCircle(x + radius, y + radius, radius, color);
  tft.fillCircle(x + size - radius, y + radius, radius, color);
  tft.fillTriangle(x, y + radius, x + size, y + radius, x + size / 2,
                   y + size, color);
}

void drawWifi(TFT_eSPI& tft, int16_t x, int16_t y, bool connected,
              uint16_t activeColor, uint16_t inactiveColor) {
  const uint16_t color = connected ? activeColor : inactiveColor;
  tft.drawArc(x, y, 9, 7, 225, 315, color, TFT_TRANSPARENT);
  tft.drawArc(x, y, 6, 4, 225, 315, color, TFT_TRANSPARENT);
  tft.fillCircle(x, y + 1, 2, color);
}

void drawAvatarPlaceholder(TFT_eSPI& tft, int16_t x, int16_t y, int16_t size,
                           const String& login, uint16_t surface,
                           uint16_t accent, uint16_t text) {
  tft.fillRoundRect(x, y, size, size, 12, surface);
  tft.drawRoundRect(x, y, size, size, 12, accent);
  tft.fillCircle(x + size / 2, y + size / 3, size / 7, accent);
  tft.fillRoundRect(x + size / 4, y + size / 2, size / 2, size / 3, 8,
                    accent);
  if (!login.isEmpty()) {
    tft.setTextColor(text, surface);
    tft.setTextDatum(MC_DATUM);
    String initial = login.substring(0, 1);
    initial.toUpperCase();
    tft.drawString(initial, x + size / 2, y + size / 2, 4);
    tft.setTextDatum(TL_DATUM);
  }
}

}  // namespace icons
