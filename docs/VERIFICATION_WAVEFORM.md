# Hardware Waveform Verification Protocol: Native 240p/288p Validation

**Status: SPECIFICATION READY FOR HARDWARE MEASUREMENT**  
**Date: 2026-11-11**  
**Target Console: PS3 Slim CECH-25xx, Evilnat 4.93 CFW + Cobra, MultiAV Port**  

---

## 1. Overview & Purpose

This protocol specifies the hardware measurement procedures to definitively confirm native 240p (NTSC) or 288p (PAL) analog video output from the PlayStation 3's MultiAV Out port following application of the in-memory LV1 timing patch.

A software configuration alone does not prove native analog video. Native 240p/288p requires physical electrical confirmation:
1. **Line Frequency ($f_H$):** Standard 15 kHz SD rate (~15.734 kHz for NTSC, ~15.625 kHz for PAL).
2. **Field Line Count:** Integer line count per field (262 lines for NTSC 240p; 312 lines for PAL 288p).
3. **Field Synchronization Phase:** Complete absence of half-line ($0.5\ H$) phase shift between successive fields.
4. **Visual Scanline Stability:** Static CRT scanlines without interlace bob flicker or line structure alternation.

---

## 2. Test Equipment & Signal Interfacing

### 2.1 Supported Output Cables & Pinouts

Measurements can be performed using standard PlayStation MultiAV cables:

| Pin | Signal | Component ($YPbPr$) Role | RGB ($RGBS$/$RGsB$) Role | Composite ($CVBS$) Role |
|:---:|---|---|---|---|
| **6** | Composite / Sync | Unused (or composite video) | **Composite Sync (CSync)** | **Composite Video ($CVBS$)** |
| **8** | Video Ground | Ground reference | Ground reference | Ground reference |
| **9** | Blue ($Pb / B$) | $Pb$ / Color Difference ($B - Y$) | Analog Blue ($0.7\ V_{p-p}$) | Unused |
| **11**| Red ($Pr / R$) | $Pr$ / Color Difference ($R - Y$) | Analog Red ($0.7\ V_{p-p}$) | Unused |
| **12**| Green ($Y / G$) | **Luma ($Y$) with Bilevel Sync** | Analog Green (or Sync-on-Green)| Unused |

### 2.2 Recommended Test Instrumentation

1. **Oscilloscope / Logic Analyzer (Primary Ground Truth):**
   - Minimum bandwidth: 50 MHz.
   - Input impedance: 75 $\Omega$ feed-through terminator (or $1\ \text{M}\Omega$ probe connected to a 75 $\Omega$ terminated monitor).
   - Probe point: Pin 12 ($Y$ channel) for Component, or Pin 6 (CSync) for RGB.
2. **Retro Video Scaler (Digital Protocol Verification):**
   - Open Source Scan Converter (OSSC) or RetroTINK-5X Pro / RetroTINK-4K.
   - OSSC OLED display or RetroTINK OSD displays exact line count ($262p$ vs $525i$) and vertical refresh.
3. **CRT Television / Broadcast Monitor (PVM / BVM):**
   - Visual inspection of 240p scanline beam gaps and absence of interlace jitter.

---

## 3. Waveform Verification Criteria

### 3.1 Horizontal Timing Measurements ($T_H$)

Trigger on negative-going sync edge:

| Parameter | Standard 480i | Native 240p Target | Tolerated Margin |
|---|---:|---:|:---:|
| **Horizontal Period ($T_H$)** | $63.555\ \mu\text{s}$ | **$63.555\ \mu\text{s}$** | $\pm 0.100\ \mu\text{s}$ |
| **Horizontal Frequency ($f_H$)** | $15.734\text{ kHz}$ | **$15.734\text{ kHz}$** | $\pm 0.025\text{ kHz}$ |
| **HSync Pulse Width ($T_{sync}$)** | $4.700\ \mu\text{s}$ | **$4.700\ \mu\text{s}$** | $\pm 0.300\ \mu\text{s}$ |
| **Back Porch / Color Burst** | $4.710\ \mu\text{s}$ | **$4.710\ \mu\text{s}$** | $\pm 0.500\ \mu\text{s}$ |

### 3.2 Vertical Cadence & Field Phase (The Definitive Test)

