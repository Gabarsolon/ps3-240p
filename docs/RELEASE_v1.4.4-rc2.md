# v1.4.4-rc2: PS1 audio diagnostic

**Purpose:** collect game-time evidence for the CECH-2004A report of lowered
pitch and crackling in 240p. **CLAIMED (owner, 2026-11-14):** Toy Story 2 and
Battle Hunter sound normal in 240p on the owner's CRT setup. The external
tester reports normal 480i audio but bad 240p audio. The difference is still
**UNKNOWN**; this build is not an audio fix or a model-compatibility claim.
See [investigation and observations](PS1_AUDIO_REPORT.md).

## What changes

**PROVEN (source):** the SD helper can build with an additional read-only
observer. It records PS1 emulator identity, native VSH video state, enforcement
and app flags, and fingerprinted `ps1_emu` cached audio-port configuration.
The existing `vsh_240p.log` independently records correction attempts. Timing
payloads, PID discovery, gamma and correction policy are unchanged.
Sources: `plugin/{ps1_audio_log.c,vsh_ps1_audio_log.c,vsh_240p.c}` and build flags.

**PROVEN (implementation):** at most 120 snapshots, spaced at least five
seconds apart, go to a text file capped at 64 KiB. The observer stops reading
processes after its budget; it does not stop the background corrector. It
records no PCM, microphone input, game-file content or console identifiers.
It uses PS3MAPI process GET operations; no process-memory, LV1, audio-register
or saved-setting writes are added by the observer.

**PROVEN (guards):** the full 4.93 `ps1_emu` initializer fingerprint must match
before reading its known context. Pointer, channel, block, size, consumer and
writer bounds are checked, followed by a second context-pointer read.
`ps1_netemu` is identified but its unaudited audio layout is not inspected.
Unsupported layouts, process exit/reinitialization and errors appear in the
log rather than triggering an audio/video change.

**UNKNOWN:** physical DAC sample rate, pitch and end-to-end latency. The port
status is cached from initialization; READY does not establish stopped audio.
Consumer samples can hide complete ring wraps and are not a sample-rate
measurement. Native VSH state is software readback, not a waveform capture.
`result=0` means bounded config reading succeeded, not that playback is correct.

## Tester steps

1. Install `CRT-Toolset-1.4.4-rc2.pkg`, then reboot normally once to remove
   the older resident helper. Decline an optional boot filesystem check if
   offered. This uses the same title ID as CRT Toolset and replaces its app;
   it is SD-only, with no experimental HDMI menu.
2. Open CRT Toolset and check that its menu says **1.4.4-rc2**. Enable **240p
   background enforcement**, exit normally, and launch the same NTSC game
   through the same launcher/emulator that previously had bad sound.
3. Within the first ten minutes, play a recognizable section for roughly
   30–60 seconds. Note whether the pitch/crackle is still wrong and which
   game/section this was. Exit the game normally to XMB.
4. Before rebooting or reopening CRT Toolset, copy these files over FTP:

   - `/dev_hdd0/tmp/ps1_audio_diagnostic.log`
   - `/dev_hdd0/tmp/vsh_240p.log`
   - `/dev_hdd0/tmp/ps3_240p.log`

5. Send those text logs to the owner with the game title/release or disc ID
   and launch method. NTSC region, white-RCA/mono-TV audio and normal 480i
   playback are already reported; no need to repeat those details unless
   something changed. A second game is optional; preserve the first logs.

The observer opens its log once at helper startup, and a later helper load
overwrites it. Reopening the app overwrites the app log. A missing diagnostic
file or missing `audio_diagnostic_v1` in `vsh_240p.log` means the diagnostic
helper has not been confirmed; it does not establish normal audio.

## Decode and limitations

**PROVEN (log format):** timestamps are monotonic microseconds; `emu=0/1/2`
means no matching process / `ps1_emu` / `ps1_netemu`. `rc=1` is no emulator,
`rc=2` is an identified unsupported layout, and negative values describe a
failed discovery, fingerprint, layout or race check. Audio fields are valid
only at `rc=0`. `native` holds VSH resolution / scan / conversion / aspect
bytes; these are a different namespace from the app's public video enums.
Source: `plugin/ps1_audio_log.c` and log header.

**INFERRED next decision:** compare emulator identity, launch-time video
changes and correction events with the owner's successful case. If these
agree, obtain a suitable analog line-level recording or another validated
clock/cadence measurement before changing audio configuration. The observer
itself changes scheduling slightly; disappearance of a symptom with logging
alone is not a verified fix.

## Build and validation

```powershell
python -B tools/build_plugin.py --audio-diagnostic
python -B tools/build_ps3_app.py --audio-diagnostic
python -B tests/test_plugin.py build/audio-diagnostic/plugin/vsh_240p.elf
python -B tools/test_app.py --firmware <private-sysmgr_ss.elf>
```

Artifacts are isolated in `build/audio-diagnostic/`; stable and other
development packages are preserved. The package builder decrypts and checks
every packaged member against staging. Runtime behavior on the affected
CECH-2004A remains **UNKNOWN** until the tester returns the logs.

**PROVEN (validation):** the full host suite, private 4.93 timing/identity
checks, signed helper loader contract and all five packaged-member checks
pass. The focused observer test covers interval/sample caps, cached READY,
unknown initializer refusal, invalid pointers/layouts/indices, a changing
context pointer, ambiguous processes, unsupported `ps1_netemu` and absence
of PCM-buffer reads. Sources: `tools/test_app.py`,
`tests/test_ps1_audio_log.c`, `tests/test_plugin.py` and the package builder.

**PROVEN (owner-console logger smoke):** the same observer and VSH adapter
ran in an isolated read-only harness alongside the original SD helper.
Battle Hunter launched under `ps1_emu`; six samples reported valid,
fingerprinted stereo/eight-block config with changing consumer indices.
The game exited normally and the observer logged its stop. Ordinary plugin
unload did not clear its slot; a normal webMAN soft reboot removed it after
the game exited. No experimental mode or audio setting was submitted.
This validates observer execution, not the complete diagnostic PKG menu
or the external console. Evidence stays private in
`local_only/ps1_audio/log-smoke-20261114-220141/`.

**PROVEN (artifact):** package version `01.45`, 292528 bytes, SHA-256
`9ec4bfb7325f68f57c653cb6ae5249754b3d4468fa24eb7e52921a85941b7779`.
The displayed version is `1.4.4-rc2`. The stable release remains unchanged.
