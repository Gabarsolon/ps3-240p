# PS1 audio and crop report after the PID compatibility fix

Checkpoint: 2026-11-14. External messages and app log relayed by the owner.
Reporter identity, console and audio have not been independently measured.

## Observations

- **CLAIMED (tester):** v1.4.4-rc1 works in the app and XMB at 240p, including
  the timed recovery and background corrector, on the previously reported
  PAL CECH-2004A / Evilnat 4.93 CEX + Cobra 8.5 configuration.
- **CLAIMED (tester):** Toy Story 2 and Battle Hunter have a usable picture
  with some zoom/crop, but lower audio pitch and slight crackling only in
  PS1 games while 240p is active.
- **CLAIMED (follow-up):** both games are NTSC versions. The initial
  PAL-game/50-Hz hypothesis therefore does not fit this report as stated.
- **CLAIMED (later follow-ups):** the same games have normal audio at stock
  480i. Audio is analog AV Multi-Out to a mono CRT using yellow video and
  white audio, not HDMI or optical. This satisfies the first stock comparison
  below as a tester report; it has not been independently recorded.
- **PROVEN (supplied app log):** discovery finds one supported identity at
  PID 9, resolves all 54 timing targets across 109 pages, applies the NTSC
  transaction, selects public resolution ID 4 and observes analog-head VID
  `0x10`, 720 x 480 source, 480-active / 522-total software-cache geometry.
  It reuses the resident corrector targeting resolution 4 and exits normally.
  Sources: supplied log timestamps `465778..487330`; this is software
  evidence, not an electrical timing or audio measurement.

## What this does and does not establish

**PROVEN (source comparison):** the PID compatibility change replaces the
fixed service lookup; it does not change `timing_payloads.h` or add audio
patches. Sources: commit `79a3835`, `app/source/sysmgr.c`,
`app/source/lv1_patch.c:service_pages`. A successful lookup does not exclude
an existing timing/emulator interaction on newly supported hardware.

**PROVEN (log scope):** the provided log covers app activity, not the later
game's emulator choice, video transitions, audio configuration, sample rate
or playback pitch. It begins at 576i/50 and switches to NTSC output, but
that does not prove the game itself ran at a PAL cadence.

**UNKNOWN:** whether the crop is source scaling, normal CRT overscan or
another geometry difference. Do not advertise an integer-scaled or perfectly
fitted PS1 picture on this evidence alone.

**UNKNOWN:** the audio cause. Possible investigation areas are the chosen
PS1 emulator, its output mode/region handling, video-cadence/audio pacing,
and the active audio route. PAL console region alone is not an established
cause. No fix or audio-clock write is justified by the app log.

## Next discriminating checks

These are proposed checks (**INFERRED**), not completed tester results:

1. Compare the same NTSC game in stock 480i/60 and 240p/60, keeping the
   emulator and audio path identical. Stock 576i/50 is not the matched
   control for this comparison. **CLAIMED:** the tester has now confirmed
   normal audio in stock 480i; do not repeatedly request this same check.
2. Record whether the emulator is `ps1_emu` or `ps1_netemu`, launch method,
   exact game region/release and audio path. **CLAIMED:** NTSC versions and
   analog white-RCA audio to a mono CRT are now known; emulator choice and
   exact releases remain unreported.
3. Collect the helper log during game launch and inspect game-time video and
   audio readback. Distinguish an actual slower emulation cadence from a
   sample-rate/pitch problem and buffer underruns. Objective audio comparison
   needs a separate console/capture-card audio diagnostic, not microphone
   recording; the HDMI picture-test captures remain audio-disabled.
4. Trace the timing and audio paths offline before changing PLL, sample-rate,
   encoder or emulator state. Nominal metadata alone cannot measure physical
   cadence or identify which clock drives audio on this setup.

The PID compatibility result is recorded in
[the model report](CECH2004A_LOOKUP_REPORT.md). Audio/crop compatibility
remains open; keep the build a prerelease.

## Owner-console investigation (2026-11-14 evening)

