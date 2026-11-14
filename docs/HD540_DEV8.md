# v1.5.0-dev8: change AVI generation while preserving the transmitter template

Checkpoint: 2026-11-14. [Dev7's stock control](HD540_DEV7.md) produced a
black/no-signal picture according to the owner, although its software AVI
cache and recovery checks passed. The shared template change was included
in the transmitter command as well as the generated AVI packet. It was
therefore an incomplete isolation of the HDMI-description hypothesis.

**PROVEN (private 4.93 static trace):** AVI builder `0x8000fde0` uses separate
pointers for the command core and AVI output. The byte load at `0x80010004`
supplies only the AVI video-code merge; the destination register is reused
after that store. The native checksum calculation follows the store. The
unmodified shared transmitter template remains the source of the core mode.
Evidence: private `sysmgr_ss.elf`, those addresses and ignored Ghidra output
under `local_only/activation/`. No Sony instruction listing is published.

**PROVEN (implementation):** `hd540_hdmi.c` now fingerprints the two qwords
around that load and separately fingerprints the unchanged transmitter
template. It creates a replacement instruction from the PPC encoding for
loading zero into the original destination register. A single aligned
ordinary-RAM qword write changes that instruction and preserves the adjacent
instruction; original code is saved at runtime. No Sony instruction bytes
are embedded in the source. The prior dev7 template-byte modification is
removed. Flash, firmware images, clocks and MMIO are not written.

**PROVEN (readback and recovery):** the control retains stock 1080i raster
and transmitter template, and requires validated AVI cache code 0 before
starting its 15-second hold. Cache code 0 verifies changed builder behavior;
RAM code readback alone is insufficient. Recovery restores the original
RAM code before the stock 720p / 1080i cycle and requires AVI code 5.
Current code, neighboring code/template values, pointers and mappings are
checked; unknown external changes are not overwritten. The combined 540p
candidate still requires the stock and AVI-only controls to complete in the
resident session, and the owner must check the control picture first.

**PROVEN (live functional check):** the new patch changed the generated AVI
cache to code 0 and restoration returned it to code 5. This verifies execution
and restoration at this location for the observed run. It does not establish
a general instruction-cache coherency mechanism for other patch locations.

**PROVEN (PC checks):** full host suite, all 24 app startup scenarios,
existing SD/HD firmware fingerprints, the HDMI-template fingerprint and the
new AVI-builder fingerprint pass. Neighboring code changes are rejected.
Tests cover generated instruction scope, unchanged transmitter template,
packet checksum/VIC readback, each store-failure position, rollback/retry,
and mapping/pointer/third-party-change refusal. The rebuilt helper passes
loader/thread-exit checks and package members decrypt and match staging.

**PROVEN (artifact/deployment):**
`build/hd540/deliverables/CRT-Toolset-1.5.0-dev8.pkg`, SFO 01.57,
303520 bytes, SHA-256
`22bef71c9d26e40318ff5030af490f1a2eb5deedd6b1e3cc38a390081f39f7a9`.
The app, SFO and both helpers were installed at idle XMB with prior dev7
hash verification, backups and installed byte readback. No remote launch or
video request was sent. Evidence: ignored `local_only/hd540/dev8/deployment.json`.
HD57 / `crt_hd540.v7.ready` requires the new resident helper after reboot.

## Live control result

**PROVEN (logs):** the app published action 5, sequence 100270889, and exited
normally before the helper changed video mode. The app explicitly logs the
normal exit, active-flag removal, pad closure and callback removal. The helper
entered ACTIVE at 111363855 microseconds and began restoration at 126528071:
the hold lasted **15.164216 seconds**. It restored stock mode 10 / AVI code 5
and reached DONE (state 7), detail 0, at 133813048. Evidence: ignored
`local_only/hd540/dev8/live/20261114-124316/app.log`, `helper.log` and reply.

**CLAIMED (owner, 2026-11-14):** no picture during the AVI-only control; the
owner thought the app crashed because the visible interval seemed shorter.
The logged intentional exit and 15.164-second hold do not show an app crash.

**UNKNOWN:** actual transmitted packet contents and electrical receiver
behavior. A successful software control/recovery does not mean its picture
worked. Do not treat this result as visual validation for the combined 540p
candidate. Capture-card status instrumentation is documented in
[Capture card input status](CAPTURE_INPUT_STATUS.md).
