# AGENTS.md: ps3-240p

Research project: native 240p/288p analog output on a PS3 (CFW Evilnat 4.93 + Cobra, slim CECH-25xx; fat models for comparison). Owner: Gabarsolon (hobbyist, own console).

## Rules
- The owner authorizes project-related PS3 access, installers, payloads, and reversible live tests without repeated permission prompts. Use backups, checked writes, and automatic rollback/timed restoration for experiments.
- The owner reaffirmed this authorization on 2026-11-14: "yes install, stop askking me". This supersedes the older read-only/no-installers instructions supplied in chat; do not ask again for authorized project installations or reversible tests.
- The owner also authorizes automated test sequences, capture inspection, and webMAN reboot/controller interaction (2026-11-14: "run the tests from now on automatically" and "u can use webman to reboot the console and interact with it"). Prefer the guarded helper protocol for XMB tests and normal soft reboots when needed. Keep the no-brick exclusions below.
- Do not perform operations that might brick the console. Exclude flash/bootloader writes, firmware installation/downgrade, and Syscon/EEPROM firmware programming. If brick risk is unresolved, investigate offline and choose a reversible approach before console execution.
- Sony-derived files (firmware, PUPs, decrypted ELFs, decompiler output) stay in `local_only/` (gitignored). Docs hold addresses and short descriptions only; no instruction listings or code dumps from Sony binaries. This repository is PUBLIC.
- Label every claim PROVEN (checked in a binary or source by you) / INFERRED / CLAIMED (a source says so) / UNKNOWN, and cite URL + quote or file + address. Unsourced claims are not acceptable.
- Tools (this PC): Ghidra 12.1.3 `C:\ghidra_12.1.3_PUBLIC`, JDK 21, pyghidra (not capstone), one JVM at a time with `-Xmx2500m`. Firmware images and unpack tools live in the sibling project `<ps2-netemu-disc workspace>` (read only from here): `local_only/fw493/lv1.elf`, `local_only/fw355/out/*.elf`, `tools/core_os_extract.py`, `tools/pup_extract.py`, `tools/p0h_*.py`, `tools/p0i_*.py`.
- Progress tracking: run `python -I tools/update_progress.py --note "..." [--set "<stage>=<pct>"]` to update `docs/progress.json`, `docs/progress.svg`, and `README.md` after milestones.
- Short answers; the owner types casually.
- For HEN research, read `docs/HEN_PORT_PLAN.md` before capability probing:
  the inspected upstream LV1 peek implementation invokes a poke-based
  precheck. Do not use the CFW LV1 inspection tools as "read-only" HEN
  diagnostics without auditing the actual backend. Initial HEN diagnostics
  must issue no LV1 operations.

## Automated HDMI testing workflow

- The owner explicitly renewed automated testing authorization on 2026-11-14
  with the PS3 connected over HDMI and OBS Virtual Camera available. Continue
  within the reversible-test authorization above; no repeated permission
  prompts. Investigate unresolved brick risk offline before execution.
- **PROVEN (project implementation):** the 540p workflow is documented in
  `docs/HD540_AUTOMATION.md`; the runner is `tools/hd540_auto_test.py` and the
  final successful candidate is documented in `docs/HD540_DEV19.md`. Reuse
  its guards and evidence collection for new display experiments; its old
  negative candidate report is historical, not the final dev19 result.
- Start with read-only checks: FTP/webMAN reachability, actual installed
  package/helper identity, live VSH plugin slots, active app/enforcement
  flags, pending requests/replies and baseline picture. Stale ready files
  alone do not prove a running helper. The installed SD PID build and the
  separate HD build have different capabilities; never infer compatibility
  from a shared plugin filename.
- Use the owner's OBS capture-card scene through **OBS Virtual Camera**;
  the established input is 2560x1440 at 60 fps, NV12. For picture tests record
  with FFmpeg `-an` (no audio), and keep OBS open. Separate audio diagnostics
  should use console/capture-card audio only, never a microphone or ambient
  recording. Do not open the physical card's
  video interface concurrently with OBS. Save frames, timestamps, logs,
  helper states and reports under ignored `local_only/`.