**CLAIMED (owner, subsequent CRT test):** both Toy Story 2 and Battle Hunter
have normal audio during 240p playback on the owner's CRT setup. Source:
owner message, 2026-11-14, "I tested those games and the audio's fine."
This is the first supplied owner result on the analog low-line path,
separate from the earlier HDMI recordings below. Exact physical clock and
end-to-end audio latency were not measured. **INFERRED:** the failure is not
universal across the two reported setups. **UNKNOWN:** whether console model,
emulator, game image, launch history or another setting explains the difference.

The next build adds bounded game-time logging, retaining the working timing
recipe. See [v1.4.4-rc2 diagnostic instructions](RELEASE_v1.4.4-rc2.md).

**PROVEN (read-only disc inspection):** the installed images in
`/dev_hdd0/PSXISO` identify these boot executables and audio assets:

| Image | Boot ID | Checked audio-related content |
|---|---|---|
| Toy Story 2 USA | `SLUS_008.93` | XA streams and SPU `.VB/.VH` banks; one data track |
| Battle Hunter USA | `SLUS_013.35` | SPU `.SEQ/.VB/.VH` assets and four AUDIO tracks in its CUE |
| Mega Man X4 USA | `SLUS_005.61` | XA music/voice streams; one data track |
| Gran Turismo 2 plus GT Mode | `SCES_123.80` | Installed image identifies itself as a PAL modified release |

**PROVEN (sampled sector subheaders):** the first 128 sectors checked from
each of Toy Story 2's first three `.XA` files and Mega Man X4's first three
music `.XA` files use coding byte `0x01`, audio submode `0x64`, and channels
0..7. **INFERRED:** there is no obvious XA-coding distinction in those
samples that explains the report. This does not compare every track or
exclude a game/emulator interaction. The installed PAL GT2 modification is
not a matched NTSC control. Evidence: private
`local_only/ps1_audio/discs-20261114-202404/report.json` and `xa-headers.json`.
No full image was copied or modified.

**PROVEN (live process listing, OBS frames and USB audio recordings):** both
new games launch through webMAN under `main_ps1_emu.self` on the owner's
current HDMI setup. Battle Hunter reaches its menu with non-silent stereo
audio; Toy Story 2 reaches gameplay with non-silent audio. **UNKNOWN:**
240p analog pitch on these runs. HDMI playback is not a substitute for the
tester's analog path. Private evidence: `live-20261114-202630`,
`live-20261114-205559`, and
`local_only/display_presets/public-diagnostic-ui/capture-20261114-210340`.

### Hardware comparison and limits

**PROVEN (private read-only system report):** the owner's reported product
subcode is `0x0B`. **INFERRED:** this identifies the CECH-25xx family; the
exact sticker SKU and motherboard revision have not been independently
verified. No console identifiers are included in public artifacts.

