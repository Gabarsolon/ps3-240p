# Experimental HDMI 540p candidate: v1.5.0-dev1

Checkpoint: 2026-11-14. **PROVEN (source/build):** an opt-in prototype now
builds as a separate package. **UNKNOWN:** whether it generates a correct
540p waveform, whether the capture card accepts that waveform, and whether
an HD CRT will display it correctly. This is not a released 540p feature.

## First experiment

**PROVEN (implementation):** the app adds **HD CRT / experimental 540p**
only when built with `CRT_ENABLE_HD540`. The normal build remains v1.4.3.
The submenu contains an inspection action and a timed test; there is no
keep, boot-default or background-enforcement action for HD.
Sources: [app entry point](../app/source/main.c),
[build configuration](../app/CMakeLists.txt).

**PROVEN (patch scope):** the candidate changes four ordinary-RAM words
across internal 1080i profiles 7 and 8. It preserves the 1920 x 1080 source,
field-fetch metadata, horizontal timing, clock selection and encoder data.
The proposed logical frame total is 1124 instead of 1125; the packed vertical
window uses 547/542 instead of 1094/1084. These are parameter hypotheses,
not a measured signal. Source: [hd540_patch.c](../app/source/hd540_patch.c).
The original record addresses and interpretation are documented in
[the feasibility investigation](HD_CRT_540P.md).

**INFERRED:** if the field timing behaves as hypothesized, a 562-line refresh
would be approximately 59.993 Hz with the rounded 74.175 MHz clock, or
60.053 Hz with 74.250 MHz. The prototype does not program a new PLL clock.
Source: [offline calculator](../tools/hd540_timings.py).

**PROVEN (guards):** the first build requires single-output HDMI 1080i at
59.94/60 Hz, stock supported 4.93 records and disabled SD enforcement.
It rejects analog/clone/50 Hz/progressive input configurations, unknown
records, invalid surfaces and old scaler edits. Mapping is checked again
before writing; unexpected third-party values are not overwritten.
Sources: [native backend](../plugin/vsh_hd540.c),
[RAM transaction](../app/source/hd540_patch.c).

**PROVEN (flow):** the helper snapshots the exact native display flags,
mode, format and pitch. It requests HDMI 720p, waits for matching native
state and a 1280 x 720 / 750-line AV head cache, then patches the inactive
1080i records and requests the exact saved native mode. This avoids
reselecting public resolution ID 1 and accidentally choosing 1080p.
Source: [native display interface](../plugin/vsh_video.c).

**PROVEN (recovery design):** the resident helper owns the transition
deadlines and six-second observation period. Recovery restores RAM first,
then transitions through 720p back to the saved 1080i mode and checks the
stock AV cache. Failed recovery remains armed and retries. Duplicate tests
cannot extend the timer, and the app blocks further timing/gamma actions
while restoration is unconfirmed.
Sources: [service state machine](../plugin/hd540_service.c),
[helper polling](../plugin/vsh_240p.c), [app](../app/source/main.c).
**UNKNOWN:** recovery on real hardware. The watchdog requires a functioning
VSH/OS; it cannot guarantee recovery from a whole-console freeze.

## Local validation

**PROVEN (host checks):** SD and gamma regression suites, four-word HD
transaction scope, dropped writes, exact rollback, changed mappings,
third-party mutations, stale requests, early cancellation, transition
timeouts, retry ordering and native HDMI guards pass. The actual app passes
23 startup/menu scenarios, including preserving selected 1080i on startup
and refusing an HD test from 1080p. Complete metadata/raster fingerprints
for both target profiles match the privately held 4.93 firmware.
Sources: [test runner](../tools/test_app.py),
[HD patch tests](../tests/test_hd540_patch.c),
[HD service tests](../tests/test_hd540_service.c),
[native guard tests](../tests/test_vsh_hd540.c),
[app tests](../tests/test_startup.c).

