# CRT Toolset v1.6.0-dev4: HDMI custom resolutions

This release adds an **HDMI custom resolutions** menu with five modes:

| Mode | For | Notes |
|---|---|---|
| **540p** (1920x540 progressive, 60 Hz) | HD CRTs | start from XMB 1080i or 1080p |
| **1024x768** | 4:3 PC monitors / VGA-style CRT setups | start from XMB 1080p |
| **1280x768** | wide PC monitors | start from XMB 1080p |
| **1280x1024** | 5:4 PC monitors | start from XMB 1080p |
| **1920x1200** | 16:10 PC monitors | start from XMB 1080p |

Each choice opens a 15-second preview inside the app. **X** keeps it for the
session; **Circle**, or no input, restores the previous output. The helper
restores stock output by itself even if the app is closed or the PC
disconnects. It is the exact package tested on the owner's console.

## PC resolutions

- **PROVEN (capture + receiver, in-app):** 1280x768 showed a readable
  confirmation screen, the receiver reported 1280x768, and the timeout
  restored 1920x1080. 1024x768 bound and drew the app at 1024x768 with a
  readable confirmation screen and checked restoration. See
  [HDMI custom resolutions](https://github.com/Gabarsolon/ps3-240p/blob/v1.6.0-dev4/docs/HDMI_CUSTOM_RESOLUTIONS.md).
- **PROVEN (earlier XMB-path tests):** 1280x1024 showed XMB with the receiver
  reporting 1280x1024. 1920x1200 showed XMB with a live receiver report of
  1920x1200. Both restored stock 1080p. See
  [PC presets](https://github.com/Gabarsolon/ps3-240p/blob/v1.6.0-dev4/docs/PC_PRESETS_DEVELOPMENT.md)
  and [XGA](https://github.com/Gabarsolon/ps3-240p/blob/v1.6.0-dev4/docs/XGA_DEVELOPMENT.md).
- **UNKNOWN:** these four modes were not retested with this exact package,
  exact refresh is unmeasured (nominal 60 Hz requested), and 1024x768 uses
  non-standard clock/totals. Game compatibility is untested.

## How to use 540p

1. Set XMB to HDMI **1080i** (recommended for HD CRTs, which usually lack
   1080p) or **1080p**. Other XMB resolutions are refused for 540p.
2. CRT Toolset → HDMI custom resolutions → **540p / 16:9**.
3. Wait about 25 s while the helper switches 1080i → 720p → 1080i, then
   check the 540p picture and press X to keep it or Circle to restore.

## Evidence

- **PROVEN (owner, Xiaomi HDMI monitor):** the monitor reported **1920x540
  at 60 Hz** with a full, correct picture. See
  [540p resolved](https://github.com/Gabarsolon/ps3-240p/blob/main/docs/HDMI_CUSTOM_RESOLUTIONS.md#540p-resolved-2026-11-15).
- **PROVEN (capture frames, this package):** the in-app preview shows the
  complete menu with correct proportions. Hardware fetch pitch was 0x4000,
  from both a 1080i start and a 1080p start, and stock output was restored.
- **PROVEN (cause of the earlier failures):** forcing HDMI AVI VIC 0 broke
  monitor lock. The in-app crop came from the doubled pitch writer being
  skipped. Both are fixed in this build.

## Limits

- **UNKNOWN:** real HD CRT behavior, component output, games (especially
  games that keep a fixed display pitch), other models/firmware and
  measured refresh. 540p samples every other row of the 1080-line source;
  this is not filtered scaling.
- **UNKNOWN:** whether some displays reject the stock AVI description (VIC 5)
  for a progressive 1920x540 signal.
- The 16:9 picture fills each resolution; 4:3 and other aspect handling are
  planned for a later release.
- Requires CFW 4.93 (Evilnat + Cobra tested), HDMI only. Never install or
  update CRT Toolset while it is running.

## Files

| File | SHA-256 |
|---|---|
| `CRT-Toolset-1.6.0-dev4.pkg` (344784 bytes) | `ce14d2b178c9a6eac0489a931c96775e0b81628585bed7c3154b86d643c311e4` |
| helper `crt_hdmi_custom.sprx` | `556ad75bb7d445628c7694a214376bf9fa2ea32fca9c19951e0aee168fe8c2a6` |
