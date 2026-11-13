# v1.4.3 - Native analog gamma correction

CRT Toolset now adjusts gamma through the PS3's native AV service. Enter a
correction value, preview the ramps, then test and keep the native curve.
The existing 240p/288p and stock 480i/576i features remain available.

## Changes

**PROVEN (release source):** `app/source/gamma.c`, `gamma_ipc.c`, `main.c`,
`ui_layout.c`, `plugin/vsh_gamma.c`, and `plugin/gamma_service.c` implement:

- Gamma values from **0.50 to 4.00**, edited to two decimal places. **1.00 is
  neutral; higher values brighten shadows.** There is no fixed 2.20 preset.
- Original/corrected grayscale ramps and near-black comparison steps.
- Native analog gamma with a **15-second recovery timer in the VSH helper**.
  Cross keeps a trial; Circle restores the previous curve. The foreground
  confirmation window is shorter to leave time for acknowledgement.
- Cancellation of a new trial preserves a previously kept correction.
  **Restore system gamma** restores the original session baseline.
- A saved editor preference. Saving the number does not apply gamma at boot.
- Optional self-contained RetroArch Cg shader/preset export, independent of
  native correction; existing RetroArch configuration is not edited.
- Repeated command-file publication on LV2 and distinct filesystem, timeout
  and AV errors. Repeated preference saves preserve the old file while
  replacing it on filesystems that refuse rename-over-existing.

Native correction affects scanout rather than only the app's preview. It
uses the audited VSH AV LUT path; the implementation still uses LV1 reads
for format guards, the prior curve and readback. [Technical evidence](https://github.com/Gabarsolon/ps3-240p/blob/v1.4.3/docs/GAMMA.md).

## Calibrate the TV first

Set your usual **Contrast/Picture**, then adjust **Brightness** so reference
black is dark while just-above-black detail remains visible; a black screen
alone can hide over-adjustment. Then use the gamma tool for the desired
shadow/midtone response. **After changing TV contrast or brightness, recheck
black level and gamma.** The app's About screen includes this guidance.

**CLAIMED (ITU alignment guidance):** CRT Brightness adjusts black level,
while Contrast controls gain/white output. The controls can interact; this
observation alone does not diagnose a faulty TV. The built-in RGB comparison
ramps are not a full PLUGE generator.
[Calibration instructions, research and TV-specific limits](https://github.com/Gabarsolon/ps3-240p/blob/v1.4.3/docs/CRT_CALIBRATION.md).

## Validation and limits

**CLAIMED (owner, Evilnat 4.93 CEX + Cobra / analog CRT):** gamma works
"perfect so far" and works together with both 240p and 288p. This is an
owner-observed result, not a measured transfer curve or exhaustive game test.

**PROVEN (live development checks):** the native AV call and independent
three-channel readback succeeded, followed by restoration of the previous
curve. Logs also established normal timed recovery while the PS menu was
open. The repeated-command fix was checked through live acknowledgements.

**PROVEN (release checks):** gamma math, preference replacement, repeated
command publication, recovery/rollback, native guards, timing/settings/plugin
lifecycle and UI tests pass; all 21 app startup/menu/overlay scenarios pass.
The compiled helper passes its loader-thread exit audit, all 54 timing targets
still match the private 4.93 firmware input, and the finalized package matches
its staged members. The helper is byte-identical to the owner-tested dev3
binary. The final app adds release-version/calibration help text and the preference-file
replacement fix; it was checked offline rather than installed again.

**UNKNOWN:** measured gamma response, electrical field parity, extended
stability and broader firmware/model/game/core compatibility. The exported Cg
shader still needs a RetroArch execution test. A game or video-mode change
may reset a kept native curve; this version does not repeatedly force it back.

## Install and use

1. Exit the app and install **CRT-Toolset.pkg** through normal XMB Package
   Manager. Reboot after upgrading to clear older resident helpers. The title ID
   remains `PS240P001`. Package version is **01.43**.
2. Use supported CFW/Cobra and AV Multi Out in 480i/576i or the toolset's
   240p/288p mode. Native gamma currently guards the audited analog RGB8 SD
   path; other output formats are not established support.
3. Open **Gamma / shadow correction > Enter gamma value**. Left/Right chooses
   a digit; Up/Down changes its place value. Cross accepts the editor value.
4. Choose **Test system gamma**, then press Cross during the countdown to
   keep it. Circle cancels. **The timer continues while the PS menu is open.**
5. Use **Restore system gamma** to undo the session correction. Saving the
   editor preference is separate; reboot clears the runtime gamma setting.

Do not combine native correction with a gamma shader: both would apply.
Gamma preserves exact black and cannot recover pixel detail already clipped
to zero. It is a correction option, not a hardware repair.

**PROVEN (current implementation):** ordinary HEN remains unsupported. Gamma
and stock-mode HEN ports are research candidates, not features of this build.
The core helper does not require webMAN; saving 480i/576i boot defaults still
uses webMAN's named-key interface. [Compatibility](https://github.com/Gabarsolon/ps3-240p/blob/v1.4.3/docs/HEN_SUPPORT.md).

## Assets

- **CRT-Toolset.pkg** - release installer.
- **CRT-Toolset-menu.png** and **CRT-Toolset-gamma-preview.png** - PC previews
  generated by the production renderer.
- **package_manifest.json** and **SHA256SUMS.txt** - package/member and asset
  verification data.
- **retroarch-sd-settings.cfg** - the existing tested **NTSC 240p / 720x480**
  settings fragment; not a full RetroArch config or a validated PAL preset.

Package size and SHA-256 are recorded in the attached manifest and checksums.
