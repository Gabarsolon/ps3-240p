"""Emulate our XGA wrapper, ABI, bounded request edit and exact restoration.

No Sony binary input. Requires one Ghidra JVM and the assembled own payload.
"""
from pathlib import Path
import os
import struct
import warnings
import sys

ROOT = Path(__file__).resolve().parents[1]
CUSTOM = "--custom" in sys.argv
BUILD = "hdmi-custom" if CUSTOM else "xga-probe"
CONTROL_OFFSET = 104 if CUSTOM else 96
WORK = ROOT / "local_only/xga-cache-emulator"
for part in ("projects", "tmp", "settings"):
    (WORK / part).mkdir(parents=True, exist_ok=True)
for key in ("TEMP", "TMP", "TMPDIR", "APPDATA", "LOCALAPPDATA"):
    os.environ[key] = str(WORK / ("tmp" if key in ("TEMP", "TMP", "TMPDIR") else "settings"))
os.environ["JAVA_HOME"] = "C:/Program Files/Microsoft/jdk-21.0.12.101-hotspot"
os.environ["GHIDRA_INSTALL_DIR"] = "C:/ghidra_12.1.3_PUBLIC"
warnings.filterwarnings("ignore", category=DeprecationWarning)
import pyghidra
from pyghidra.launcher import HeadlessPyGhidraLauncher
launch = HeadlessPyGhidraLauncher()
launch.java_home = os.environ["JAVA_HOME"]
launch.add_vmargs("-Xmx2500m", "-Duser.home=" + str(WORK / "settings"),
                  "-Dapplication.settingsdir=" + str(WORK / "settings"))
launch.start()
from ghidra.app.emulator import EmulatorHelper
from ghidra.util.task import ConsoleTaskMonitor
from java.math import BigInteger

CODE, NATIVE, TRACE = 0x8003f800, 0x8000d8ec, 0xc000de00
STACK, RETURN, PACKET = 0x10008000, 0x20000000, 0x90000000
FP, PCRTC = 0xa000a000, 0xa0006000
CONTROL = 0x5847413900001000
data = (ROOT / f"build/{BUILD}/cache/cache.bin").read_bytes()
cases = [({}, True, status) for status in (0, 6, 0x1234567800000000)]
cases += [(change, False, 0) for change in (
    {"control": 0}, {"control": CONTROL ^ 1}, {"head": 1}, {"vid": 11},
    {"width": 1024}, {"height": 768}, {"pitch": 5120}, {"format": 1},
    {"count": 0}, {"count": 2}, {"length": 12}, {"record_size": 16})]

