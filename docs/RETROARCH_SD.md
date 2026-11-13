# RetroArch SD setup for v1.2.2

**CLAIMED (owner test):** RetroArch PSX-Place CE 1.10.0, including Super Mario All-Stars and the PS menu, works with the 240p service active after the following saved settings are corrected. Source: [geometry checkpoint](GEOMETRY_STATUS_2026-11-13.md), RetroArch validation section.

Close RetroArch and back up its existing `retroarch.cfg`. Merge these values into that file, replacing existing keys rather than adding duplicates:

```ini
current_resolution_id = "4"
custom_viewport_width = "720"
custom_viewport_height = "480"
```

The tested installation used `/dev_hdd0/game/RETROARCH/USRDIR/retroarch.cfg`; other installations may use a different title directory. Restart RetroArch after editing. Keep the existing core paths, controller bindings and other settings. The release's `retroarch-sd-settings.cfg` contains these three values as a merge reference, not a complete replacement config.

**PROVEN (SDK/source):** native resolution ID 4 is 480 and ID 1 is 1080. [RetroArch 1.10.0's PS3 context](https://github.com/libretro/RetroArch/blob/v1.10.0/gfx/drivers_context/ps3_ctx.c) passes the configured resolution dimensions to PSGL. Source: PS3DK `cell/cell_video_out.h`, `CELL_VIDEO_OUT_RESOLUTION_*`.

The failing setup requested ID 1 and retained a 1280x720 custom viewport. The successful run logged GL 720x480 while the matched timing patch remained active. File/verbose logging was enabled during diagnosis; those logging options are not part of the three display settings above. Vsync and the RetroArch executable were unchanged. Source: the checkpoint's before/after configuration and runtime-log references.

**UNKNOWN:** compatibility with every RetroArch build, core, shader or output mode. The switcher PKG does not modify RetroArch settings automatically.
