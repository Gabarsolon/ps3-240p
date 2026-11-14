# v1.5.0-dev4: explicit progressive stock transition

Checkpoint: 2026-11-14. This opt-in development build addresses the failed
stock transition documented in [dev3](HD540_DEV3.md). It is not a release
or evidence of working 540p output.

**PROVEN (private 4.93 VSH audit):** the mapper at `0xc56f4` can return
native mode 8 for a requested 720-line source when conversion is selected.
The dispatch table at `0x4356f0` sends mode 8 through resolution case 3
at `0x435818`, and mode 11 through case 2 at `0x4357e0`. Both cases reach
public source resolution 2 through the configuration bridge at `0xc7ecc`.
However, the selector at `0x4303f0` yields interlaced scan value 1 for mode
8 and progressive value 2 for mode 11. Original native mode 10 follows
the 1080/interlaced case. Sony-derived listings remain private.
Evidence: ignored `local_only/activation/vsh.elf` at those addresses and
`local_only/hd540/dev4/native-mode-audit.json`.

**INFERRED (static audit plus dev3 live logs):** accepting native mode 8
as a physical 720p alternate explains why dev3 changed its surface to
1280 x 720 while retaining interlaced VID 7 / 1080 active / 1125 total
lines. The mapper result alone does not establish physical output timing.

**PROVEN (implementation):** `plugin/vsh_video.c:vsh_video_hd720` now
selects audited native mode 11 after accepting only the expected HDMI
mapper modes 8/11 and rejecting clone/analog/50 Hz or unexpected modes.
It retains mapped connector/refresh flags and saved surface format/pitch.
The HD backend still requires progressive native readback and independent
720-active / 750-total AV cache readback. It preserves the exact original
mode for recovery. No guard is relaxed and no persistent display setting,
clock, encoder or MMIO operation is added.

**PROVEN (helper compatibility):** protocol magic is now HD53 and readiness
uses `crt_hd540.v3.ready`. This prevents the new app from treating the
older resident helper as the corrected service. Reboot once after replacing
the files. Diagnostics now include the native surface format as well as
mode, flags, pitch and cache geometry.

**PROVEN (PC validation):** the full host suite passes, including a regression
for the observed mapper mode 8, rejection of unexpected mappings, all 24
app startup/return scenarios, bounded recovery, exact restoration and
candidate locking. Private firmware checks match all 54 SD words and both
HD record fingerprints. The rebuilt helper passes loader/thread-exit checks;
the package members decrypt and match staging. These checks do not substitute
for a console transition or electrical measurement.

**PROVEN (artifact and deployment readback):** experimental package
`build/hd540/deliverables/CRT-Toolset-1.5.0-dev4.pkg`, SFO 01.53,
301152 bytes, SHA-256
`0ee86af8f56b1ddebb459ae8c126f22cd9455fbd72bc24582f1d4d74333cf9e7`.
The app, SFO and both helper copies were installed at XMB with app/enforcer
flags absent, prior-version identity checks, backups and byte readback.
No remote launch, controller input or video test was performed.
Evidence: ignored `local_only/hd540/dev4/deployment.json`.

**UNKNOWN:** whether the explicit progressive request produces and restores
the expected physical output on the console. The next owner-operated check
starts in HDMI 1080i and selects **Test stock 720p / 1080i round trip**.
Leave the controller untouched while the app exits and the helper returns
to stock. Only a verified successful stock round trip can unlock the 540p
candidate in that helper session. Correct 540p signaling and HD CRT
compatibility remain unverified. Stable v1.4.3 artifacts are unchanged.

## Live stock result

**PROVEN (owner-triggered action 4 logs):** dev4 selected native mode 11,
reported native resolution/scan 2/2, and passed the independent AV cache
check: frontend VID 9, source 1280 x 720, 720 active / 750 total lines.
The helper advanced to state 2, then returned to original mode 10 and
native resolution/scan 1/1. It finished in state 7 with detail 1 after an
entering-stage timeout and a direct stock recovery request. No candidate
patch was requested or applied, and the stock prerequisite remained false.
Evidence: ignored `local_only/hd540/dev4/live/20261114-014228/`, request
sequence 160872109. This establishes software readback, not an electrical
measurement of the HDMI signal.

**PROVEN (source and diagnostic timestamps):** the service calculated the
return deadline from tick-entry time, before the alternate readiness query
and original-mode submission. Several synchronous diagnostic writes during
that query took substantial time; the logged surface query began near
166.30 seconds, with the final head readback near 168.20 seconds. That work
consumed part of the following transition's five-second allowance before
the original request was sent. The stock-enabled readback arrived near
172.06 seconds. [Dev5](HD540_DEV5.md) addresses the stale deadline origin
and reduces diagnostic I/O. Its live stock round trip still needs testing.
