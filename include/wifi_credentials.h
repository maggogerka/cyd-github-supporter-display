#pragma once

#if __has_include("secrets.h")
#include "secrets.h"
#define CYD_HAS_WIFI_SECRETS 1
#else
inline constexpr char WIFI_SSID[] = "";
inline constexpr char WIFI_PASSWORD[] = "";
#define CYD_HAS_WIFI_SECRETS 0
#endif
