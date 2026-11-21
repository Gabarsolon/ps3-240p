# CRT Toolset v1.6.1: SCART RGB DVD fix

Everything in v1.6.0 (single-SD-family 240p/288p, HDMI custom resolutions)
plus a new menu, **SCART RGB DVD fix**.

## What it does

On a SCART RGB setup, the PS3's native DVD and Blu-ray player switches the AV
MULTI output from RGB to component video, so the picture turns green or purple.
This release bundles the plugin that fixes it and lets you turn it on from the
app.

- **Load now until reboot:** installs the plugin into `/dev_hdd0/plugins/` and
  loads it into a free VSH slot. Nothing is unloaded.
- **Load at every boot:** adds or removes the plugin's line in
  `/dev_hdd0/boot_plugins.txt`. The original list is saved once as
  `boot_plugins.txt.crt-backup`, the new list is read back, and the original is
  restored if the readback differs.
- **About / credits.**

Set Video Output to **AV MULTI / SCART with RGB**. Play a DVD or Blu-ray from
the XMB; the plugin keeps the output RGB. HDMI is not affected.

## Credit

The fix is **RGB DVD Patch for PS3** (`disc_rgb_fix` 2.1.3) by **WeebLabs**,
MIT licence: https://github.com/WeebLabs/RGB-DVD-Patch-for-PS3. It is included
unmodified (SHA-256
`aff98e5da5b8d0826ab1e9e645d51e7bfd37ffb1027903056b60d7a6f13b1ca1`) with its
licence text (`WeebLabs-LICENSE.txt` in the app's USRDIR). All credit for the
mechanism and plugin goes to them. This project only adds the menu and the boot
option.

## Validation

- PROVEN (owner, Evilnat 4.93, SCART RGB, DVD): picture correct, both loaded at
  runtime and loaded from `boot_plugins.txt` after a reboot. Log lines
  `REWRITE: DVD-start batch kept RGB` and `send result 0x74`.
- CLAIMED (plugin author): written and tested on Evilnat 4.92 + Cobra 8.5. The
  plugin checks the exact VSH send wrapper and does nothing if it differs;
  the same bytes were found in the 4.93 VSH.
- UNKNOWN: Blu-ray on 4.93, other firmware, other CFW.

## Build

`python -B tools/build_ps3_app.py --sd-timing-fix` -> `CRT-Toolset-1.6.1.pkg`,
APP_VER 01.82, in-app `1.6.1`.

SHA-256: `1609ff9a6b620b1142f8ccd9bba08e2ac2f9db48e452351fa355150b310841c1`

## If the console fails to boot after adding a plugin

Start Recovery Mode and choose Restart System. On Evilnat this boots without
plugins. Then remove the line from `boot_plugins.txt` (or restore
`boot_plugins.txt.crt-backup`).
