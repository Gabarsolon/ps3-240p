# CRT Toolset v1.6.0-dev5 — single-SD-family timing gate

Pre-release. Carries the full v1.6.0-dev4 (HDMI custom resolutions) feature
set plus one fix to the SD timing test.

## What changes from v1.6.0-dev4

**PROVEN (implementation):** fixes the SD timing test failing on consoles that
expose only one SD standard:

```text
Firmware does not expose both SD modes; timing test stopped
```

`cellVideoOutGetResolutionAvailability` reports only the console's region SD
standard — a PAL unit advertises 576i alone, an NTSC unit 480i alone — so the
old gate (which required **both** 480i and 576i available) stopped every
region-locked console. The encoder drives either interlaced family and the app
configures either in practice, so the gate now requires only that **one** SD
family be exposed. The transition still drives both families (480i↔576i, both
15 kHz-safe); `configure()` and the automatic 15-second revert recover a
console that genuinely refuses the second family.

- Changed: `app/source/switch_flow.c` `select_modes()` — the availability
  check became `available(target)<=0 && available(alternate)<=0` (was `||`).
  New message on genuine refusal: `Firmware exposes no SD mode for AV Multi;
  timing test stopped`.
- Host tests: 44/44 pass (`python -B tools/test_app.py`).

## Build

```text
python -B tools/build_ps3_app.py --sd-timing-fix
```

Output: `build/hdmi-custom/deliverables/CRT-Toolset-1.6.0-dev5.pkg`
APP_VER 01.74, in-app version `1.6.0-dev5`.
SHA-256 `5e1d519531d3451f9d6efbf0dbcd37385a930e79c2bc44b9af8c19034703f541`,
344,848 bytes.

## Validation

**PROVEN (owner live test, 2026-11-15):** PAL console, AV Multi Out, RGB SCART,
starting at 576i. Test 288p proceeded past the gate and the picture held;
previously this console hit the "does not expose both SD modes" stop. Installed
through webMAN; all four installed files matched the package byte-for-byte.
