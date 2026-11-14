# CRT Toolset v1.5.0-dev19 — experimental HDMI 540p test

This prerelease adds a timed **1920×540 progressive HDMI** experiment for HD
CRT testing. It is the exact dev19 package tested on the owner's console.
**It does not add permanent 540p output or 540p background enforcement.**

## Compatibility

- **PROVEN (implementation):** the HD experiment accepts **HDMI only**,
  starting from stock **1080i at 59.94/60 Hz** on the supported 4.93 CFW
  layout. It refuses component, RGB SCART, clones, 50 Hz, unknown records and
  active SD enforcement. Sources: `app/source/main.c:hd_run` and
  `plugin/vsh_hd540.c:prepare`.
- **PROVEN (capture/readback):** the signal recipe has produced a full XMB
  picture and receiver-reported 1920×540/progressive on the owner's HDMI
  capture setup. The source scanout samples alternate rows of the unchanged
  1080-line framebuffer; this is not filtered downscaling. Sources:
  [dev18 signal result](https://github.com/Gabarsolon/ps3-240p/blob/v1.5.0-dev19/docs/HD540_DEV18.md)
  and [dev19 app-menu fix](https://github.com/Gabarsolon/ps3-240p/blob/v1.5.0-dev19/docs/HD540_DEV19.md).
- **CLAIMED (owner):** the final dev19 manual test worked: “540p works!”
  Console: Slim CECH-25xx, Evilnat 4.93 CEX + Cobra, HDMI monitor/capture.
- **UNKNOWN:** real HD CRT compatibility, component 540p, other models or
  firmware, games, long-duration use and measured refresh rate. This build
  targets nominal 60 Hz operation; exact refresh has not been measured.
- **INFERRED:** ordinary HEN remains unsupported by this LV1 timing backend.
  [HEN support notes](https://github.com/Gabarsolon/ps3-240p/blob/v1.5.0-dev19/docs/HEN_SUPPORT.md).

## Install

1. Exit CRT Toolset and install **CRT-Toolset-1.5.0-dev19.pkg** through XMB
   Package Manager. It updates title ID `PS240P001`; its package version is
   **01.68** and its menu says **v1.5.0-dev19**.
2. Reboot once after upgrading to clear the previous resident helper.
3. Use HDMI and select **1080i** in the PS3's video output settings. Start
   from 59.94/60 Hz, with SD 240p/288p enforcement disabled. If old SD timing
   edits are active, disable enforcement and reboot before this HD test.
4. Keep a way to return to stock 1080i. The timer runs in the resident helper
   and depends on a functioning console/OS; it cannot guarantee recovery
   from a whole-console freeze.

## Test sequence after reboot

Open **HD CRT / experimental 540p**. Press Cross on the following in order,
reopening CRT Toolset and this submenu after each return to XMB:

1. **Test stock 720p / 1080i round trip (exit to XMB)**.
2. **Test AVI packet only (stock 720p; 15 seconds)**.
3. **Test 540p with custom AVI packet (15 seconds)**.

**Skip “Test AVI packet only (stock 1080i; 15 seconds)”** for normal testing;
that separate diagnostic produced a black picture on the tested receiver.

The first two checks are required once per resident-helper session. After
they pass, repeat only the 540p option until the next reboot/helper restart.
An error invalidates those prerequisites. This prerelease does not combine
all three steps into one button.

Each timing action exits the app normally before changing the display.
Leave the controller untouched during the test and return. The candidate
holds for **15 seconds after native readiness**, then automatically restores
stock timing and returns through 720p to the saved 1080i mode. Mode changes
and receiver resynchronization add time before and after the hold; allow
roughly 30–40 seconds for the entire candidate sequence. A few seconds of
black while the receiver reacquires sync were observed in the app-menu test,
so 15 seconds of active timing is not a promise of 15 seconds of visible lock.

Check that the TV locks to the candidate, the entire XMB fits, and the picture
returns normally to 1080i. The test **always restores**; Cross does not keep
540p permanently.

## Feedback requested from HD CRT testers

Please report the TV model, HDMI connection chain, starting resolution,
whether the candidate locks, whether the full picture fits, and whether
automatic 1080i recovery succeeds. A photo/video or the TV's signal-status
display is useful. Attach these logs if possible:

- `/dev_hdd0/tmp/vsh_240p.log`
- `/dev_hdd0/tmp/ps3_240p.log`

Component users should report their connection first: this HD experiment
currently refuses component, so this package is not a component 540p test.

## Verification and changes

**PROVEN (live regression):** dev19 fixes an app-to-XMB row-pitch mismatch
that made dev18 abort early and falsely report failed cleanup. The app's
7680-byte pitch could be replaced by XMB's 8192-byte pitch during switching.
The check now requires current VSH and AV-source pitch to agree, then compares
that with the hardware pitch. The waveform and 15-second hold are unchanged.
Actual app-menu test sequence 280557604 held for 15.219765 seconds and ended
with DONE/detail 0. The owner's repeat, sequence 458662207, held for
15.453232 seconds and also completed with error 0 and stock recovery.
Evidence: `docs/HD540_DEV19.md` and ignored owner-test logs.

**PROVEN (offline verification):** host regression/failure-recovery tests,
private firmware fingerprints, compiled helper loader contract, self-authored
wrapper emulation and package-member checks passed. Normal stable release
artifacts were preserved. The normal build still excludes the experimental
HD menu by default; source builds use `--experimental-hd540` for both helper
and app. This prerelease is not marked as GitHub's latest stable release.

## Download integrity

`CRT-Toolset-1.5.0-dev19.pkg` — **308560 bytes**

SHA-256:

```text
50ca756c9dbbc1fe74a97e2d5fb120259163ab9b6b49a62860cb4489c5dde4d2
```

**PROVEN (artifact identity):** this is the existing tested package, not a
newly rebuilt binary. `SHA256SUMS.txt` and `package_manifest.json` accompany
the download. Stable v1.4.3 remains available separately.
