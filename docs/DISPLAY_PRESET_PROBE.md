# Native HDMI PC preset probe

Checkpoint: 2026-11-14. **Experimental developer probe, not a released app
menu or permanent preset. The later dev1 menu results are tracked in
[the PC preset checkpoint](PC_PRESETS_DEVELOPMENT.md).** It uses existing retail VSH display interfaces;
it does not patch LV1 timing tables or contain Sony instruction listings.
See [roadmap](DISPLAY_PRESETS_ROADMAP.md) and
[offline evidence](DISPLAY_PRESETS_INVESTIGATION.md).

## Result

**PROVEN (owner-console logs, real OBS captures and receiver report):** on
the owner's Slim / Evilnat 4.93 CEX + Cobra HDMI setup, the native path
produced receiver-reported **1280 x 768**, retained a visible XMB picture
through a **15.068853-second** active hold, and restored the original 1080p
mode, source format/pitch, two capability masks and monitor-request state.
The undisturbed run passed all four stages: inspection, stock 720p control,
capability-only control and WXGA candidate. Mature active frames all passed
the lit/textured-picture check. Inspected captures show XMB content with
vertical borders; calibrated aspect, crop and pixel-perfect scaling are not
established by those images.

Evidence: ignored `local_only/display_presets/automatic/20261114-182317/`,
candidate sequence `2489216664`; helper ACTIVE at `154422746` microseconds,
restoration at `169491599`, then DONE/detail 0. Cached receiver size/signal
was 1280 x 768/present during the hold and 1920 x 1080/present after return.

**PROVEN (separate diagnostic run):** sequence `926959872` also completed
and restored stock. The capture tool reported 1280 x 768 with raw scan
status `0x07`, interpreted upstream as progressive. Evidence: ignored
`local_only/display_presets/automatic/20261114-182524/`. This one-shot live
query can interrupt capture; it is separate from the undisturbed proof.
Exact receiver firmware mapping and physical refresh measurement remain
**UNKNOWN**. Nominal 60 Hz is the native profile/request, not a measured Hz.

**UNKNOWN:** games/apps, other models/firmwares, component/RGB/VGA, HEN,
the other PC presets, arbitrary modelines and user-adjustable scaling.
1280 x 768 is **5:3**, not 4:3 or 16:10. The renderer remains a normal
1280 x 720 source converted by VSH; no game-native rendering change is claimed.

## Why the native request needed a separate path

**PROVEN (retail 4.93 VSH trace and live readback):** the original display
manager is reached through `xsetting` export `0x4712f276`; its device objects
are reached from manager pointer `0x00738bf4`. The active HDMI capability
method is `0x0047c534`, and the other registered device's method is
`0x00474128`. Their existing mask fields are not necessarily zero. The
probe snapshots each and restores the exact values using the same native
methods, rather than treating monitor type zero as a full rollback.

**PROVEN:** monitor selection 11 corresponds to WXGA capability mask
`0x40000000`. After selection, native device-info advertises a 720-line
source with conversion enum 1 and refresh mask 4. Native mapper
`0x000c7080` returns flags `0x00040210` and source mode 11 for that request.
It deliberately selects logical category `0x10` for the PC conversion path,
despite the device-info port remaining HDMI. Copying the original TV flags
instead of using this mapped request failed. Sources: the matching private
VSH, mapper/dispatch at `0x000c7080` / `0x00435524`, and the live probe logs.

**PROVEN:** configuration bridge `0x000c825c` clears the one-shot monitor
request at `0x0070f13c`. The successful target guard therefore checks the
actual converted source state and owned capability masks, allowing that
documented acknowledgement; it does not demand the request flag stay set.
The native format/pitch and scan/conversion enums are distinct from public
`cellVideoOut` enums. Source identity is the VSH SHA-256 in the offline report.

