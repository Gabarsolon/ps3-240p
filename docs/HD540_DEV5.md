# v1.5.0-dev5: transition deadlines after native callbacks

Checkpoint: 2026-11-14. This experimental build follows the [dev4 stock
test](HD540_DEV4.md#live-stock-result), which reached progressive 720p AV
cache readback but falsely exhausted part of the return allowance while
checking and logging the preceding transition.

**PROVEN (implementation):** `plugin/hd540_service.c` now samples an injected
monotonic clock after native callbacks. The next transition's five-second
deadline starts after its mode request returns, including direct recovery
and the one permitted recovery pulse. Expiry decisions after readiness
queries use fresh time. The six-second candidate timer remains bounded,
and duplicate commands cannot extend it or reset the video retry budget.
The production backend supplies the PS3 monotonic system clock; host tests
supply a controllable clock. Callback time cannot move the service clock
backwards.

**PROVEN (diagnostics):** `plugin/vsh_hd540.c` suppresses unchanged native
state and combines surface fields and head fields into fewer log writes.
It retains flags, mode, format, pitch, native scan/state and head geometry.
Actual-raster checks, exact original-mode preservation, transactional RAM
restoration and the stock-round-trip prerequisite are retained.

**PROVEN (validation):** the regression simulates two seconds in the prior
readiness query plus a half-second submission. It verifies that the
original return still gets its full five seconds, can finish successfully
after the formerly incorrect deadline, and still fails a real timeout.
The full host suite, all 24 app startup/return scenarios, private SD/HD
firmware record checks, experimental helper loader checks and package
member verification pass. Evidence: `tests/test_hd540_service.c`,
`tools/test_app.py`, `tests/test_plugin.py` and the build tools.

**PROVEN (artifact and deployment):** package
`build/hd540/deliverables/CRT-Toolset-1.5.0-dev5.pkg`, SFO 01.54,
301216 bytes, SHA-256
`21d15631b5e0d293b948021f7cd77083f5361595841f00a9bb9b32c6e5ce64a7`.
The app, SFO and both helper copies were installed at XMB with app/enforcer
flags absent, prior-version checks, reversible backups and byte readback.
No remote launch or video request was performed. Evidence: ignored
`local_only/hd540/dev5/deployment.json`.

**PROVEN (compatibility gate):** HD54 / `crt_hd540.v4.ready` prevents reuse
of the older resident service. Reboot once, remain in HDMI 1080i, open
v1.5.0-dev5, then select **Test stock 720p / 1080i round trip** and leave
the controller untouched during exit and automatic return.

**UNKNOWN:** dev5's live stock round-trip result, successful candidate
output and HD CRT compatibility. The 540p candidate stays locked until
a stock round trip completes without an error. Stable v1.4.3 artifacts
are unchanged.

## Live stock round trip passed

**PROVEN (owner-triggered console logs):** action 4, sequence 114527939,
completed with reply state 7 / detail 0. After normal app exit, native mode
11 reached resolution/scan 2/2 and AV head VID 9, 1280 x 720 source,
720 active / 750 total lines. The original native mode 10 then returned
with resolution/scan 1/1 and XMB pitch 8192, passing the original-profile
cache check. There was no recovery error and no candidate patch operation.
The successful stock round trip sets the in-memory candidate prerequisite
for this resident helper session. Evidence: ignored
`local_only/hd540/dev5/live/20261114-014853/app.log`, `helper.log` and
`reply.bin`. This is software readback; it does not establish electrical
waveform measurements or successful 540p output.

**UNKNOWN:** the result of the next owner-operated **Test 540p candidate**
check. It automatically restores stock; there is no keep/enforce action.

## Candidate produced black capture; separate stock cycle recovered XMB

**PROVEN (console logs):** owner-triggered candidate action 2, sequence
207491092, passed the 720p alternate check and called the patcher with
result 0. The candidate cache check reached active state 3, held for
approximately six seconds, then finished in state 7 / detail 0 after
restoring timing RAM and passing the stock cache check. Evidence: ignored
`local_only/hd540/dev5/candidate/20261114-015014/`.

**CLAIMED (owner):** the capture was black during the candidate and remained
black in OBS afterwards. **PROVEN (captured preview):** the subsequent
OBS Virtual Camera frame was also black. Software recovery success did not
establish recovery of the visible picture.

**PROVEN (separate stock recovery):** one stock-only action 4, sequence
2678139811, reached progressive 720p cache readback, returned to stock
1080i, and completed state 7 / detail 0. The subsequent OBS preview showed
XMB with CRT Toolset selected. No app launch or controller input was sent.
Evidence: ignored `local_only/hd540/dev5/candidate/stock-recovery-*`,
`current-preview.png` and `after-stock-recovery.png`.

**UNKNOWN:** whether the black candidate reflects invalid output timing,
unsupported HDMI signaling or receiver behavior. The successful stock
cycle does not distinguish these explanations. [Dev6](HD540_DEV6.md)
adds this stock cycle to candidate recovery and extends the observation
hold to 15 seconds at the owner's request. It does not establish a
working 540p waveform.
