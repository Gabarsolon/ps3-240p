# CECH-2004A v1.4.3 lookup failure report

Checkpoint: 2026-11-14. Reporter identity and console are not independently
verified. No claim of a model-specific fix or working test build is made.

**CLAIMED (external tester, message relayed by owner):** a PAL Slim
CECH-2004A (NOR), Evilnat 4.93 CEX + Cobra 8.5, with full CFW syscalls
enabled, refuses 240p and 288p in v1.4.3. Their supplied log reports
`PATCH ERROR: 4.93 sysmgr PID 10 not found; check CFW syscalls` after the
analog video query succeeds. Source: owner's relayed message, 2026-11-14.

**PROVEN (source):** `app/source/lv1_patch.c`, function `service_pages`,
reads only process-table slot 10 at `0x36cdb0 + 10*16 + 8`. The quoted
error conflates three failures: an out-of-bounds pointer, an unaligned
pointer, or a process header whose PID is not 10. It does not log the raw
readback. `plugin/vsh_gamma.c` also assumes slot/PID 10 for LUT readback.

**UNKNOWN:** whether this tester's sysmgr occupies another PID, LV1 reads
are unavailable, or the fixed 4.93 layout differs. Successful video queries
and an AV UART permission probe do not independently validate LV1 peeks;
they use different interfaces (`app/source/main.c`). NOR alone is not an
established cause. The current owner-tested console's observed PID 10 is
documented in `LV1_SYSMGR_SS_LOADADDR.md`; that observation is not a
universal PID guarantee. Earlier slot-3 prose in that historical document
is explicitly superseded by its correction at the top.

**INFERRED (proposed compatibility change):** bounded enumeration of the
known 32-entry process table could support a shifted PID, but accepting the
first domain or guessed PID would be unsafe. A candidate must have a valid
process/domain/page list, uniquely mapped ordinary-RAM anchors and an
immutable sysmgr identity checked against the supported private firmware.
Reject zero matches, ambiguity, unstable mappings and unknown firmware.
Keep all timing/encoder preflight and rollback checks.

**INFERRED (diagnostic requirement):** record bounded table/header readback
and distinguish unreadable transport, wrong process identity and unsupported
layout before attributing this to the hardware revision. Validate discovery
with synthetic relocated-PID, decoy, duplicate, malformed-list and denied-read
cases, then the owner's console. A CECH-2004A fix remains unverified until
the reporter tests a suitable build.

## Read-only diagnostic available

**PROVEN (implementation):** `tools/diagnose_sysmgr.py --ip CONSOLE_IP`
reads the known table, bounded process/domain/page metadata, a 32-byte
immutable entry hash, and two anchor pointers through webMAN HTTP peeks.
It accepts no arbitrary address, performs no writes or mode changes, and
rejects ambiguous identity matches. Firmware responses are saved only under
gitignored `local_only/sysmgr-diagnostic/`; share its `report.json`, not the
raw HTML. Source: `tools/diagnose_sysmgr.py`.

Run from this repository on a PC with Python 3 and webMAN already running:

```powershell
python tools/diagnose_sysmgr.py --ip YOUR_PS3_IP
```

**PROVEN (offline tests):** eight synthetic checks cover a relocated PID,
decoy domains, changed identity bytes, duplicate identities, cyclic lists,
out-of-bounds/MMIO refusal, zero tables, mismatched PID headers and malformed
HTTP views. Source: `tests/test_diagnose_sysmgr.py`.

**PROVEN (owner's console):** the read-only scan found processes in slots
0, 3, 5, 6 and 10, and matched only slot 10 to the private audited 4.93
sysmgr identity and both anchor pointers. Evidence: ignored
`local_only/sysmgr-diagnostic/20261114-132346/report.json`.
**UNKNOWN:** this external tester's result. This diagnostic is not a fix,
and the app still assumes PID 10 pending evidence for a safe compatibility
change. The unrelated experimental 540p build should not be used to test
this SD lookup report.
