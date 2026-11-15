# HDMI custom resolutions: in-app preview and session enforcement

Owner-requested flow, 2026-11-15: choose one resolution, preview it inside
CRT Toolset for 15 seconds, press Cross to keep or Circle to restore, and
exit to XMB manually. Modes: 540p, 1024x768, 1280x768, 1280x1024, 1920x1200.

**PROVEN (implementation):** the isolated `--hdmi-custom` app/helper use
`HC_MAGIC=0x48434d32`, readiness `.v2.ready` and module `crt_hdmi_custom`.
A fresh app-selection request registers its process/heartbeat owner.
The app drains its own GPU work before each native VSH video command,
then binds and draws both app buffers at the enabled source geometry.
The helper starts the confirmation timer only after that acknowledgement.
Cross must be freshly pressed and released; holding the launch button
cannot confirm. Sources: `app/source/main.c`,
`app/source/hdmi_custom_protocol.h`, `experiments/hdmi_custom/video.c`,
`service.c`, `confirm.c`, `tests/test_hdmi_video.c`, `test_hdmi_custom.c`.

**PROVEN (live, 2026-11-15):** the 1280x768 entry showed a readable
confirmation screen with the app still running. The receiver reported
1280x768; timeout restored receiver-reported 1920x1080 and the app menu.
The helper's preview-to-restoration interval was 15.014 seconds.
Evidence: ignored `local_only/hdmi_custom/app-20261115-120941/`.

**PROVEN (live app source/rollback):** the corrected 1024x768 trial
bound the app at 1024x768, pitch 4096, acknowledged both buffers and showed
a readable confirmation screen. The 15.050-second software preview
restored the checked 1080p source, original source instructions and cache
wrapper. FTP observation timed out; separately retrieved HTTP logs and
saved frames establish this limited result. A fresh independent receiver
sample for this in-app trial remains UNKNOWN.
Evidence: ignored `local_only/hdmi_custom/app-20261115-121555/`,
`crt_hdmi_custom.log`, `ps3_240p.log`, `latest.jpg`.

**PROVEN (causes and guards):** an earlier app-owner publication gap
incorrectly looked like process exit; missing/partial context publications
now retain only the original three-second lease. The old native prepare
check also rejected the owned app; it now uses the same owner-aware check,
while still rejecting SD enforcement. In-app XGA needs an explicit
4096-byte source pitch because VSH does not allocate its patched XMB
buffer while the app owns GCM. Earlier refusals restored stock and are
not successful preview results. Sources: `video.c`,
`experiments/display_probe/native.c`, `tests/test_hdmi_video.c`,
`tests/test_display_probe_native.c`; ignored `app-20261115-115823/`,
`app-20261115-120604/`, `app-20261115-121119/`.

**PROVEN (540p control sequence in source):** the controller snapshots
stock 1080p, temporarily selects native 1080i, runs the original stock
round-trip and progressive-720p AVI controls, then starts the existing
540p candidate. Recovery completes that engine's RAM-first stock-1080i
sequence before returning to the original 1080p tuple. Both engines share
the checked wrapper arena serially. Sources: `backend.c`,
`plugin/hd540_service.c`, `plugin/vsh_hd540.c`, `xga_cache.S`.

**PROVEN (offline):** `python -B tools/test_hdmi_custom.py` exercises
all five session modes, exact preview deadlines, confirmation ownership,
120-second diagnostic expiry, keep/cancel, suspend/resume, failed-recovery
latching, real engine integration with fake interfaces, app/GPU handshake,
and default HD backend regression checks. Custom wrapper emulation
(`tests/test_xga_cache_ghidra.py --custom`) checks its authored 784-byte
payload's ABI, temporary packet restoration, instruction-cache coverage
and absence of MMIO stores. These checks do not establish physical output.

**PROVEN (session policy in source):** confirmation returns to the app
menu. The owner exits manually. On app exit the helper restores the old
surface and reapplies the confirmed mode at idle XMB; unrelated game/app
launches suspend it and trigger stock recovery. The app waits before GCM
startup when a resident selection is recovering. No boot plugin or saved
video setting is installed. Sources: `service.c`, `input.c`, app startup.
**UNKNOWN:** arbitrary game compatibility and timing during other launches.

**PROVEN (automation guard in source):** normal `HC_APP_SELECT` keeps a
confirmed selection for this session. An optional, exact-mode test lease
with a console-clock expiry within 120 seconds is consumed once by the
helper; that same app-menu request then uses `HC_TRIAL`, which restores
120 seconds after confirmation even if the PC disconnects. A separate
`HC_STOP` also restores. Sources: `plugin.c`, `service.c`, protocol header.

