# CFW and HEN compatibility

**PROVEN (project source):** the timing patcher needs working LV1 reads and
writes to the live `sysmgr_ss` timing/encoder records. It tries PS3MAPI LV1
peek/poke, then CFW syscalls 8/9, and requires the supported 4.93 memory layout
and recognized record values before writing. Cobra-style plugin loading alone
does not supply that LV1 access. Sources: `app/source/main.c:inspect`,
`app/source/lv1_patch.c:patch_resolve`.

**CLAIMED (owner tests):** v1.2.2 works on Evilnat 4.93 CEX + Cobra 8.5,
including XMB, PS1 `ps1_emu.self`, and the tested RetroArch configuration.
See [the test checkpoint](GEOMETRY_STATUS_2026-11-13.md).
**UNKNOWN:** other CFW/firmware/model combinations until separately checked.

**PROVEN (upstream source inspected 2026-11-13):** PS3HEN's LV1 peek/poke
PS3MAPI cases are marked `BadWDSD only`. The additional installation path
checks `BadWDSD_IsExploited` before installing hypervisor calls. This does not
provide ordinary HEN installations with general LV1 read/write access.
Source: PS3HEN commit `6c529455261db97a0f9028fe96db522a97fd5ccc`,
[`payload/main.c`](https://github.com/PS3Xploit/PS3HEN/blob/6c529455261db97a0f9028fe96db522a97fd5ccc/payload/main.c),
`BadWDSD_TryInstallHvcall` and `PS3MAPI_OPCODE_LV1_PEEK/POKE` cases.

**INFERRED:** the current low-line implementation is CFW-only for normal
users. A normal HEN port needs a different permitted video-control path or
additional LV1 capability; rebuilding the same PKG does not solve it.
**UNKNOWN:** compatibility on a separately hardware-exploited HEN system.
No such hardware/firmware modification is implemented or tested here.
The upstream [BadWDSD project](https://github.com/aomsin2526/BadWDSD)
describes it as a hardware modchip, distinct from a normal HEN installation.

**PROVEN (API/source distinction):** ordinary 480i/576i configuration uses
the public video API; it does not inherently require an LV1 timing patch.
**UNKNOWN:** HEN support for this app's combined restore/saved-default actions:
the current menu still requires the CFW timing-record preflight, and saving
defaults also requires webMAN's named-key support. These stock modes should
not be advertised as tested HEN features.
