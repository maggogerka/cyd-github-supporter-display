# GitHub Supporter Display v0.2.0

Автономный настольный информер для **Cheap Yellow Display ESP32-2432S028R**. Он получает всех публичных followers профиля [@maggogerka](https://github.com/maggogerka), сохраняет их в LittleFS и показывает карусель карточек благодарности. VPS, отдельный сервер, GitHub PAT, microSD, корпус и подставка не нужны.

![Работающий GitHub Supporter Display на CYD](docs/images/cyd-github-supporter-display.jpg)

## Возможности

- экран 2,8″, 320×240 landscape, ILI9341_2 и корректная полярность цветов этой ревизии CYD;
- captive portal `CYD-GitHub-XXXX`: сеть настраивается без изменения исходников и сохраняется ESP32 в NVS;
- XPT2046 на отдельной SPI-шине, фильтрация касаний и мастер калибровки;
- Previous/Next, ручной Refresh с cooldown, Settings, Statistics, Brightness и локальный QR-код follower;
- яркость 20/40/60/80/100% через PWM GPIO21 с сохранением в NVS;
- pagination GitHub followers до безопасного внутреннего лимита;
- ETag/`If-None-Match`, HTTP 304 и ожидание `X-RateLimit-Reset`;
- подробный профиль запрашивается только пока остаётся безопасный запас API;
- атомарный versioned cache профилей и JPEG/PNG-аватаров в LittleFS;
- offline startup: сохранённая карусель появляется до подключения Wi‑Fi;
- безопасный TLS с root CA и синхронизацией времени NTP;
- локальная поддержка 16-bit PNG без стороннего сервиса изображений;
- CI собирает secure firmware, проверяет отсутствие secrets и запускает native tests.

## Быстрый запуск

1. Подключите CYD USB-кабелем данных.
2. Определите порт: `pio device list`.
3. Соберите: `pio run -e cyd`.
4. Прошейте: `pio run -e cyd -t upload --upload-port COMX`.
5. Откройте журнал: `pio device monitor --port COMX --baud 115200`.
6. Если сети ещё нет, подключитесь телефоном к `CYD-GitHub-XXXX`.
7. Откройте `192.168.4.1`, выберите Wi‑Fi и введите пароль.
8. Дождитесь синхронизации и карусели.

Пароль Wi‑Fi хранится только в NVS устройства и никогда не выводится в Serial. Локальный `include/secrets.h` поддерживается исключительно для однократной миграции существующей установки v0.1.0 и игнорируется Git.

## Управление

- край слева/справа карточки — Previous/Next;
- карточка/ссылка — QR-код `https://github.com/{login}`;
- кнопка `*` справа сверху — Settings;
- верхняя строка — Statistics;
- Settings → Refresh, Brightness, Statistics, Calibrate, Clear avatars, Reset Wi‑Fi;
- опасные действия требуют подтверждения;
- после ручного переключения 10-секундный таймер карусели начинается заново.

### Калибровка touch

При первом запуске без сохранённых данных мастер открывается автоматически. Последовательно нажмите четыре мишени в углах. Повторно его можно открыть через Settings → Calibrate. Прошивка проверяет геометрию точек, определяет swap/invert, сохраняет min/max в NVS и сразу применяет результат. При неисправном сенсоре автоматическая карусель продолжает работать.

### Смена Wi‑Fi

Settings → Reset Wi‑Fi → Confirm удаляет только сетевые credentials и перезапускает setup mode. Профили и аватары остаются. Чтобы принудительно открыть портал, удерживайте кнопку `BOOT` во время запуска устройства.

## Архитектура

```text
Application
├── NetworkManager + WiFiManager (NVS/captive portal/NTP)
├── GitHubClient (pagination/ETag/rate limit/TLS)
├── ProfileStore + AvatarCache (atomic LittleFS/offline)
├── TouchManager + SettingsStore (XPT2046/Preferences)
└── DisplayManager (carousel/QR/settings/statistics/brightness)
```

Основной цикл не создаёт дополнительных FreeRTOS tasks. Сетевые данные ограничены `kMaximumFollowers`, JSON фильтруется, изображения декодируются по одному, а крупные decoder buffers не размещаются в стеке.

## GitHub API и rate limit

Используются публичные endpoints:

```text
GET /users/maggogerka/followers?per_page=100&page=N
GET /users/{login}
```

PAT не нужен. Первая страница использует сохранённый ETag. При 304 cache не перезаписывается. При низком `X-RateLimit-Remaining` дополнительные details-запросы прекращаются; при нуле следующая попытка назначается после `X-RateLimit-Reset`, а экран продолжает показывать offline cache.

## LittleFS

Раздел LittleFS — `0x1F0000` (1 966 KiB). В нём находятся `/profiles.json` со schema version/ETag и `/avatars/{numeric-id}.img`. Запись профилей выполняется через temporary file + rename. Аватары ограничены по размеру, проверяются по сигнатуре, orphan-файлы удаляются. Clear avatars не затрагивает профили и Wi‑Fi.

## Аппаратная конфигурация

| Сигнал | GPIO |
|---|---:|
| TFT MISO/MOSI/SCLK | 12 / 13 / 14 |
| TFT CS/DC/RST | 15 / 2 / -1 |
| Backlight | 21 |
| Touch SCLK/MISO/MOSI | 25 / 39 / 32 |
| Touch CS/IRQ | 33 / 36 |

TFT работает через HSPI, touch — через отдельный VSPI. PSRAM и microSD не используются.

## Разделы flash и OTA

| Раздел | Offset | Размер |
|---|---:|---:|
| NVS | `0x9000` | 20 KiB |
| OTA metadata | `0xE000` | 8 KiB |
| Application (factory) | `0x10000` | 1 984 KiB |
| LittleFS | `0x200000` | 1 984 KiB |
| Core dump | `0x3F0000` | 64 KiB |

OTA в v0.2.0 намеренно не включена. Firmware занимает около 1,25 МБ; две application slots с безопасным запасом вместе с LittleFS 1,94 МБ не помещаются в 4 МБ. Уменьшать offline avatar cache ради формального OTA нельзя. Обновление выполняется по USB.

## Bruce

Прошивка заменяет Bruce. Для восстановления загрузите официальный Bruce binary/web installer. Это также заменит GitHub Supporter Display; LittleFS может потребовать повторной инициализации.

## Устранение неполадок

| Симптом | Решение |
|---|---|
| Видимый цветовой негатив | для проверенной платы требуется `TFT_INVERSION_ON`; настройка уже в `platformio.ini` |
| Нет Wi‑Fi | подключитесь к `CYD-GitHub-XXXX` и откройте `192.168.4.1` |
| HTTP 403/429 | устройство показывает cache и ждёт официальный reset лимита |
| Touch промахивается | Settings → Calibrate |
| Аватар-placeholder | неизвестный формат, TLS/redirect, лимит файла или LittleFS |
| Upload не начинается | проверьте COM, data-кабель; при необходимости удерживайте BOOT на `Connecting...` |

## Безопасность

- production build не использует `setInsecure()`;
- Wi‑Fi password, PAT и приватные ключи не входят в репозиторий;
- `include/secrets.h` запрещён CI;
- диагностический `CYD_ALLOW_INSECURE_TLS` выключен по умолчанию;
- captive portal активируется только при отсутствии/сбросе credentials и имеет ограниченное время работы.

## Ограничения

- без PAT анонимный GitHub API имеет небольшой общий лимит;
- RAM/flash классического ESP32 требуют ограничения числа followers и размера avatar cache;
- system emoji не используются; интерфейс рисует собственные иконки;
- корпус, подставка, microSD, VPS и OTA не входят в проект.

## Лицензия

Код проекта распространяется по [MIT License](LICENSE). Локальная модификация PNGdec сохраняет исходную Apache-2.0 лицензию в `lib/PNGdec16/LICENSE`.
