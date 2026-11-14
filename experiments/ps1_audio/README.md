# PS1 audio investigation tools

**PROVEN:** these original tools build and the stock-output control and
resident PCM sampler run on the owner's supported 4.93 CFW console.
**UNKNOWN:** cause and fix for the reported CECH-2004A analog pitch/crackle.
See [the investigation](../../docs/PS1_AUDIO_REPORT.md) for evidence and limits.
This is a developer diagnostic, not a new release or an audio correction.

## Build and verification

```powershell
python -B experiments/ps1_audio/build.py
python -B tests/test_plugin.py build/ps1-audio/plugin/crt_ps1_audio.elf --module-name crt_ps1_audio
python -B tests/test_ps1_audio_snapshot.py
python -B tests/test_ps1_audio_collect.py
```

**PROVEN (source):** the build is isolated under `build/ps1-audio/` and does
not replace release app/helper artifacts. The host state-machine regression
test is `tests/test_ps1_audio_probe.c`; compile it with the host C compiler,
not the PPU compiler, then run the resulting executable.

## Read-only collection

**PROVEN (implementation):** stage and hash-check the helper at
`/dev_hdd0/tmp/crt_ps1_audio.sprx`, then load it into an independently empty
Cobra VSH slot. Never replace another plugin or add it to boot plugins.
Require live module name `crt_ps1_audio`, matching staged bytes, fresh log
marker `ps1_audio_control_v2` and `.v2.ready`. A stale ready file alone is
insufficient. The helper is intended for this CFW research environment;
actual HEN compatibility remains **UNKNOWN**.

With an owned game running in the fingerprinted `ps1_emu`:

```powershell
python -B tools/ps1_audio_collect.py --ip 192.168.0.169
```

**PROVEN (source):** the client sends collection action 4 only. The helper
enumerates processes, verifies the complete audio initializer fingerprint,
validates the two-channel/eight-block ring, then saves 64 independent
snapshots to a sequence-specific HDD file. It does not change video/audio
settings, write process memory, invoke LV1, install hooks or record inputs.
Raw PCM and reports are downloaded into ignored `local_only/ps1_audio/`.

**PROVEN (live check):** Toy Story 2 collection returned all 64 records,
with maximum process-read duration 236 microseconds and an unchanged
consumer index during each read. Evidence: private
`resident-20261114-210710`. This does not establish atomicity against the
producer, continuous audio, physical output frequency or correct pitch.
Do not concatenate these overlapping snippets into a purported recording.

For a slower diagnostic without loading the helper:

```powershell
python -B tools/ps1_audio_snapshot.py --ip 192.168.0.169 --transport http --count 2
```

**PROVEN:** HTTP reads on this console exceed the ring's nominal turnover
period and mix data from different moments. They are useful for identifying
the buffer and checking bounded PCM, not waveform or pitch analysis.
The optional TCP reader is implemented; its server was disabled on this
console, so its actual end-to-end behavior remains **UNKNOWN**.

## Stock analog capture control

**PROVEN (implementation):** the helper's separate action 2 is a developer
display test. It requires idle, single-output, stock HDMI 1080p/60 with the
fingerprinted XMB buffer format/pitch, no app and no SD enforcement flag.
It selects stock composite 480i for 15 seconds, then submits and verifies
the exact saved native display tuple. Cancellation restores early;
unconfirmed restoration latches failure. It changes no saved preference
or low-line timing. The public collection client never sends this action.

**PROVEN (owner test):** the HDMI picture and usable USB audio disappear
during this analog interval; HDMI returns after automatic restoration.
Consequently this HDMI capture setup cannot validate the reporter's white
RCA analog audio. See the investigation for the recorded noise measurements.

**INFERRED next comparison:** use the same exact game/input sequence and
emulator at stock 480i and 240p, collecting pre-DAC snippets plus actual
analog line audio and game/video cadence. Keep CFW/model, audio multi-output,
boot region and background-correction events in the report. A successful
collection on HDMI is not a 240p analog compatibility result.
