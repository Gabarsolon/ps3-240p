# Checked sysmgr discovery / v1.4.4-rc1

Checkpoint: 2026-11-14. This is an SD compatibility test build. It does not
include the experimental 540p menu from v1.5.0-dev19.

## Change and evidence

**PROVEN (source):** `app/source/sysmgr.c::sysmgr_discover` scans all 32
process pointers in the supported 4.93 table instead of assuming PID 10.
Both the timing resolver (`app/source/lv1_patch.c::service_pages`) and
gamma readback (`plugin/vsh_gamma.c::read_exponents`) use it. The existing
timing/encoder target checks, transaction rollback and gamma native-call
readback remain in their respective callers.

**PROVEN (private binary / prior live evidence):** the 4.93 process table
is at physical `0x36cdb0`, with 32 entries of 16 bytes and the process
pointer at entry `+8`. A zero first word does not mean an empty slot.
The process header contains its PID at `+0` and domain pointer at `+0x18`;
the domain vtable identity is `0x35ecb8`. Sources: private
`local_only/fw493/lv1.elf`, constructor `0x29d614`, lookup `0x29c8d4`,
and the correction at the top of [LV1_SYSMGR_SS_LOADADDR.md](LV1_SYSMGR_SS_LOADADDR.md).

**PROVEN (private binary / production fingerprint audit):** the immutable
32-byte sysmgr entry at VA `0x80005e78` has SHA-256
`3ab766ca914a9fe2948403f023f051ff59415322767210bdbeb431875ead0e7e`
and FNV-1a-64 `d79673d8c39c85a6`, hashing bytes in big-endian word order.
Two additional pointer identities are VA `0xc000af68` → `0x80038ad8`
and VA `0xc000af70` → `0x80038b10`. Source: private
`local_only/analysis/p0i/modules/sysmgr_ss.elf` at those VAs;
`tools/test_app.py --firmware ...` checks the compiled production entry
fingerprint and rejects a changed byte at each of its 32 positions.
Public code stores hashes and pointer identities, not firmware instructions.

**PROVEN (implementation):** discovery requires the slot/header PID to
agree, a bounded domain, and a finite page list with at most 512 nodes.
Identity targets must map uniquely to whole ordinary-RAM pages within
`[0x1000, 0x02000000)`. MMIO mappings may be present in the list but are
never dereferenced. Every candidate needs both the entry fingerprint and
the two pointer identities. Zero or multiple verified matches refuse.
The selected process is traversed and identified again; its node/mapping
signature and all 32 process pointers must remain unchanged. Output page
count is zero on refusal. Source: `app/source/sysmgr.c`.

**INFERRED (limits of these checks):** repeated snapshots detect the tested
mapping/table changes; they do not lock hypervisor state or provide atomic
transactions. FNV fingerprints identify the audited image against accidental
differences, not an adversarial firmware. The fixed 4.93 table layout and
normal-RAM bound are retained. This does not add other-firmware or HEN support.

**PROVEN (diagnostics):** app logs include occupied-slot/domain/match counts,
slot-10 pointer and header PID, selected PID and page count. All-zero process
pointers, no verified identity, ambiguous identities and changing snapshots
have separate errors. The read callback has no independent transport-error
channel, so all-zero reads are reported as unavailable reads **or** an
unsupported layout, rather than proof that syscalls are disabled. Sources:
`app/source/lv1_patch.c::service_pages`, `app/source/sysmgr.c`.

## Validation

**PROVEN (synthetic host tests):** `tests/test_sysmgr.c` relocates the
service to every PID 0–31 and runs SD apply/exact undo. It rejects a decoy at
PID 10, duplicate valid identities, wrong header PID, changed entry bytes,
MMIO/overflow mappings, cycles, missing identity mappings, table/mapping/
identity changes between reads, zero reads and error-valued reads. Refused
resolves perform no writes. `tests/test_vsh_gamma.c` also exercises native
gamma apply/restore with a relocated service. Fixtures use self-authored
entry bytes, enabled only by `SYSMGR_SYNTHETIC_TEST` in the host-test build.

**PROVEN (regression checks):** the complete `tools/test_app.py --firmware ...`
run passed, including SD rollback tests, experimental HD recovery/mapping
tests, gamma tests, UI/startup scenarios and all 54 stock-target comparisons
against the private firmware. The RC helper compiled with strict warnings
and passed `tests/test_plugin.py build/pid-compat/plugin/vsh_240p.elf`.
`tools/build_ps3_app.py --pid-compatibility` decrypted and compared all five
PKG members against staging.

**PROVEN (owner's console, native PPU lookup only):** the packaged RC app's
read-only **Recheck timing memory** found exactly one identity, PID 10,
109 pages and all 54 timing targets through PS3MAPI. Both SD families were
stock. The discovery log spans approximately 15 ms on this run. Evidence:
ignored `local_only/pid-fix/native-app.log`, timestamps `2517761..2517776`;
the independent read-only scan is in ignored
`local_only/sysmgr-diagnostic/20261114-142623/report.json`.

**PROVEN (test cleanup):** the app exited normally, and the previous dev19
EBOOT/SFO were restored byte-for-byte. The resident helper was not replaced,
and the RC test applied no timing patches or saved video defaults. XMB was
visible afterward. Evidence: ignored `local_only/pid-fix/owner-test.json`
and `restored-xmb.png`.

**UNKNOWN:** the RC app's visual result on the owner's HDMI capture: the
virtual-camera feed was black while the app was open, despite responsive
input and completed frame/log activity. It returned to a visible XMB on
normal exit. This run validates native lookup, not the RC's visual output
or analog 240p/288p operation. No inference about a console freeze is made.

**UNKNOWN:** whether the CECH-2004A report was actually caused by a shifted
PID, and whether this RC resolves it. There is no test result from that
console yet. A valid alternate PID is covered synthetically, not demonstrated
on different physical hardware. See [the original report](CECH2004A_LOOKUP_REPORT.md).

## Tester instructions

1. Install `CRT-Toolset-1.4.4-rc1.pkg`, then reboot once to clear an older
   resident helper. Enable the usual CFW/Cobra syscalls.
2. Use your normal supported analog SD setup. Open CRT Toolset and confirm
   its title says **v1.4.4-rc1**.
3. Select **Recheck timing memory** first. If it fails, send
   `/dev_hdd0/tmp/ps3_240p.log`; do not keep retrying video tests.
4. If memory is recognized, try the timed **Test 240p** or **Test PAL 288p**.
   Leave the controller untouched to check the automatic 15-second return.
5. Report the model, firmware/Cobra versions, cable/output, picture result
   and app log. Only then try background enforcement if desired.

**PROVEN (build isolation):** these commands write under `build/pid-compat/`
and preserve the stable release package/helper and existing HD build outputs:

```powershell
python tools/build_plugin.py --pid-compatibility
python tools/build_ps3_app.py --pid-compatibility
```

**PROVEN (local artifact):** `build/pid-compat/deliverables/CRT-Toolset-1.4.4-rc1.pkg`,
255568 bytes, SHA-256
`e35062e49bdb6a3cb785442c49d5e8e855389d44d92590fd2bfa3aba5614165e`.
Source: package builder's decrypted-member verification and manifest in
that directory. It is a local test build, not a published stable release.
