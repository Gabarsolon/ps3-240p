# v1.5.0-dev6: 15-second candidate hold and stock recovery cycle

Checkpoint: 2026-11-14. [Dev5's candidate](HD540_DEV5.md) left OBS black
despite stock cache recovery. A separate stock 720p / 1080i round trip
restored the visible XMB. The owner requested a 15-second observation hold.

**PROVEN (implementation):** `HD540_TEST_SECONDS` in
`app/source/hd540_protocol.h` is 15, used by the helper's active timer.
The app's status/menu labels identify the 15-second candidate test.
This interval begins when native/candidate cache readiness passes;
entry and automatic restoration take additional time. Repeated requests
cannot extend the active timer, and cancellation can end it early.

**PROVEN (recovery flow):** after any attempted candidate patch, restore
timing RAM first. Only after successful restoration, request stock 720p,
wait for its native and AV cache readiness, and request the saved original
1080i mode. A failed/timed-out 720p check still sends that final original
request. Its wait is bounded to five seconds. Persistent return failure
latches recovery stopped instead of issuing repeated mode changes.
RAM restoration failure retries RAM only and issues no video commands.
Before any candidate patch, failed transitions retain direct original-mode
recovery with at most one alternate/final-original fallback cycle.
Evidence: `plugin/hd540_service.c`.

**PROVEN (validation):** the host regression verifies that recovery does
not begin before the full 15-second active deadline, duplicate commands
do not extend it, RAM is restored before the recovery alternate, the final
original request occurs even if alternate readiness times out, and repeated
cancels cannot restart a latched failure. Full app/SD/gamma/HD tests, all
24 startup/return scenarios, private firmware record checks, helper loader
checks and package member verification pass. Evidence:
`tests/test_hd540_service.c`, `tools/test_app.py`, `tests/test_plugin.py`
and the experimental build tools.

**PROVEN (artifact and deployment):** package
`build/hd540/deliverables/CRT-Toolset-1.5.0-dev6.pkg`, SFO 01.55,
301328 bytes, SHA-256
`7bf7018e5cf0d021af4b1d7aa6a314ad26f307cdf8b1de82da13af350f0dfc01`.
The app, SFO and both helper copies were installed at XMB with app/enforcer
flags absent, prior-version checks, reversible backups and byte readback.
No app launch or candidate request was sent during deployment. Evidence:
ignored `local_only/hd540/dev6/deployment.json`.

**PROVEN (helper gate):** HD55 / `crt_hd540.v5.ready` requires the matching
resident helper. Reboot once, open dev6 in HDMI 1080i, perform the stock
round trip, then select the 15-second candidate test. Leave the controller
untouched through automatic restoration.

**UNKNOWN:** dev6's live hold/recovery result, usable candidate output and
HD CRT compatibility. The candidate timing data is unchanged from the
black-capture attempt. Increasing its hold tests receiver acquisition time;
it does not establish that the timing is correct. Stable v1.4.3 artifacts
are unchanged.

## Live 15-second result

**PROVEN (console logs):** candidate sequence 188498577 reached state 3
near 198.415 seconds and began the stock recovery cycle near 213.725
seconds, a roughly 15.31-second hold including polling granularity.
After RAM restoration, stock 720p reached VID 9 / 720 active / 750 total,
then original 1080i passed the stock cache check. The final reply was
state 7 / detail 0. Evidence: ignored
`local_only/hd540/dev6/live/20261114-020111/` and
`local_only/hd540/dev6/live/20261114-020138-final/`.

**CLAIMED (owner):** the candidate remained black; the automatic recovery
worked. **PROVEN (preview):** a captured frame while recovery was still
in progress was black. Successful recovery does not establish a usable
candidate signal.

**UNKNOWN:** whether the capture receiver rejects otherwise valid timing,
the candidate waveform is invalid, or HDMI signaling remains inconsistent
with that waveform. A longer hold did not produce a reported picture.
Testing a second receiver can help, but another black screen alone cannot
distinguish a bad signal from two receivers lacking support.

## Direct monitor check

**CLAIMED (owner):** ordinary 1080i works with the PS3 connected directly
to the 1440p monitor. During the candidate hold, the monitor reports
"no signal"; XMB returns after automatic recovery.

**PROVEN (latest console logs):** candidate action 2, sequence 206937073,
passed stock 720p readback and patch application. Candidate cache readiness
entered state 3 at 216.841 seconds; recovery began at 232.204 seconds,
approximately 15.36 seconds later. Recovery reached stock 720p (VID 9,
720 active / 750 total), returned to original native mode 10 / interlaced
1080, and finished state 7 / detail 0. The same helper log also contains
an earlier candidate/recovery sequence with no reported service error.
Evidence: ignored `local_only/hd540/dev6/monitor/20261114-021133/`.

**INFERRED:** extending the observation interval did not resolve picture
acquisition, and capture software alone cannot explain the direct monitor
result. **UNKNOWN:** a malformed candidate waveform versus rejection of
valid but unsupported timing by both receivers; AV cache success cannot
distinguish those conditions. Candidate timing and HDMI signaling need
further analysis before this is described as working 540p output.
