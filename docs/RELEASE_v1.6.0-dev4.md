# CRT Toolset v1.6.0-dev4: HDMI custom resolutions (experimental)

This prerelease adds an **HDMI custom resolutions** menu: **540p (1920x540
progressive)** for HD CRTs, plus 1024x768, 1280x768, 1280x1024 and 1920x1200.
Each choice opens a 15-second preview. **X** keeps it for the session;
**Circle**, or no input, restores the previous output. The helper restores
stock output by itself even if the app is closed or the PC disconnects.
It is the exact package tested on the owner's console.

## How to use 540p

1. Set XMB to HDMI **1080i** (recommended for HD CRTs, which usually lack
   1080p) or **1080p**. Other XMB resolutions are refused for 540p.
2. CRT Toolset → HDMI custom resolutions → **540p / 16:9**.
3. Wait about 25 s while the helper switches 1080i → 720p → 1080i, then
   check the 540p picture and press X to keep it or Circle to restore.

## Evidence

- **PROVEN (owner, Xiaomi HDMI monitor):** the monitor reported **1920x540
  at 60 Hz** with a full, correct picture. See
  [540p resolved](HDMI_CUSTOM_RESOLUTIONS.md#540p-resolved-2026-11-15).
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
  planned for a later release. The PC modes were not retested for this build.
- Requires CFW 4.93 (Evilnat + Cobra tested), HDMI only. Never install or
  update CRT Toolset while it is running.

## Files

| File | SHA-256 |
|---|---|
| `CRT-Toolset-1.6.0-dev4.pkg` (344784 bytes) | `ce14d2b178c9a6eac0489a931c96775e0b81628585bed7c3154b86d643c311e4` |
| helper `crt_hdmi_custom.sprx` | `556ad75bb7d445628c7694a214376bf9fa2ea32fca9c19951e0aee168fe8c2a6` |