- `tools/capture_input_status.py --json` supplies the cached input report;
  `--scan-once --json` also performs a live scan query that may interrupt
  capture. Take sparse scan samples separately from undisturbed picture
  checks; do not continuously poll the live register. A report of 1920x540
  alone may describe one 1080i field. Scan raw 0x0f/0x07 has an upstream
  interlaced/progressive interpretation; exact firmware mapping and refresh
  remain unvalidated. A configured OBS capture size is not input timing.
  A cached size can remain stale even with visible video: the WUXGA test
  reported cached 1920x1080 but live 1920x1200. Use
  `--timing-once --json` only for a separate, sparse diagnostic: it reads
  live scan plus two dimension words and may interrupt the USB capture.
  It cannot be combined with `--watch`. Preserve both cached/live fields.
  See `docs/PC_PRESETS_DEVELOPMENT.md` and `docs/CAPTURE_INPUT_STATUS.md`.
- For deployments, exit the app normally, ensure no experiment is active,
  back up affected files, stage and read back complete replacements, and
  verify hashes. The owner explicitly reaffirmed webMAN PKG installation
  on 2026-11-14; use it for project packages without asking again.
  **CLAIMED (official webMAN documentation):** after uploading a checked PKG
  by FTP, request `http://<PS3-IP>/install_ps3/dev_hdd0/packages/<file>.pkg`
  to install and **keep** the package. `/install.ps3/dev_hdd0/packages/<file>.pkg`
  installs and **deletes** the package afterward. Source:
  https://github.com/aldostools/webMAN-MOD/wiki/Web-Commands#xmb
  ("install & keep the pkg"). Prefer the keep variant and URL-encode filenames.
  Start from idle XMB with no active experiment or pending restoration;
  verify installed files and their hashes afterward. An HTTP response alone
  does not prove installation. Earlier remote-installer attempts froze this
  console: make one bounded attempt, inspect capture and installation state,
  and do not repeat blindly on timeout. **PROVEN (2026-11-14 live check):**
  the keep variant installed `CRT-Public-Video-Diagnostic-0.1.0.pkg` on this
  console; XMB reported completion, all four installed files matched the
  package, and the staged PKG remained. Evidence:
  `local_only/display_presets/public-diagnostic-ui/{install-webman.html,installed.json}`.
  Load a verified helper only
  into an independently confirmed empty slot; never evict another plugin
  or modify the boot-plugin list for a test.
- A live experiment needs a console-side, bounded restoration path that
  survives a lost PC connection. Use fresh request sequence IDs, staged
  complete requests, matching ABI/readiness, acknowledgement checks and
  snapshot ownership. Never overwrite an unacknowledged request. Hold a
  candidate for 15 seconds after readiness, then restore automatically.
  On timeout/interruption request cancel, wait for bounded recovery and stop
  the sequence if restoration is unconfirmed. Do not retry blindly.
- Establish a stock control before a changed candidate. The existing HD
  helper requires both the stock 720p/1080i round trip and the stock-720p
  AVI-zero control before its 540p candidate; those prerequisites apply to
  that protocol, not automatically to unrelated experiments. Actual menu
  invocation must be tested separately from direct helper requests.
- Verify software completion, independent receiver status, visible geometry
  and stock restoration separately. Pixel statistics detect a lit/textured
  frame, not readability, correct scaling, scan cadence or reduced latency.
  Inspect real saved frames; do not substitute generated previews. XMB
  success does not establish app/game compatibility.
- webMAN virtual pad input and normal soft reboot are authorized. Use
  `/pad.ps3?release` after presses and `/reboot.ps3?soft` when needed. Restore
  owned RAM hooks before a VSH-only restart: it need not reset LV1 state.
  Never reboot through unverified pending recovery or alter saved boot
  settings as part of an experimental display test.