**PROVEN (input ownership in source):** in-app previews use the app's own
controller input. The helper borrows no VSH pad byte while that app owner
is live. The older XMB diagnostic path retains a checked, bounded pad
borrow, based on the upstream mechanism in
[webMAN's pad implementation](https://github.com/aldostools/webMAN-MOD/blob/master/_Projects_/vsh_menu/include/pad.h).
Sources: `input.c`, `confirm.c`.

**UNKNOWN / remaining live checks:** 540p and the other native presets in
the revised in-app flow; released-Cross keep and stop; manual app exit,
XMB enforcement, reopening and suspend/resume. Prior successful XMB-only
mode tests do not establish these app-buffer paths. A 4:3 output raster
also does not establish correctly proportioned 4:3 game content; see
[aspect research](HD_ASPECT_RESEARCH.md).

**PROVEN (in-app 540p failure, not a successful mode):** the first trial
used a pre-rebind callback pitch snapshot and refused, then restored stock.
The next trial verified both GCM buffer descriptions after completed flips
and reached software preview, but the receiver reported no signal; it
expired without confirmation and completed checked stock restoration.
That app registered a 1920x540 source with pitch 16384. The latest offline
candidate retains a full 1920x1080 logical source and only doubles the row
pitch, matching the source-height convention of the successful XMB path.
**INFERRED:** changing logical buffer height as well as physical timing may
have caused the in-app no-signal result. This requires a live test.
Sources: ignored `app-20261115-122646/`, `app-20261115-123322/` logs;
`app/source/main.c`, `video.c`, `plugin/vsh_hd540.c`.

**CLAIMED (owner, 2026-11-15):** repeated power outages interrupted the
session, and the owner had moved the OBS scene, explaining shifted frame
edges. Those black/displaced capture intervals are not independent evidence
of an app crash. Live testing is waiting for stable power; the final source
height candidate has been built, not installed or validated on hardware.

## 540p resolved (2026-11-15)

**PROVEN (owner observation, Xiaomi HDMI monitor):** with the helper's AVI
VIC-0 patch skipped, the XMB-only 540p candidate was locked and shown as
1920x540 at 60 Hz with a full, correct picture. With the patch, the same
monitor showed black with no backlight and its OSD did not open. dev19's own
stock-720p AVI-zero control was also black on that monitor. The unchanged
raster telemetry (FP vertical end 539, hardware fetch pitch 0x4000) was
identical in both cases. Evidence: ignored `local_only/hdmi_custom/xmb-20261115-173851/`
(`hd_avi_skipped`), `local_only/hd540/automatic/20261115-173505/`.
**INFERRED:** forcing the AVI builder's VIC byte to zero affects more than
the AVI field on this transmitter path. The UltraSemi capture card accepted
it only intermittently; earlier "no signal" 540p results today are not
evidence about the timing. The custom edition now keeps stock AVI (VIC 5) for
540p and no longer runs the black AVI-zero 720p control. Its prerequisite is
the stock roundtrip only. **UNKNOWN:** whether some sinks reject VIC 5 for a
progressive 1920x540 signal. Sources: `plugin/vsh_hd540.c` `apply`,
`plugin/hd540_service.c`, `experiments/hdmi_custom/backend.c`.

**PROVEN (in-app crop cause, capture frames):** the app's 540p preview was
cropped to the top half and stretched vertically, regardless of registering
its display buffer at pitch 8192 or 16384. The doubled native pitch writer
`0x8000369c` is called by display setup `0x800005e4` only when the pitch
changes ([dev18](HD540_DEV18.md)). XMB rebuilds its 720p surface at 0x1400,
so the return to 1080i at 0x2000 reaches the writer. The app kept 0x2000 at
720p (`hd_surface mode=b pitch=2000`), so the writer was skipped and the
fetch stayed 8192. Evidence: ignored `app-20261115-175520/`, `app-20261115-175803/`.

**PROVEN (fix, live):** with an app owner, the 720p step now requests pitch
1280*4 (`vsh_video_hd_submit_pitch`); the app draws and registers at 8192
(factor 1). The candidate now also requires hardware fetch pitch twice the source
in the app path. In-app 540p showed the complete menu with correct
proportions and fetch pitch 0x4000 from both a native 1080i start (no bridge)
and a 1080p start (bridge), then restored stock pitch 0x2000.
Evidence: ignored `app-20261115-180343/` and `app-20261115-181023/`.

**PROVEN (implementation):** 540p accepts XMB at 1080i (recommended for HD
CRTs, which usually lack 1080p) or 1080p; other custom modes still need
1080p. The refusal message says so. All custom labels now read 16:9 (the 16:9 source
fills each raster; other aspect ratios are future work).
**UNKNOWN:** HD CRT behavior, games that keep their own pitch, and component.
