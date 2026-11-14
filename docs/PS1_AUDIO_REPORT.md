# PS1 audio and crop report after the PID compatibility fix

Checkpoint: 2026-11-14. External messages and app log relayed by the owner.
Reporter identity, console and audio have not been independently measured.

## Observations

- **CLAIMED (tester):** v1.4.4-rc1 works in the app and XMB at 240p, including
  the timed recovery and background corrector, on the previously reported
  PAL CECH-2004A / Evilnat 4.93 CEX + Cobra 8.5 configuration.
- **CLAIMED (tester):** Toy Story 2 and Battle Hunter have a usable picture
  with some zoom/crop, but lower audio pitch and slight crackling only in
  PS1 games while 240p is active.
- **CLAIMED (follow-up):** both games are NTSC versions. The initial
  PAL-game/50-Hz hypothesis therefore does not fit this report as stated.
- **CLAIMED (later follow-ups):** the same games have normal audio at stock
  480i. Audio is analog AV Multi-Out to a mono CRT using yellow video and
  white audio, not HDMI or optical. This satisfies the first stock comparison
  below as a tester report; it has not been independently recorded.
- **PROVEN (supplied app log):** discovery finds one supported identity at
  PID 9, resolves all 54 timing targets across 109 pages, applies the NTSC
  transaction, selects public resolution ID 4 and observes analog-head VID
  `0x10`, 720 x 480 source, 480-active / 522-total software-cache geometry.
  It reuses the resident corrector targeting resolution 4 and exits normally.
  Sources: supplied log timestamps `465778..487330`; this is software
  evidence, not an electrical timing or audio measurement.

## What this does and does not establish

**PROVEN (source comparison):** the PID compatibility change replaces the
fixed service lookup; it does not change `timing_payloads.h` or add audio
patches. Sources: commit `79a3835`, `app/source/sysmgr.c`,
`app/source/lv1_patch.c:service_pages`. A successful lookup does not exclude
an existing timing/emulator interaction on newly supported hardware.

**PROVEN (log scope):** the provided log covers app activity, not the later
game's emulator choice, video transitions, audio configuration, sample rate
or playback pitch. It begins at 576i/50 and switches to NTSC output, but
that does not prove the game itself ran at a PAL cadence.

**UNKNOWN:** whether the crop is source scaling, normal CRT overscan or
another geometry difference. Do not advertise an integer-scaled or perfectly
fitted PS1 picture on this evidence alone.

**UNKNOWN:** the audio cause. Possible investigation areas are the chosen
PS1 emulator, its output mode/region handling, video-cadence/audio pacing,
and the active audio route. PAL console region alone is not an established
cause. No fix or audio-clock write is justified by the app log.

## Next discriminating checks

These are proposed checks (**INFERRED**), not completed tester results:

1. Compare the same NTSC game in stock 480i/60 and 240p/60, keeping the
   emulator and audio path identical. Stock 576i/50 is not the matched
   control for this comparison. **CLAIMED:** the tester has now confirmed
   normal audio in stock 480i; do not repeatedly request this same check.
2. Record whether the emulator is `ps1_emu` or `ps1_netemu`, launch method,
   exact game region/release and audio path. **CLAIMED:** NTSC versions and
   analog white-RCA audio to a mono CRT are now known; emulator choice and
   exact releases remain unreported.
3. Collect the helper log during game launch and inspect game-time video and
   audio readback. Distinguish an actual slower emulation cadence from a
   sample-rate/pitch problem and buffer underruns. Objective audio comparison
   needs a separate console/capture-card audio diagnostic, not microphone
   recording; the HDMI picture-test captures remain audio-disabled.
4. Trace the timing and audio paths offline before changing PLL, sample-rate,
   encoder or emulator state. Nominal metadata alone cannot measure physical
   cadence or identify which clock drives audio on this setup.

The PID compatibility result is recorded in
[the model report](CECH2004A_LOOKUP_REPORT.md). Audio/crop compatibility
remains open; keep the build a prerelease.
