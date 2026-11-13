# CRT black level, contrast and gamma

## What the controls mean

**CLAIMED (ITU-R BT.814-4, Annex 3):** traditional **Contrast/Picture** is
the gain control used to set the white output; **Brightness** is the black
level/lift control. Their names can be misleading. On a CRT there is no LCD
backlight: its phosphor emits light when excited by the electron beam.
Brightness is best described as a black-level adjustment, regardless of how
the particular TV implements its OSD controls.
[ITU-R BT.814-4](https://www.itu.int/dms_pubrec/itu-r/rec/bt/R-REC-BT.814-4-201807-I!!PDF-E.pdf).

| Control | Purpose |
|---|---|
| TV Brightness | Place signal black at the display's black threshold |
| TV Contrast/Picture | Set picture gain and the intensity of whites |
| CRT Toolset gamma | Remap intermediate pixel values before display |

**PROVEN (project source):** the tool's curve is `output = input^(1/value)`;
1.00 is neutral and higher values brighten intermediate tones. Exact input
black and white remain the curve's endpoints. This correction value is not
the same quantity as a measured CRT display gamma. Source:
`app/source/gamma.c:crt_gamma_ramp`, `plugin/vsh_gamma.c:vsh_gamma_set`.

## Practical adjustment order

**INFERRED (workflow combining the cited alignment guidance with this app):**

1. Let the CRT settle at normal operating temperature. Use your normal room
   lighting, input, picture mode and intended 240p/288p/stock SD setting.
   Start with native correction neutral and other gamma shaders disabled.
   Changing only the editor number does not disable an already-kept curve;
   use **Restore system gamma**, or apply/keep a native value of **1.00**.
2. Set **Contrast/Picture** to a comfortable normal viewing level using
   content with bright details. Avoid driving it until whites spread, fine
   detail softens or geometry changes. Those are useful CRT overdrive checks.
   [Spears & Munsil: Contrast](https://spearsandmunsil.com/portfolio-item/technical-notes-on-the-patterns/).
3. Adjust **Brightness** using a suitable near-black/PLUGE pattern. Bring it
   down until reference black stops looking visibly raised, while the
   just-above-black detail remains distinguishable. An entirely black screen
   alone cannot show when you have lowered brightness too far. The ITU SDR
   alignment procedure uses that above/below-black distinction after setting
   white output. [ITU-R BT.814-4, Annex 3](https://www.itu.int/dms_pubrec/itu-r/rec/bt/R-REC-BT.814-4-201807-I!!PDF-E.pdf).
   Match the pattern's setup to the connection: the original
   [240p Test Suite documentation](https://junkerhq.net/xrgb/index.php?title=240p_test_suite#PLUGE)
   distinguishes console RGB/PAL levels from NTSC video setup.
4. Use **Enter gamma value** and the comparison ramps to choose a correction.
   Begin near 1.00 and adjust gradually. Then choose **Test system gamma**;
   Cross keeps it before the timer ends. Judge shadow detail and ordinary
   pictures together; a single chosen number is not a measurement.
5. Recheck black, shadow detail and bright scenes with the kept curve active.
   **After changing TV contrast or brightness, recheck black level first,
   then gamma.** Re-adjust gamma if the result has changed. The CRT-era
   BT.814-1 procedure explicitly anticipates repeated adjustment because
   the two TV controls interact.
   [ITU-R BT.814-1, Annex 2](https://www.itu.int/dms_pubrec/itu-r/rec/bt/R-REC-BT.814-1-199407-S!!PDF-E.pdf).

**PROVEN (renderer):** the built-in preview is a comparison of RGB ramps and
nonnegative shadow steps, not a standards-compliant PLUGE generator with
below-black bars. Its logical framebuffer black is RGB 0. Do not treat a
video-disc instruction that calls code 16 black as the numeric threshold for
these RGB patches. `app/source/ui_layout.c:ui_render_gamma`.

## Does contrast changing black mean the TV is faulty?

**CLAIMED (owner observation):** this CRT needs its OSD Brightness reduced to
obtain a dark black background, followed by gamma correction for the desired
shadows. Changing Contrast can require repeating the adjustment.

**CLAIMED (pattern authors):** Spears & Munsil's PLUGE High adds a bright area
specifically to check whether black remains stable; not every display holds
it constant. [PLUGE High](https://spearsandmunsil.com/portfolio-item/technical-notes-on-the-patterns/).
DisplayMate also describes CRT internal reflections lowering local contrast
and picture-level-dependent brightness response.
[CRT evaluation, items 7 and 9](https://www.displaymate.com/crt_test.html).

**INFERRED:** control interaction and black-level retention provide reasons
to recheck both dark and mixed bright/dark pictures. Needing another check
after moving Contrast does not, by itself, prove a defect. **UNKNOWN:** the
cause or severity on this particular TV; no model-specific circuit analysis
or measurements were performed. A substantial unstable shift needs that
kind of diagnosis rather than a guessed component replacement.

**PROVEN/INFERRED distinction:** the software curve leaves input RGB 0 at 0;
it cannot fix raised black caused downstream simply by remapping that zero.
It can lift nonzero shadow values, but cannot reconstruct source detail
already clipped to zero. These follow from the implemented curve. A physically
displayed black can still change with the TV's operating conditions; the
pixel formula alone does not establish constant screen luminance.