In standard interlaced video (480i), an odd field starts with a full line and ends with a half line ($0.5\ H = 31.777\ \mu\text{s}$), causing the vertical sync of the subsequent even field to occur halfway through a scanline.

In native progressive video (240p):
- **Lines Per Field:** Exactly **262 integer lines** (never 262.5 or 525).
- **Field Phase Shift:** Exactly **$0.0\ \mu\text{s}$**. Every VSync begins precisely aligned with the start of an HSync pulse.
- **Equalizing Pulses:** In native 240p, equalizing pulses and half-line serration during the vertical blanking interval are either disabled or symmetric, preventing display deflection coils from shifting the raster down by half a track.

```
480i Interlaced Cadence:
Field 1: |--- H ---|--- H ---|--- H ---|-- 0.5H --| VSYNC (shifted by 31.78 us)
Field 2: |-- 0.5H --|--- H ---|--- H ---|--- H ---| VSYNC (aligned to line start)

240p Progressive Cadence:
Field 1: |--- H ---|--- H ---|--- H ---|--- H ---| VSYNC (aligned to line start)
Field 2: |--- H ---|--- H ---|--- H ---|--- H ---| VSYNC (aligned to line start, 0 us shift)
```

---

## 4. Owner Step-by-Step Execution Procedure

### Step 1: Baseline Readout (Pre-Patch)
1. Power on the PlayStation 3 with the MultiAV cable connected to the test setup.
2. Configure console display settings to standard 480i (Composite or Component).
3. On OSSC / RetroTINK, confirm baseline report: `480i @ 59.94Hz` (15.73 kHz / 525 lines).
4. On oscilloscope, capture the VSync transition and observe the alternating $31.8\ \mu\text{s}$ field shift.

### Step 2: Apply the In-Memory Timing Override
1. Open PowerShell on the LAN workstation.
2. Execute the verification check and patch:
   ```powershell
   powershell -ExecutionPolicy Bypass -File tools/apply_240p_patch.ps1 -ConsoleIP "192.168.0.169" -Apply -Mode "240p"
   ```
3. Confirm that all three targets (Metadata, Raster, Encoder) report `VERIFIED`.

### Step 3: Trigger Hardware Register Reload
1. On the PS3 XMB:
   - Go to `Settings` $\rightarrow$ `Display Settings` $\rightarrow$ `Video Output Settings`.
   - Select `Component / D-Terminal` (or `Composite / S-Video`).
   - Change resolution to `480p` (or `720p`), then immediately switch back to `Standard (NTSC) / 480i`.
   - Alternatively, boot a PS1 or PS2 classic game running under emulator.
2. `sysmgr_ss` will execute `0x800005e4` using the patched Record 1, writing 240p parameters to RSX CRTC registers and disabling interlace in the CXM4027R encoder.

### Step 4: Measure & Record Results
1. **OSSC / RetroTINK Check:**
   - Verify whether the OSD transitions from `480i` to `240p` (`15.73 kHz / 60.05 Hz`, $262p$).
2. **Oscilloscope Measurement:**
   - Measure $T_H$ on Pin 12 ($Y$): verify $63.55\ \mu\text{s}$.
   - Measure vertical line count: count HSync pulses between successive VSync falling edges. Confirm exactly 262 pulses.
   - Verify $0.0\ \mu\text{s}$ phase offset between successive fields.
3. **CRT Inspection:**
   - Observe image: verify clear, static black scanlines between active video rows without interlace flicker.

### Step 5: Restore / Clean Up
- To restore stock timing in RAM:
  ```powershell
  powershell -ExecutionPolicy Bypass -File tools/apply_240p_patch.ps1 -ConsoleIP "192.168.0.169" -Restore -Mode "240p"
  ```
- Or simply power off / restart the console (volatile memory completely clears on reboot).

---

## 5. Result Log Template

| Test | Expected Value | Measured Value | Result (PASS / FAIL) |
|---|---|---|:---:|
| Horizontal Line Period ($T_H$) | $63.555\ \mu\text{s}$ | | |
| Horizontal Frequency ($f_H$) | $15.734\text{ kHz}$ | | |
| Field Line Count | 262 lines | | |
| Field Phase Offset | $0.0\ \mu\text{s}$ | | |
| Scaler OSD Identification | `240p` / `262p` | | |
| CRT Scanline Rendering | Static non-interlaced | | |
