#pragma once

// Canonical ESP32-2432S028R (single-USB/classic CYD) mapping.
// The same values are supplied to TFT_eSPI from platformio.ini so its
// globally installed files never need to be edited.
namespace cyd {
inline constexpr int kTftMiso = 12;
inline constexpr int kTftMosi = 13;
inline constexpr int kTftClock = 14;
inline constexpr int kTftChipSelect = 15;
inline constexpr int kTftDataCommand = 2;
inline constexpr int kTftReset = -1;
inline constexpr int kBacklight = 21;
inline constexpr int kTouchChipSelect = 33;
inline constexpr int kWidth = 320;
inline constexpr int kHeight = 240;
inline constexpr int kRotation = 1;
}  // namespace cyd