**PROVEN (recovery regression):** a redundant restore submission can return
busy while an already accepted transition completes. Completion requires
independent full snapshot readback; a transient retry result cannot override
that proof. This behavior is covered by `tests/test_display_probe.c`.

## Build and checks

```powershell
python tools/build_plugin.py --display-probe
python tests/test_plugin.py build/display-probe/plugin/vsh_240p.elf --module-name crt_display_probe
python tests/test_display_probe_auto.py
```

**PROVEN (build isolation):** outputs stay in `build/display-probe/plugin/`;
the packed module name is `crt_display_probe`. Stable SD/PID/HD artifacts
and the installed CRT Toolset app are not replaced by this variant.
Successful tested helper: **49408 bytes**, SHA-256
`d58540bc9b3a7585b7c121bb0f71a0a059f8f8d9adc4625d19b10ee5aae1eea0`.
Source: checked deployment
`local_only/display_presets/live/deploy-20261114-182307/deployment.json`.

**PROVEN (offline checks):** host service tests cover the full hold, failed
entry/partial capability updates, exact restoration, cancellation, duplicate
requests, callback latency and the recovery latch. Native-backend tests cover
nonzero mask snapshots, the mapped PC request, consumed acknowledgement,
bad mapping, source/connector/identity restrictions and ownership changes.
Six Python cases cover acknowledged inbox preservation, bad/partial replies,
bad staging, lost rename acknowledgement, cancellation and latched recovery.
The compiled PRX passes the start/stop thread-exit and export contract checks.
Sources: `tests/test_display_probe.c`, `tests/test_display_probe_native.c`,
`tests/test_display_probe_auto.py`, `tests/test_plugin.py`.

## Owner-authorized developer workflow

The guarded installation follows [AGENTS.md](../AGENTS.md): snapshot at XMB,
pause SD enforcement with a checked backup, preserve affected files, stage
and read back the standalone helper at
`/dev_hdd0/tmp/crt_display_probe.sprx`, then load it only into a confirmed
empty slot. Verify the live slot, fresh startup, ABI marker and exact file
identity. The normal app and boot-plugin list are not changed.

The public runner requires that verified helper already resident and the
console at single HDMI stock 1080p/60-family output:

```powershell
python tools/display_probe_test.py --action all
python tools/display_probe_test.py --action wxga --scan-candidate
```

The second command is a separate receiver diagnostic after the first passes.
The helper requires both controls in the same resident session before WXGA.
It owns the 15-second hold and bounded restoration even if the PC disconnects.
Requests are staged/read back, acknowledged and sequence-checked; an uncertain
recovery stops further tests. Frames, helper replies/logs and receiver reports
stay in ignored `local_only/display_presets/automatic/`.

**PROVEN (loader observation):** stopping the first inspection-only worker
removed its ready file but left its Cobra slot occupied. A checked stock
state and normal soft reboot cleared it before replacement. Do not assume
an HTTP unload success means the slot is free, and never load over it.

## Devkit and HEN implications

**PROVEN:** Sony's Linux HDMI selector explicitly handles the existing PC
modes, and the live result above confirms one on this retail HDMI setup.
A DECR's extra physical VGA connector does not make all PC timing modes
devkit-exclusive. Sources:
[HDMI/VESA selection](https://github.com/torvalds/linux/blob/v6.12/drivers/ps3/ps3av.c#L543-L614),
[HDMI/DVI monitor path](https://github.com/torvalds/linux/blob/v6.12/drivers/ps3/ps3av.c#L703-L728).
**UNKNOWN:** retail analog RGB above stock supported modes or VGA/RGBHV
hardware behavior. HDMI success cannot establish either.

**INFERRED:** this native-only preset path is a useful separate HEN port
candidate, since it uses process reads/native calls rather than LV1 memory
patching. **UNKNOWN:** actual HFW/HEN binding and execution. It does not
provide a low-line 240p/288p raster or remove that backend's LV1 dependency.
See [HEN port requirements](HEN_PORT_PLAN.md).