**PROVEN (compiled artifacts):** the experimental helper passes the
loader-thread termination and PRX export checks. The package builder
decrypts its final package and compares every packaged member with its
staged input. The original release package and tested helper are preserved.
Sources: [helper ABI check](../tests/test_plugin.py),
[package builder](../tools/build_ps3_app.py).

Build locally:

```powershell
python -B tools/test_app.py --firmware <private-path-to-4.93-sysmgr_ss.elf>
python -B tools/build_plugin.py --experimental-hd540
python -B tests/test_plugin.py build/hd540/plugin/vsh_240p.elf
python -B tools/build_ps3_app.py --experimental-hd540
```

**PROVEN (artifact):** the resulting package is
`build/hd540/deliverables/CRT-Toolset-1.5.0-dev1.pkg`, SFO version 01.50.
It updates the existing title ID; it is not a separate installed title.
Source: package manifest in that directory.

**PROVEN (final artifact and FTP readback):** the package is 299424 bytes,
SHA-256 `b95994c17df28056e47fee4c5d5358be4c33e623be28830cf7a067b601974e35`.
Its executable, SFO and matching staged/runtime helper were installed while
the owner was at XMB with the app-active marker absent. Every installed file
was read back byte-for-byte. Original files were retained locally and as
adjacent `.crt-hd540-dev1.prev` files on the console. No remote installer,
reboot, controller input, app launch, plugin load or timing-test request was
issued. This verifies files on disk, not execution of the new helper.
Evidence: ignored `local_only/hd540/dev1/deployment.json`, console backup
and final package manifest.

## Capture setup and first hardware steps

**PROVEN (local device inspection):** the connected capture device reports
UltraSemi USB3 Video, VID/PID `345F:2131`, and advertises 1920 x 1080 at
approximately 60 fps in YUY2 and MJPEG. A moving XMB was captured after the
owner selected 1080i. The short MJPEG sample had 61 distinct images among
120 decoded USB frames, so USB frame count is not proof of 60 distinct source
fields. The exact input timing has not been independently measured.
Evidence: ignored `local_only/hd540/capture-card-assessment.json` and
`local_only/hd540/capture-1080i-baseline/assessment.json`.
**CLAIMED (owner):** the console was set to 1080i for that capture.

**PROVEN (capture observation):** an immediate first-frame capture later
contained heavy colored noise while the console was still on stock XMB.
Captures taken after discarding 90 USB frames were visibly clear in both
MJPEG and YUY2. **UNKNOWN:** the noise's cause; use a settled recording
before and after a test so startup artifacts are not attributed to the patch.
Evidence: ignored `local_only/hd540/dev1/*settled.png` and
`xmb-before-update.png`. No experimental timing had been requested then.

**INFERRED (test procedure):**

1. Install the matching dev build, reboot once to clear old RAM edits and
   resident helpers, and keep HDMI 1080i selected. Do not enable the SD
   enforcer. Reopen the app manually and check for v1.5.0-dev1.
2. Open **HD CRT / experimental 540p**, choose **Inspect HDMI 1080i**, and
   stop if it refuses. This action must not change the output mode.
3. Start a settled capture, then choose the timed test once. Observe the
   transition, candidate interval and automatic return. Do not start games
   or change system output settings during the test.
4. Preserve `/dev_hdd0/tmp/ps3_240p.log`, `/dev_hdd0/tmp/vsh_240p.log`, the
   HD reply and capture. Confirm stock recovery before another experiment.
5. Analyze the actual signal and field phase before calling this 540p.
   HDMI receiver metadata may still describe the original 1080i mode.
   Component and an identified HD CRT need separate implementation and
   validation before giving a component tester a supported prerelease.

**UNKNOWN:** native gamma together with this HD candidate, component output,
HDMI information-frame correctness, measured field parity, game transitions,
HD CRT processing/latency and broad stability. The initial experiment changes
neither the analog encoder templates nor the HDMI metadata builder.
