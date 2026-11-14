# v1.5.0-dev7: isolate HDMI description from the raster candidate

Checkpoint: 2026-11-14. This build follows the [signal audit](HD540_SIGNAL_AUDIT.md)
and the owner's report that the independent NVIDIA 1920 x 540 reference
works on both receivers. The remaining PS3 output hypothesis is unverified.

**PROVEN (implementation):** the new `hd540_hdmi.c` fingerprints the two
ordinary-RAM qwords surrounding the audited 1080i60 HDMI template. It checks
the template/cache pointer identities and re-resolves physical mappings
before every operation. One aligned qword write clears only the template's
VIC byte from 5 to 0. Neighboring bytes, transmitter configuration fields,
instructions, clocks and persistent settings are preserved. The native AVI
builder calculates the packet checksum. Static evidence and addresses are
in the signal audit; firmware-derived listings remain private.

**PROVEN (readback):** readiness now also reads the native software AVI
cache at the audited service address. It requires two matching snapshots,
AVI type/version/length, a valid checksum and the expected VIC. A template
write by itself cannot pass packet readiness. This is software cache
verification, not an electrical capture of transmitted HDMI packets.

**PROVEN (test separation):** the HD submenu now offers these sequential
checks, each leaving the app normally before output transitions:

1. Stock 720p / 1080i round trip, with no new timing/HDMI template writes.
2. **Test HDMI description only:** retain stock 1080i raster and clear
   only its HDMI video code for 15 seconds.
3. **Test 540p with custom HDMI description:** apply the existing four-word
   raster hypothesis together with the same VIC change for 15 seconds.

The helper requires successful software completion of the stock round trip
before the control, and both prerequisites before the candidate. The owner
must still check the visible result of the control before proceeding;
software completion does not establish that a receiver displayed it.
Evidence: `hd540_protocol.h`, `plugin/hd540_service.c`, `plugin/vsh_hd540.c`,
and `app/source/main.c`.

**PROVEN (recovery):** both experiments restore the original timing and HDMI
template RAM before the stock 720p / 1080i recovery cycle. Readback checks
stock raster and AVI code 5. A failed store, changed mapping or unknown
third-party template value is refused; restoration failures keep recovery
pending. The bounded display retry behavior is retained. HD56 and
`crt_hd540.v6.ready` prevent the new app from using an older resident helper.

**PROVEN (PC verification):** the complete host suite, all 24 app startup
scenarios, private SD/HD record fingerprints and the new HDMI template
fingerprint pass. Tests verify one-byte scope, no raster stores for the
control, cached-packet checksum/VIC checks, both prerequisites, all five
candidate-store failure positions, RAM rollback/retry, stale mapping/pointer
refusal and preservation of unknown data. Experimental helper loader/thread
checks and decrypted package-member verification pass.

**PROVEN (artifact):**
`build/hd540/deliverables/CRT-Toolset-1.5.0-dev7.pkg`, SFO 01.56,
303344 bytes, SHA-256
`77e52998ebe1c7a22ebc35e484b18410f2ea5694dce41ebf57a083f626e18577`.
Helper: 62176 bytes, SHA-256
`69cded7669567527f4be9f0d26a4448afd04f54c0297472c1f07589fa73b002d`.
Stable v1.4.3 package/helper hashes remain unchanged.

**PROVEN (current deployment scope):** this checkpoint built files on the
PC and inspected the existing OBS Virtual Camera preview. It did not
contact the PS3, install the build or issue a video request.

**UNKNOWN:** the live HDMI control result, whether clearing the description
alone is sufficient, and whether the raster hypothesis produces usable
540p. A black control picture would prevent interpreting the candidate
as a clean test of the raster. A working control followed by a black
candidate would strengthen the case for changing the raster/field behavior
or other transmitter configuration, rather than extending its duration.

## Authorized deployment

**PROVEN (subsequent file readback):** after the owner explicitly authorized
installation, the app, SFO and both helper copies were replaced at XMB.
App/enforcer flags were absent, installed dev6 hashes matched the prior
deployment, each prior file was backed up, staged bytes were read back,
and all four installed files matched dev7 hashes. No remote app launch or
video request was sent. Evidence: ignored
`local_only/hd540/dev7/deployment.json`.

**PROVEN (owner instruction and repository policy):** the owner also requested
that `AGENTS.md` permit installers and payloads, retaining the prohibition
on anything that might brick the console. The rule now authorizes reversible
project work without repeated permission prompts and excludes unresolved
brick risks, flash/bootloader writes and firmware programming.

**UNKNOWN:** the live stock/control/candidate results. The next manual
check is a reboot to clear an older resident helper, the stock round trip,
then the HDMI-description-only control. Check its picture before attempting
the combined candidate.

## Live stock and HDMI-description control readback

**PROVEN (console logs):** the stock round trip completed state 7 / detail 0,
including stock AVI code 5 readback. Owner-triggered action 5, sequence
1408795220, subsequently passed the 720p alternate check and applied only
the HDMI template change. There was no raster patch call. After returning
to native mode 10 / interlaced 1080, the validated AVI cache reported code 0
and the helper entered state 3. Recovery began about 15.057 seconds later,
passed stock 720p cache readback, then restored original 1080i and AVI code 5.
The final reply was state 7 / detail 0.
Evidence: ignored `local_only/hd540/dev7/live/20261114-122841/`.

**UNKNOWN:** the owner's picture during the code-0 control interval, actual
transmitted AVI packets and the combined candidate result. Software success
alone does not establish a visible control picture.

**CLAIMED (owner, subsequent response):** the control picture went black /
no signal. The combined candidate was not requested for this checkpoint.
**PROVEN (subsequent static packet trace):** the modified shared-template
byte remains in the transmitter command's core fields as well as supplying
the AVI code. **INFERRED:** dev7 did not isolate a change to AVI packet
generation sufficiently to interpret this as evidence against the raster
alone. [Dev8](HD540_DEV8.md) leaves the transmitter template intact.
