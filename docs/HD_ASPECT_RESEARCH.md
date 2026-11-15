# 4:3 content versus a 4:3 output raster

Owner follow-up, 2026-11-15. This is research alongside the in-app preview
work, not a new compatibility claim or authorization for flash-file edits.

**CLAIMED (Sony user guide):** the TV-type selection is offered when only
480p/480i or 576p/576i is selected. Thus the stock 4:3 selector is an SD
path; selecting a higher raster does not supply a stock HD 4:3 menu.
Source: [Video Output Settings, step 7](https://manuals.playstation.net/document/en/ps3/3_10/settings/videooutput.html).

**PROVEN (project trial):** the native PC mapper rejected aspect argument
1 for the XGA-derived mode, returning `-1` and zero flags/mode before any
source/timing staging. The accepted path uses aspect argument 2 and the
logical 1280 x 720 canvas. The 1024 x 768 active raster is 4:3, but that
does not establish correctly proportioned 4:3 XMB content on a physical
4:3 display. Source: [XGA evidence](XGA_DEVELOPMENT.md), sequence
`567754447`, ignored `20261115-012545/xga-native.log`.

**PROVEN (primary project source):** RPCS3 reads `PARAM.SFO`'s `RESOLUTION`
mask to check advertised resolution support. Its map includes separate
480/480-widescreen and 576/576-widescreen flags, but single 720 and 1080
flags. This is source evidence about its interpretation of the format,
not a retail-console test. Source:
[RPCS3 resolution selection](https://github.com/RPCS3/rpcs3/blob/master/rpcs3/Emu/System.cpp),
the `psf_resolution` check;
[format definitions](https://github.com/RPCS3/rpcs3/blob/master/rpcs3/Loader/PSF.h).

**INFERRED:** changing advertised metadata alone cannot guarantee a game's
camera projection, HUD layout or hardcoded render-target aspect changes.
A game that queries the system aspect and has a 4:3 renderer may follow an
override; a widescreen-only renderer needs aspect-preserving scaling/borders,
cropping or a game-specific patch. These alternatives must be distinguished
from compressing a 16:9 image to fill a 4:3 raster.

**UNKNOWN / investigation targets:** whether a checked temporary VSH aspect
override can select a coherent 4:3 layout at a PC timing, whether existing
layout tables can supply the correct logical canvas without flash writes,
which game queries honor the aspect value, and the effect on actual geometry.
Use a circle/grid pattern and native UI/game readback before claiming success.