**CLAIMED (hardware investigators' tables):** the relevant family differences
are as follows; they are not measurements of either individual console.

| Family | Motherboard family | Cell / RSX process | MultiAV family |
|---|---|---|---|
| CECH-20xx, including reported CECH-2004A | DYN-001 | 45 nm / 65 nm | CXM4027R |
| CECH-25xx | JTP-001 / JSD-001 | 45 nm / 40 nm | CXM4027R |

Sources: [chassis/product codes](https://www.psdevwiki.com/ps3/Chassis_ID),
[board revisions](https://www.psdevwiki.com/ps3/Motherboard_Revisions),
[RSX inventory](https://www.psdevwiki.com/ps3/RSX), and
[MultiAV inventory](https://www.psdevwiki.com/ps3/MultiAV), whose table lists
both DYN-001 and JTP/JSD-001 with CXM4027R.
**UNKNOWN:** whether silicon revision, clock routing, encoder revision or
another model-dependent difference contributes to this bug. Shared chip
family does not establish identical behavior; different RSX manufacturing
process does not establish an audio cause.

### Capture control and remaining gap

**PROVEN (live timed stock control):** a separate helper selected stock
analog 480i for 15 seconds and restored the exact saved native HDMI tuple.
It changed no low-line timing, audio setting or saved video preference.
HDMI picture disappeared during the analog interval and returned after
restoration; the USB audio recording during that interval contained only
near-silent quantization/noise (roughly `1.46e-5` full-scale AC RMS).
Evidence: private `stock-control-20261114-204045/{report.json,probe.log,audio-quality.json}`.
The control's source, safeguards and host tests are in
`experiments/ps1_audio/` and `tests/test_ps1_audio_probe.c`.

**INFERRED limitation:** the currently connected HDMI-only capture path
cannot directly measure the reported white-RCA/analog-DAC failure. This is
not evidence that analog audio is silent or defective. An analog line-level
recording or an independently validated equivalent measurement is still
needed to verify an analog fix; no microphone recording was made.

### Firmware observations and diagnostics

**PROVEN (private 4.93 source inspection):** PS1 audio initializer
`0x87c90..0x87e8c` opens a two-channel, eight-block PCM ring. Context pointer
slot `0x17c348` leads to the cached port configuration and ring addresses.
It obtains the configuration before starting playback, so its cached READY
status is not a fresh statement that the port is stopped. The full
initializer SHA-256 is
`58aba84d826ead0d60c015ab83600fe5be13e83db0eaca2901da70e089cf16e0`.
Evidence: private `offline/emu_0x87c90.{c,asm}` and the original
`local_only/ps1ps2/ps1_emu_493.elf`.

**PROVEN (owner-console readback):** the initializer fingerprint matches
and bounded process reads retrieve changing stereo float PCM from both
games. `tools/ps1_audio_snapshot.py` supports these read-only diagnostics;
the optional PS3MAPI TCP server is disabled on this console, and its HTTP
fallback is slow. **PROVEN (timed reads):** an HTTP ring read takes about
1.9 seconds, exceeding the nominal 42.7-ms ring period at 48 kHz. Equal
consumer indices can hide complete wraps. These non-atomic snapshots must
not be concatenated into a purported recording or used as pitch proof.
Private evidence: `ring-20261114-204658`, `ring-20261114-205600`.

**PROVEN (subsequent resident collection):** the isolated v2 helper captures
64 independent PCM snippets locally before FTP transfer, avoiding the slow
HTTP scan. Toy Story 2's maximum process-read duration was 236 microseconds;
all 64 records had an unchanged consumer index. Its reader validates every
record's sequence, process identity, timestamps, cursor bounds and finite
PCM. **UNKNOWN:** atomicity against the producer and analog playback pitch.
These are independent/overlapping ring snippets, not a continuous recording.
Source, commands, host checks and limitations:
[audio diagnostic tools](../experiments/ps1_audio/README.md).
Private evidence: `resident-20261114-210710`; checked helper SHA-256
`e3f69f171a932821957acc19bd71833baf9a8cd6561aa7f2f9dfe98205c726be`.

**PROVEN (private AV-service trace):** normal audio-mode handler
`0x8000c988` calls `0x80004b60`; the latter distinguishes sample-rate enums
and reaches audio setup `0x80004918`. The timing payload does not directly
write those audio configuration fields. **UNKNOWN:** an indirect clock or
emulation-pacing interaction. Register `0x37`'s exact electrical role is
still not independently established; do not equate our template bit change
with a demonstrated analog sample-clock change.

**PROVEN (payload arithmetic), INFERRED (nominal cadence):** the NTSC
metadata float changes by about one part per million. Using the existing
nominal 54-MHz sample clock and the checked raster totals gives roughly
59.940 to 60.005 fields/s, an increase of about 0.108%. This calculation is
not a physical clock measurement and does not itself explain a large
downward pitch shift. Source: `app/source/timing_payloads.h`.

**Current result: UNKNOWN cause; no verified fix.** The next comparison must
keep the exact game, emulator, audio route and input sequence fixed while
comparing stock 480i and 240p. Separate pre-DAC PCM, actual analog playback,
game/video cadence and stock restoration. Preserve the existing known
working timing recipe until a discriminating test justifies changing it.

**PROVEN (cleanup):** normal soft reboot removed the temporary sampler;
only webMAN remained in VSH slot 1. The original empty enforcement flag was
restored, and hashes of the installed app, SD helpers and boot-plugin list
matched the initial inventory. XMB/HDMI picture returned. Evidence: private
`cleanup-20261114-211454/report.json` and `xmb.jpg`.