with pyghidra.open_program(str(ROOT / f"build/{BUILD}/cache/cache.bin"),
                         project_location=str(WORK / "projects"), project_name="own_xga_wrapper",
                         analyze=False, language="PowerPC:BE:64:default") as flat:
    program = flat.getCurrentProgram()
    space = program.getAddressFactory().getDefaultAddressSpace()
    def address(n): return space.getAddress(format(n, "x"))
    for at, size in [(0x8003f000, 4096), (0x8000d000, 4096), (0xc000b000, 4096),
                     (0xc000d000, 4096), (0xa0001000, 4096), (FP, 4096),
                     (PCRTC, 4096), (0x10000000, 65536), (RETURN, 4096), (PACKET, 4096)]:
        if program.getMemory().getBlock(address(at)) is None:
            flat.createMemoryBlock("owned_" + hex(at), address(at), bytes(size), False)
    for change, normalized, status in cases:
        params = dict(control=CONTROL, head=0, vid=13, width=1280, height=720,
                      pitch=4096, format=0, count=1, length=2048, record_size=44)
        params.update(change)
        emu = EmulatorHelper(program)
        emu.enableMemoryWriteTracking(True)
        def wr(at, b): emu.writeMemory(address(at), b)
        def read(at, n): return bytes(emu.readMemory(address(at), n))
        def reg(name): return int(str(emu.readRegister(name)))
        def setreg(name, value): emu.writeRegister(name, BigInteger(str(value)))
        def pc(at): emu.getEmulator().setExecuteAddress(at)
        wr(CODE, data)
        wr(NATIVE, bytes.fromhex("4e800020"))
        wr(TRACE, bytes(CONTROL_OFFSET + 8))
        wr(TRACE + CONTROL_OFFSET, struct.pack(">Q", params["control"]))
        packet = bytearray(2048)
        struct.pack_into(">H", packet, 8, params["count"])
        struct.pack_into(">HH", packet, 16, 0x205, params["record_size"])
        for offset, key in ((24, "head"), (32, "vid"), (36, "width"), (40, "height"),
                            (44, "pitch"), (48, "format")):
            struct.pack_into(">I", packet, offset, params[key])
        wr(PACKET, bytes(packet))
        wr(0xc000b440, struct.pack(">Q", 0xc000d4c8))
        wr(0xc000d4c8, struct.pack(">Q", 0xa0001860))
        wr(0xa0001880, struct.pack(">Q", 0x9f98a000))
        wr(0xa0001878, struct.pack(">Q", 0x9fa06000))
        for offset, value in ((0x848, 0x11003101), (0x880, 0x21101101), (0x834, 0),
                              (0x838, 1023), (0x814, 0), (0x818, 767),
                              (0x898, 0x10000000), (0x89c, 0x10000000)):
            wr(FP + offset, struct.pack(">I", value))
        wr(PCRTC + 0x200, struct.pack(">I", 1023))
        if CUSTOM:
            wr(PCRTC + 0x820, struct.pack(">I", 8192))
            wr(PCRTC + 0x210, struct.pack(">I", 1080))
        args = {i: 0x100000000 + i for i in range(3, 11)}
        args[4], args[6] = PACKET, params["length"]
        saved = {i: 0x1234000000000000 + i for i in range(14, 32)}
        setreg("r1", STACK); setreg("r2", 0xc0012f40); setreg("LR", RETURN)
        for i, value in (args | saved).items(): setreg("r" + str(i), value)
        pc(CODE)
        calls, cache = 0, []
        for step in range(1500):
            at = emu.getExecutionAddress().getOffset()
            if at == RETURN: break
            if at == NATIVE:
                assert reg("r1") == STACK - 192 and reg("r2") == 0xc0012f40
                assert all(reg("r" + str(i)) == v for i, v in args.items()), change
                actual = struct.unpack(">II", read(PACKET + 36, 8))
                assert actual == ((1024, 768) if normalized else (params["width"], params["height"])), (change, actual)
                expected_packet = bytearray(packet)
                if normalized: struct.pack_into(">II", expected_packet, 36, 1024, 768)
                assert read(PACKET, 2048) == bytes(expected_packet)
                link = reg("LR")
                wr(STACK - 192 + 16, struct.pack(">Q", 0xdeadbeef))
                for i in (0, 4, 5, 6, 7, 8, 9, 10, 11, 12): setreg("r" + str(i), 0xbad00000 + i)
                setreg("r3", status); pc(link); calls += 1
                continue
            word = int.from_bytes(read(at, 4), "big")
            opcode, xo = word >> 26, (word >> 1) & 1023
            if opcode == 31 and xo in (54, 982):
                cache.append(("dcbst" if xo == 54 else "icbi", reg("r11")))
                pc(at + 4); continue
            if word in (0x7c0004ac, 0x4c00012c): pc(at + 4); continue
            assert emu.step(ConsoleTaskMonitor()), (hex(at), emu.getLastError())
        else: raise AssertionError("Wrapper did not return")
        assert calls == 1 and reg("r1") == STACK and reg("r2") == 0xc0012f40 and reg("r3") == status
        assert all(reg("r" + str(i)) == v for i, v in saved.items())
        assert read(PACKET, 2048) == bytes(packet), "Caller packet was not restored exactly"
        trace = struct.unpack(">" + str(CONTROL_OFFSET // 4) + "I", read(TRACE, CONTROL_OFFSET))
        assert trace[:2] == (1, 0) and trace[6] == status & 0xffffffff
        assert trace[20:22] == ((1, 1) if normalized else (0, 0)), (change, trace[20:22])
        assert trace[22:] == ((8192, 1080, 0, 0) if CUSTOM else (0, 0))
        lines = (0x80002d00, 0x80002e00, 0x80002f80, 0x80003000, 0x80003080, 0x80003100, 0x80025f80)
        if CUSTOM: lines += (0x80002f00, 0x80003680)
        assert cache == [("dcbst", a) for a in lines] + [("icbi", a) for a in reversed(lines)]
        writes = emu.getTrackedMemoryWriteSet()
        assert not writes.intersects(address(FP), address(FP + 4095))
        assert not writes.intersects(address(PCRTC), address(PCRTC + 4095))
        assert read(TRACE + CONTROL_OFFSET, 8) == struct.pack(">Q", params["control"])
        emu.dispose()
    print("PASS: XGA PPC64 ABI, exact armed tuple, all refusal guards, complete caller-packet restoration, cache coverage and no MMIO stores", flush=True)
