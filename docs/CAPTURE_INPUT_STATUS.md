# Capture card input status

**PROVEN (local USB enumeration):** the owner's UltraSemi USB3 Video exposes
vendor HID interface 4, VID:PID `345f:2131`, device release `3100`.
`tools/capture_input_status.py` selects only this device/interface/release.

Run from the repository on the PC, with OBS still open:

```powershell
python tools/capture_input_status.py
```

For repeated reports every two seconds, stop with Ctrl+C:

```powershell
python tools/capture_input_status.py --watch 2
```

`--json` prints machine-readable samples. Dependency: `hidapi`
(`python -m pip install hidapi` if missing).

To query the live interlace flag once:

```powershell
python tools/capture_input_status.py --scan-once
```

**PROVEN (implementation):** `--scan-once` sends one additional RAM-read
request at `0xe180` and interprets the first returned status byte. The command
rejects use together with `--watch`. Default cached polling remains unchanged.
This live-register query can interrupt capture; it is deliberately not polled.

**PROVEN (implementation):** the script sends only the `0xb5` RAM-read
command, validates the echoed request header, and limits reads to chip-family
register `0xf800`, cached size `0xf660` (four bytes), and cached signal byte
`0xf6e9`, plus the explicit one-shot scan query above. It does not install
patches or write RAM/EEPROM/firmware. No PS3
connection is made. Protocol source:
[ms-tools hal_rom.go](https://github.com/BertoldVdb/ms-tools/blob/master/mshal/hal_rom.go)
and [hal_region.go](https://github.com/BertoldVdb/ms-tools/blob/master/mshal/hal_region.go).

**CLAIMED (upstream):** the cached size/signal locations are less reliable
than live registers. Reads of live HDMI registers can disturb capture; the
script therefore excludes that block from continuous polling and never calls
upstream firmware-patch functions.
Slow polling is optional, not a guarantee of uninterrupted capture. Sources:
[status.go, readFlaky/readMurderous](https://github.com/FOSDEM/video-ms213x-status/blob/main/status.go)
and [upstream investigation](https://github.com/BertoldVdb/ms-tools/issues/14).

**PROVEN (one-shot local observation, 2026-11-14):** the card returned chip
byte `00`, size bytes `80 07 1c 02` (1920 by 540), and signal byte `00`
(present under the upstream mapping). The subsequent script read returned
the same values. Evidence: ignored `local_only/sources/capture-status/` and
`local_only/hd540/dev8/capture-input-status.json`.

**INFERRED:** 540 reported lines can represent one field of the recovered
stock 1080i signal. They are not evidence of 540p output. The PS3's preceding
dev8 helper log restored stock mode 10 and AVI code 5; see
[dev8 live result](HD540_DEV8.md).

**CLAIMED (upstream observation):** status byte `0x0f` at `0xe180` means
interlaced input, `0x07` progressive input, and `0x00`/`0x08` no signal.
Source: [status.go, readMurderous](https://github.com/FOSDEM/video-ms213x-status/blob/main/status.go),
comment: "interlaced signal: 0x0f". This is a reverse-engineered mapping,
not a manufacturer guarantee for every firmware.

**PROVEN (one-shot local observation, after owner-reported console reboot):**
the live flag returned `0x0f`. Evidence: ignored
`local_only/hd540/dev8/capture-scan-status.json`, 2026-11-14 10:52:33 UTC.
**INFERRED:** the card is receiving interlaced 1920x1080, with the cached
height reporting 540 lines per field. This observation supports the stock
1080i interpretation; the reboot did not establish working 540p.

**UNKNOWN:** this capture firmware's precise cached-field semantics,
freshness during signal loss and refresh rate. The default cached-only
script reports scan type as unknown; `--scan-once` labels the interpretation's
source and retains the raw byte. Its size is not
OBS's configured capture-stream size, but neither is it an independent
measurement proving the physical HDMI timing. A stale size can remain
visible after loss of lock.

**CLAIMED (owner, dev9 live control):** the same status query reported
1280x720 / `0x07` during the readable stock-720p AVI-zero control, then
1920x540 / `0x0f` on return to 1080i. **PROVEN (helper logs):** the native
720p head/AVI-zero hold and stock-1080i restoration completed without error.
Together these observations support the upstream scan-flag interpretation
on this board for these two modes; they do not validate other firmware or
refresh-rate measurements. See [dev9 live result](HD540_DEV9.md).
