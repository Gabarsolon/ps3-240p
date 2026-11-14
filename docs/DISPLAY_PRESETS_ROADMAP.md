# Display presets and scaling roadmap

Owner-approved investigation, 2026-11-14. This is a proposed direction for
CRT Toolset, not a list of supported new modes.

Evidence labels: **PROVEN** = checked source/binary or recorded readback;
**INFERRED** = interpretation; **CLAIMED** = attributed report;
**UNKNOWN** = not established. Proposed features below make no compatibility
claim. Sony-derived analysis stays in ignored `local_only/`.

## Product direction

Keep resolution and picture controls inside CRT Toolset. Start with named,
validated presets and timed restoration; add arbitrary custom timings only
after the preset path is reliable.

1. **Resolution presets:** group existing stock and experimental modes;
   investigate PC modes and the requested 1024 x 768. Display connector,
   dimensions, refresh family and progressive/interlaced status separately.
2. **Picture sizing:** investigate width, height, position, preserved aspect,
   stretching and borders for 4:3 and 16:10 displays. Correct scaling must be
   checked separately from successfully emitting a signal.
3. **Saved profiles:** combine validated timing and picture settings for each
   display. Preview with automatic restoration before keeping a change.
4. **Advanced timings:** later investigate manual active dimensions, refresh,
   porches and sync. Do not expose arbitrary values as universally supported.

## Initial evidence and limits

- **PROVEN (public source):** Sony's Linux PS3 AV driver includes 1280 x 768,
  1280 x 1024 and 1920 x 1200 in `video_mode_table`; its header defines WXGA,
  SXGA and WUXGA frontend IDs 0x0d, 0x0e and 0x0f. Sources:
  [driver](https://github.com/torvalds/linux/blob/master/drivers/ps3/ps3av.c),
  [header](https://github.com/torvalds/linux/blob/master/arch/powerpc/include/asm/ps3av.h).
  **UNKNOWN:** usability under the current retail GameOS, game compatibility,
  connector limits and correct aspect treatment. Linux support is not a
  retail compatibility result. 1280 x 1024 is 5:4, not 4:3.
- **CLAIMED (devkit owner):** DECR-1000A VGA PC-mode output is demonstrated
  in the [linked Reddit post](https://www.reddit.com/r/crtgaming/comments/1odzq7z/ps3_native_vga_support/).
  This is a lead, not proof that retail AV Multi-Out can provide RGBHV or
  the same resolution choices.
- **PROVEN (project implementation):** the current 540p experiment samples
  alternate rows from a 1080-line source, uses HDMI only and always restores.
  Source: [dev19 release evidence](RELEASE_v1.5.0-dev19.md),
  `app/source/hd540_scale.c`, targets 0x80002f48..0x800036d0.
  **INFERRED:** output timing, source rendering and picture scaling must be
  treated as separate controls; choosing a timing cannot promise a game
  renders natively at that resolution.
- **CLAIMED (Sony user guide):** stock supported modes vary by connector.
  Source: [Video Output Settings](https://manuals.playstation.net/document/en/ps3/current/settings/videooutput.html).
  **UNKNOWN:** each additional custom-mode/connector pairing until checked.

## Investigation sequence

1. Trace existing PC frontend IDs through retail 4.93 validation, internal
   profile selection, raster/clock records and HDMI backend descriptions.
2. Identify how GameOS/VSH chooses output timing and source-buffer geometry;
   locate any policy restriction separately from hardware capability.
3. Trace scaler/aspect/placement inputs and bounds. Prefer a narrowly scoped
   existing-mode path before adding new arbitrary timing values.
4. Choose one HDMI preset with a coherent source, timing and backend route;
   prepare a reversible test only after the offline evidence is sufficient.
5. Validate receiver scan status plus visible geometry/test patterns and exact
   stock restoration; keep XMB, application and game results separate.

See [investigation findings](DISPLAY_PRESETS_INVESTIGATION.md) for the evidence
collected after this roadmap was approved.

## Other community requests to track

These requests were supplied by the owner from Reddit on 2026-11-14. They
are proposed work, not evidence that the feature currently works.

- PS1 automatic 240p/480i transitions, including Gran Turismo 2.
- Pixel/integer-scaling verification using suitable test patterns.
- End-to-end input-latency measurement; emulator buffering reduction is a
  separate investigation and no improvement is established by timing alone.
- Component 240p/288p and 540p validation; do not infer support from the shared
  AV Multi-Out connector.
- Compatibility reports for Mega Man 9/10, PS2 and backward-compatible models.
- External CECH-2004A PID-fix results; owner success does not cover that model.
- Reusable implementation notes for homebrew/emulator developers such as NotDC.
- HEN research remains separate; see [current evidence](HEN_SUPPORT.md).
- Gamma calibration guidance should describe a correction, not promise repair
  of worn hardware or recovery of already clipped image detail; see
  [calibration guide](CRT_CALIBRATION.md).

## First milestone

An offline, address-backed map of existing PC-mode and scaling paths, with a
single justified first preset and explicit unresolved blockers. No automatic
boot default or arbitrary clock programming is needed for this milestone.
