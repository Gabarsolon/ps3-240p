# Automated HD diagnostics on the owner's console

**PROVEN (implementation):** `tools/hd540_auto_test.py` drives the existing
guarded helper protocol directly from XMB, records OBS Virtual Camera frames
and helper state, and stops the sequence after a failed control. It does not
need app-menu navigation. The default tests are the stock round trip and
stock-progressive-720p AVI-zero control. `--candidate` adds the current
experimental candidate after both controls pass.

```powershell
python tools/hd540_auto_test.py
python tools/hd540_auto_test.py --candidate
```

Keep OBS Virtual Camera running with the capture-card scene. The runner uses
the owner's 2560x1440/60 NV12 virtual-camera configuration, records 640x360
JPEG frames at four per second, and saves state samples, logs and results
under ignored `local_only/hd540/automatic/<timestamp>/`. It captures no audio.
Dependencies: FFmpeg, Pillow and NumPy. This is an owner-authorized live test,
not an offline simulator.

**PROVEN (guards):** the runner checks disarmed SD enforcement, absent
app-active flag, matching installed helper hashes, live VSH slot identity
and matching ABI marker. It stages/readbacks each complete request and
preserves the previously acknowledged inbox. It does not replace an
unacknowledged inbox for a new test. On interruption/timeout it requests
cancellation, waits for bounded recovery, and reports unconfirmed recovery
separately. It never sends arbitrary memory-write commands or installers.
Sources: `tools/hd540_auto_test.py`, `app/source/hd540_protocol.h` and the
existing helper service.

**PROVEN (bootstrap finding):** the owner's `boot_plugins.txt` lists only
webMAN. After restart, the live VSH list showed slot 1 WWWD and slots 2..6
empty, although old project ready/reply/log files remained on disk. A stale
ready file cannot prove a live helper. Evidence: ignored
`local_only/hd540/automatic/vsh-plugins.html` and the FTP boot-list read.

**PROVEN (bootstrap implementation):** the runner parses the live VSH slot
page and, when the project helper is absent, loads the hash-checked
`/dev_hdd0/plugins/vsh_240p.sprx` into an empty slot 2..6. It never unloads an
occupied slot or edits boot-plugin lists. Fresh startup logs, the ready file
and live slot must agree before tests. The helper's own startup discards
stale HD requests/replies before polling.

**CLAIMED (webMAN documentation) / PROVEN (upstream source):**
`/vshplugin.ps3mapi` lists/loads VSH plugins; its `load_slot`/`prx` parameters
issue the Cobra VSH plugin-load call. `/reboot.ps3?vsh` requests a normal VSH
restart. Sources: [webMAN commands](https://github.com/aldostools/webMAN-MOD/wiki/Web-Commands),
[plugin handler](https://github.com/aldostools/webMAN-MOD/blob/master/include/ps3mapi/ps3mapi.h)
and [restart handler](https://github.com/aldostools/webMAN-MOD/blob/master/include/cmd/restart.h).
`--reboot` requests that restart before controls, then verifies a new worker
startup or that the previous resident helper disappeared and is reloaded.
HTTP success alone is not restart proof. Initial runs that relied on an
unchanged ready file failed closed; their reports are preserved privately.

**PROVEN (offline checks):** ten focused tests cover black/flat versus
textured pixel classification, mature active samples, acknowledged inbox
preservation, busy/unacknowledged refusal, bad staging, uncertain rename
recovery arming, partial replies, latched recovery failure, slot parsing,
stale ready-file refusal and occupied-slot protection. Run:
`python tests/test_hd540_auto.py`.

**UNKNOWN / interpretation limits:** pixel tests detect a sufficiently lit,
spatially varying image; they do not prove readability, correct crop/scaling,
frame pacing, integer scaling, physical scan type or HDMI refresh. A frozen
or unrelated textured image may satisfy them. Active picture checks skip the
first two seconds of the helper's active state; a control needs at least
three later samples and at least 80% positive pixel checks. Recovery needs
DONE/detail 0 plus six of eight positive post-return samples. Inspect saved
frames when judging geometry. No live capture-register polling is performed,
so such reads cannot disturb these undisturbed picture tests.

## Completed live automatic run

**PROVEN (2026-11-14):** the corrected runner loaded the verified helper into
empty slot 2, verified fresh startup, and ran all three tests from XMB with
OBS Virtual Camera recording. Stock roundtrip sequence 4050358996 passed;
720p control sequence 366547977 had 22 mature active samples, all positive;
candidate sequence 82426074 had 22 mature active samples, all negative.
All three returned DONE/detail 0 and passed the post-return picture check.
The runner reported candidate failure and stopped, rather than equating
software completion with picture success. Evidence: ignored
`local_only/hd540/automatic/20261114-133801/report.json`, individual test
JSON/logs and JPEG frames. A representative 720p frame was visibly readable;
the inspected candidate frame was flat black. This confirms the negative
picture result without live HID-register reads disturbing the test.

**PROVEN (bootstrap diagnosis):** normal remote restart attempts left only
webMAN resident; the old project ready/reply/log files were still present.
The initial runner incorrectly relied on those disk files and could not
confirm a live helper. It now queries live slots and reloads the matching
helper when absent. Initial failed/stopped reports are retained as evidence,
not classified as successful video tests.

**UNKNOWN:** a fully completed run with the optional `--reboot` argument
after the bootstrap correction; the last such foreground run was interrupted.
The completed background run above verified automatic loading and all video
test/recovery paths without another restart. The restart request, plugin
disappearance and subsequent verified helper load were independently observed.
