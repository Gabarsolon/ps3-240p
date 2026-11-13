# Redraw after the PS menu

**CLAIMED (owner, 2026-11-13):** after closing the PS menu, the app remained
dim and retained XMB's loading indicator at the top right. A D-pad input
immediately repaired the picture. This also happened before the PAL work.

**PROVEN (source):** unchanged menu content was rendered into both buffers,
then reused without repainting. The close-event path reset this cache once.
**INFERRED:** VSH could finish writing overlay pixels after those first
refreshed frames, leaving cached dimming/spinner pixels until the next input.

**PROVEN (dev4 source):** after the overlay closes, the app redraws both
buffers continuously for one second before resuming unchanged-content reuse.
Sources: `app/source/main.c:frame`, `redraw_until`.

**PROVEN (host test):** startup scenario 18 emits close events, then corrupts
both buffers after the initial refreshed frames. With no controller input,
the app repaints both buffers correctly. The eight-minute idle/exit scenario
and existing menu/GPU transition scenarios also pass. Source:
`tests/test_startup.c`, `tools/test_app.py` (19 scenarios).

**CLAIMED (owner after dev4 installation):** the dimming/loading indicator
now clears automatically after closing the PS menu, without D-pad input.
**PROVEN (deployment):** the installed app files match the decoded package,
and a fresh dev4 log completes its first two frames. Private evidence:
`local_only/activation/pal288-development/dev4/`, including `deployment.json`,
`launch.log`, and `ps-menu-fixed.log`.

**UNKNOWN:** the exact timing of the firmware's real overlay writer. The
host model and owner result support the fix without establishing that timing.
