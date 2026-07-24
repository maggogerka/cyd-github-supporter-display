# GitHub Supporter Display

An ESP32-2432S028R (Cheap Yellow Display) dashboard that retrieves the public
followers of a GitHub account and presents them as a rotating set of thank-you
cards. The firmware runs entirely on the board and keeps a local cache for
offline operation.

![GitHub Supporter Display running on an ESP32-2432S028R](docs/images/cyd-github-supporter-display.jpg)

## Features

- 320×240 landscape interface for the ILI9341-based CYD
- XPT2046 touch input with noise filtering and fixed mapping for the target board
- previous and next navigation, manual refresh, settings, statistics, and QR view
- five persistent backlight levels controlled through PWM
- Wi-Fi setup through the `CYD-GitHub-XXXX` captive portal
- GitHub follower pagination with a bounded in-memory profile list
- conditional requests through `ETag` and `If-None-Match`
- GitHub rate-limit tracking and delayed retries
- versioned, atomic profile storage in LittleFS
- cached JPEG, PNG, and 16-bit PNG avatars
- cached cards available during startup and network outages
- certificate-validated HTTPS after NTP time synchronization
- PlatformIO build and native logic tests in GitHub Actions

## Hardware

- ESP32-2432S028R with a 2.8-inch 320×240 display
- USB data cable
- 2.4 GHz Wi-Fi network

No server, microSD card, or GitHub personal access token is required.

## Pin Configuration

| Function | GPIO |
|---|---:|
| TFT MISO / MOSI / SCLK | 12 / 13 / 14 |
| TFT CS / DC / RST | 15 / 2 / -1 |
| Backlight | 21 |
| Touch SCLK / MISO / MOSI | 25 / 39 / 32 |
| Touch CS / IRQ | 33 / 36 |

The display uses HSPI. The touch controller uses a separate VSPI bus.

## Build and Upload

Install [PlatformIO](https://platformio.org/), connect the board, and run:

```powershell
pio run -e cyd
pio run -e cyd -t upload --upload-port COMX
pio device monitor --port COMX --baud 115200
```

Replace `COMX` with the serial port assigned to the board.

The default upload port in `platformio.ini` is only a convenience and can be
overridden on the command line.

## First-Time Wi-Fi Setup

1. Power on the board.
2. Connect a phone or computer to `CYD-GitHub-XXXX`.
3. Open `http://192.168.4.1`.
4. Select a 2.4 GHz Wi-Fi network and enter its password.
5. Wait for the display to synchronize its clock and load GitHub data.

The access-point suffix is derived from the ESP32 chip ID. Wi-Fi credentials
are stored in the ESP32 NVS partition and are never printed to the serial log.

Hold the `BOOT` button while the board starts to force the setup portal even
when credentials have already been saved.

## Touch Controls

- tap the left or right edge to show the previous or next follower
- tap the follower card to display its local QR code
- tap the top status row to open statistics
- tap `*` in the top-right corner to open settings
- use Settings for Refresh, Brightness, Statistics, Profile QR, Clear avatars,
  and Reset Wi-Fi

Destructive settings require confirmation. The touch input uses pressure,
stability, release, and debounce checks to reject electrical noise and repeated
events.

## GitHub API Behavior

The firmware uses these public REST endpoints:

```text
GET /users/{account}/followers?per_page=100&page=N
GET /users/{login}
```

Every request includes `Accept`, `User-Agent`, and `X-GitHub-Api-Version`
headers. The first followers page also sends the stored `ETag` through
`If-None-Match`. A `304 Not Modified` response keeps the current local data and
avoids unnecessary profile requests.

Unauthenticated requests have a limited hourly quota. The firmware reads
`X-RateLimit-Remaining` and `X-RateLimit-Reset`, stops optional detail requests
before the quota is exhausted, and continues to display cached cards while
waiting to retry.

The account shown by the display is configured through `kGitHubUser` in
`include/app_config.h`.

## Storage

The custom partition table provides:

| Partition | Offset | Size |
|---|---:|---:|
| NVS | `0x9000` | 20 KiB |
| OTA metadata | `0xE000` | 8 KiB |
| Application | `0x10000` | 1,984 KiB |
| LittleFS | `0x200000` | 1,984 KiB |
| Core dump | `0x3F0000` | 64 KiB |

LittleFS stores the versioned profile document at `/profiles.json` and avatars
under `/avatars/{github-id}.img`. Profile updates use a temporary file followed
by a rename. Stale and orphaned avatar files are pruned automatically.

## OTA

OTA is not enabled in v0.2.0. Two safely sized application slots and the current
offline avatar cache do not fit together in the 4 MB flash layout. Firmware
updates are performed over USB.

## Troubleshooting

| Symptom | Suggested action |
|---|---|
| Colors appear inverted | Keep `TFT_INVERSION_ON` enabled for the supported CYD revision |
| Wi-Fi setup does not appear | Hold `BOOT` during startup and connect to `CYD-GitHub-XXXX` |
| GitHub returns HTTP 403 or 429 | Leave the device running; it will use its cache and retry after the reported rate-limit reset |
| An avatar is unavailable | Check network access and LittleFS capacity; the card will use a generated placeholder |
| Upload cannot connect | Verify the serial port and USB data cable; hold `BOOT` while PlatformIO displays `Connecting...` if required |

## Security

- production builds validate the GitHub TLS certificate chain
- no Wi-Fi password, access token, or private key is committed
- CI rejects a committed `include/secrets.h`
- the optional insecure TLS build flag is disabled by default
- the captive portal runs only during setup or an explicitly requested reset

## Limitations

- the unauthenticated GitHub REST API has a shared per-IP rate limit
- the number of followers and avatar cache size are bounded for ESP32 memory
- the fixed touch mapping targets the ESP32-2432S028R board layout listed above
- OTA, external servers, and microSD storage are outside the v0.2.0 scope

## License

The project is available under the [MIT License](LICENSE). The local PNGdec
modification retains its original Apache-2.0 license in
`lib/PNGdec16/LICENSE`.
