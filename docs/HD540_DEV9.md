# v1.5.0-dev9: progressive 720p AVI-only control

Checkpoint: 2026-11-14. This is a diagnostic build, not working 540p support.

**PROVEN (prior evidence):** the dev8 AVI-only control entered its timed state
with stock 1080i raster and cached AVI code 0, held for 15.164216 seconds,
and restored stock 1080i / code 5 with no reported error. **CLAIMED (owner):**
the control produced no picture. See [dev8 live result](HD540_DEV8.md).

**INFERRED (test rationale):** repeating the AVI-only change on a stock
progressive mode can help distinguish an interlaced-input limitation from
a broader AVI/transmitter problem. A visible 720p result alone would not
prove the existing 540p raster or identify the exact failure mechanism.

**PROVEN (implementation):** new action 6, `HD540_HDMI_720_CONTROL`, requires
the existing successful stock round trip. Starting from audited HDMI
1080i, the helper waits for normal app exit and a stable XMB, fingerprints
the original code/templates and stock timing records, then changes the same
single AVI-builder RAM instruction used in dev8. It does this **before**
requesting stock 720p so the native transition generates a fresh packet;
it does not rely on a same-mode request rebuilding that packet.
Source: `plugin/hd540_service.c`, `plugin/vsh_hd540.c`,
`app/source/hd540_hdmi.c`.

**PROVEN (readiness and recovery):** the active hold requires native
progressive 720p, an independently read AV head with 1280x720 active,
720 active / 750 total lines, and validated cached AVI code 0. The hold
lasts 15 seconds from readiness. It makes no raster/clock/MMIO/flash writes.
Recovery restores original RAM code first, requests stock 720p and then the
exact saved 1080i mode, and requires restored code 5. Existing bounded video
recovery, RAM-only retry and error latching remain in force. A successful
720p control does not satisfy the separate 1080i-control prerequisite for
the combined candidate. Protocol HD58 / `crt_hd540.v8.ready` refuses older
resident helpers. Sources: the implementation files above and
`app/source/hd540_protocol.h`.

**PROVEN (PC verification):** the host suite and 24 startup scenarios pass,
as do the private 4.93 code/template/timing fingerprints. New tests cover
patch-before-transition ordering, no same-mode submission on entry, exact
15-second hold, mandatory stock-roundtrip prerequisite, refusal of a wrong
720p head or unchanged AVI packet, failed early write with recovery retries,
missing readiness recovery and unknown patch-kind refusal. The compiled
helper passes the loader/thread-exit contract; package members decrypt and
match staging. Sources: `tests/test_hd540_service.c`,
`tests/test_vsh_hd540.c`, `tools/test_app.py`, `tests/test_plugin.py` and
the package builder's acceptance checks.

**PROVEN (artifact):**
`build/hd540/deliverables/CRT-Toolset-1.5.0-dev9.pkg`, SFO 01.58,
303856 bytes, SHA-256
`dc5db47d9602879143f43ebe9b2e6764ccaaa19fef55bdb83d280ad707cd7de1`.
The stable release PKG and helper retain their original hashes.
Evidence: `build/hd540/deliverables/package_manifest.json` and local hashes.

**PROVEN (deployment):** after renewed owner authorization, the app, SFO
and both helper copies were installed at idle XMB. Preflight required the
known dev8 hashes, no app-active flag and disarmed SD enforcement. Each
file was backed up, staged, read back, renamed and read back again. Evidence:
ignored `local_only/hd540/dev9/deployment.json` and `console-backup/`.
No app launch, reboot or video-mode request was sent during deployment.

## Live result

**PROVEN (logs, 2026-11-14):** the stock round trip reached DONE without
error. The app then published action 6, sequence 106244143, and exited
normally. The helper applied the AVI-builder change before entering stock
1280x720, verified 720 active / 750 total lines and AVI code 0, entered ACTIVE
at 114263040 microseconds, and began restoration at 129735008: the observed
hold lasted **15.471968 seconds**. Recovery returned stock mode 10 / AVI
code 5 and DONE state 7, detail 0, at 136608294. Evidence: ignored
`local_only/hd540/dev9/live/20261114-130755/app.log`, `helper.log`, `reply.bin`.

**CLAIMED (owner):** the XMB picture was visible and readable during this
720p control. The owner also supplied capture-status reports at 11:07:07
and 11:07:10 UTC showing 1280x720, signal present, progressive flag `0x07`;
at 11:07:15 and 11:07:18 they showed 1920x540, signal present, interlaced
flag `0x0f`. This agrees with the native transition/recovery log.

**INFERRED:** clearing the AVI video code is compatible with stock 720p
on this tested PS3/card setup. The prior 1080i control's black picture does
not demonstrate a general rejection of AVI code 0. Its failure could depend
on interlaced signalling or another mode-specific transmitter configuration.

**UNKNOWN:** why the 1080i AVI-only control loses picture, the exact packets
on the wire, and whether the candidate raster produces progressive 540p.
The successful 720p control does not prove those raster changes. Future
candidate gating should use the now visually validated progressive control
and stock recovery, rather than require a visible interlaced AVI-zero
picture. That change is not present in dev9.

Control procedure used after installing the matching app/helper:

1. Reboot to remove the old resident helper; retain stock HDMI 1080i.
2. Open CRT Toolset v1.5.0-dev9, enter HD CRT experiments and run
   **Test stock 720p / 1080i round trip**. The app exits normally.
3. Once the picture returns, reopen the app and select
   **Test AVI packet only (stock 720p; 15 seconds)**.
4. Leave the controller untouched and observe the picture and automatic
   return. Record software logs alongside the visual result. Do not treat
   software completion alone as picture validation.

**CLAIMED (upstream capture-status mapping):** a one-shot card query reports
`0x07` for progressive and `0x0f` for interlaced input. It may disturb capture,
so the first visual control should run without live-register polling. See
[Capture input status](CAPTURE_INPUT_STATUS.md) for the source and limits.
