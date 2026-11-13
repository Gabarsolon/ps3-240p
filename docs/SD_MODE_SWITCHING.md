# Live SD selection and the saved boot default

## Recovered Gemini procedure

**PROVEN (original tool transcript, 2026-11-11):** Gemini read
`/setting/display/tvSystem` as `1`, changed only that named setting to `0`
through webMAN, and read it back as `0`. It then asked the owner to reboot.
**CLAIMED (owner's next message):** the console booted into 480i.

Private evidence: Antigravity conversation
`e141753d-3365-4bc7-88ab-1eed2e275623`,
`.system_generated/logs/transcript_full.jsonl`, steps 3326-3341.
The transcript and console registry remain outside the public repository.

Exact requests, with `<PS3-IP>` replaced by the console's address:

```text
Read:  http://<PS3-IP>/xmb.ps3$xregistry(/setting/display/tvSystem)
480i:  http://<PS3-IP>/xmb.ps3$xregistry(/setting/display/tvSystem)=0
576i:  http://<PS3-IP>/xmb.ps3$xregistry(/setting/display/tvSystem)=1
```

Read again after writing. PowerShell URLs must use single quotes to preserve
the literal `$xregistry`. The earlier URL without `$xregistry` was incorrect.
The corrected read-only helper is `tools/check_ps3.py`.

**CLAIMED (webMAN issue):** `tvSystem=0` selects the NTSC display default;
the PAL default on this console was `1`. This is a display setting, not a
console region or firmware conversion. Source:
[webMAN issue 136](https://github.com/aldostools/webMAN-MOD/issues/136).

**PROVEN (webMAN source):** named keys require `DEBUG_XREGISTRY` and administrator
access. The command finds the named entry and updates its value through
`get_xreg_value`; it does not upload a replacement registry file.
Without that feature, the numeric-ID handler can interpret a named key as
ID zero. Do not assume an HTTP 200 or a generic OK means the key was changed.
Sources at webMAN commit `4b4e41aefee06154a7bb78c57f3385865dbc5bdf`:
[command handler](https://github.com/aldostools/webMAN-MOD/blob/4b4e41aefee06154a7bb78c57f3385865dbc5bdf/include/cmd/xmb_browser.h),
[named-key implementation](https://github.com/aldostools/webMAN-MOD/blob/4b4e41aefee06154a7bb78c57f3385865dbc5bdf/include/feat/xregistry.h).

## Is a reboot required?

**PROVEN (implementation and previous console transition logs):** the app
can request public resolution ID 4 (480i) or 5 (576i) while running, using
`videoConfigure` and waiting for matching enabled-state readback. The existing
240p transition already passes through both SD families without rebooting.
Source: `app/source/main.c:configure`, `app/source/switch_flow.c`;
private 2026-11-13 field-timing captures referenced in
[the geometry checkpoint](GEOMETRY_STATUS_2026-11-13.md).

**INFERRED:** Gemini's reboot applied the saved default during startup; it
was not an inherent requirement of changing the live analog timing.
**UNKNOWN:** whether changing the registry alone makes every running XMB/game
reload its cached choice immediately. Games can request their own video modes.

## Development build

**PROVEN (source/host tests):** the development menu offers explicit stock
480i and 576i choices. Each restores stock timing records, requests the selected
SD mode, and gives a 15-second picture confirmation. Accepting also saves
`tvSystem=0` or `1` through local webMAN and reads it back. Cancel/timeout
restores the previous timing snapshot and mode without changing the saved default.
The background low-line corrector is stopped after a successful stock selection.
In dev6, stopping correction means disarming its flag; an unchanged resident
worker is retained for reuse rather than unloaded on every mode selection.

**PROVEN (console response captured during development):** this webMAN build's
setter replies with a bare decimal, while the getter includes decimal and hex.
Dev2 wrongly rejected the setter's shorter reply and rolled back a successful
576i save. Dev3 verifies the setting through a separate getter after every write;
the setter's echo is not treated as persistence proof. Source:
`app/source/sd_default.c`; private
`local_only/activation/pal288-development/dev2-direct/registry-write0.html`
and `registry-read.html`. Tests cover bare setter replies, lost replies after
a successful write, failed writes, and failed independent readback/rollback.

**PROVEN (source/host tests):** before any registry write, two different named
SD keys must give different valid results, preventing the numeric-ID fallback
from being mistaken for named-key support. Failed/ambiguous writes attempt
to restore and verify the original value. Sources: `app/source/sd_default.c`,
`tests/test_sd.c`, `tests/test_patch.c`.

**PROVEN (dev3 console checks, 2026-11-13):** selecting stock 576i produced
the stock 574-active / 625-total analog raster and saved/read back `tvSystem=1`.
Selecting stock 480i then produced 480 active / 525 total and saved/read back
`tvSystem=0`. Both actions ran in the same app session without rebooting,
and stopped the low-line helper. Private evidence:
`local_only/activation/pal288-development/dev3/stock576-save.{log,json}` and
`stock480-save.{log,json}`.

**UNKNOWN:** a fresh reboot test of the new 576i choice. The earlier Gemini
session already established the 480i boot default, as recorded above.
