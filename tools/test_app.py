"""Offline native tests; never contacts a console. Optional private ELF audit."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--firmware", type=Path, help="Optional local 4.93 sysmgr_ss.elf")
    args = parser.parse_args()
    out = ROOT / "build" / "tests"
    out.mkdir(parents=True, exist_ok=True)
    compiler = shutil.which("clang") or shutil.which("gcc")
    if not compiler and Path("C:/Program Files/LLVM/bin/clang.exe").exists():
        compiler = "C:/Program Files/LLVM/bin/clang.exe"
    if not compiler:
        sys.exit("A host C compiler (clang or gcc) is required")
    flags = [compiler, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
             "-Wno-misleading-indentation", "-D_CRT_SECURE_NO_WARNINGS", "-Iapp/source", "-Itests", "-DSYSMGR_SYNTHETIC_TEST=1"]
    for name, sources in [
        ("sysmgr", ["tests/test_sysmgr.c", "app/source/lv1_patch.c", "app/source/switch_flow.c"]),
        ("patch", ["tests/test_patch.c", "app/source/lv1_patch.c", "app/source/switch_flow.c"]),
        ("hd540-patch", ["tests/test_hd540_patch.c", "app/source/lv1_patch.c", "app/source/switch_flow.c", "app/source/hd540_patch.c"]),
        ("hd540-hdmi", ["tests/test_hd540_hdmi.c", "app/source/lv1_patch.c", "app/source/switch_flow.c", "app/source/hd540_patch.c", "app/source/hd540_hdmi.c"]),
        ("hd540-scale", ["tests/test_hd540_scale.c", "app/source/lv1_patch.c", "app/source/switch_flow.c", "app/source/hd540_patch.c", "app/source/hd540_scale.c"]),
        ("hd540-cache", ["tests/test_hd540_cache.c", "app/source/lv1_patch.c", "app/source/switch_flow.c", "app/source/hd540_patch.c", "app/source/hd540_cache.c"]),
        ("hd540-service", ["tests/test_hd540_service.c", "plugin/hd540_service.c"]),
        ("vsh-hd540", ["tests/test_vsh_hd540.c", "plugin/vsh_hd540.c", "app/source/lv1_patch.c", "app/source/hd540_patch.c", "app/source/hd540_hdmi.c", "app/source/hd540_scale.c", "app/source/hd540_cache.c", "app/source/switch_flow.c"]),
        ("raster", ["tests/test_raster.c", "tests/raster_sim.c"]),
        ("sd", ["tests/test_sd.c", "app/source/sd_default.c"]),
        ("gamma", ["tests/test_gamma.c", "app/source/gamma.c", "app/source/ui_layout.c"]),
        ("gamma-service", ["tests/test_gamma_service.c", "plugin/gamma_service.c"]),
        ("gamma-ipc", ["tests/test_gamma_ipc.c", "app/source/gamma_ipc.c", "plugin/gamma_service.c"]),
        ("plugin-lifecycle", ["tests/test_plugin_lifecycle.c", "app/source/plugin_lifecycle.c"]),
        ("vsh-video", ["tests/test_vsh_video.c", "plugin/vsh_video.c"]),
        ("vsh-gamma", ["tests/test_vsh_gamma.c", "plugin/vsh_gamma.c"]),
        ("ui", ["tests/test_ui.c", "app/source/ui_layout.c"]),
        ("startup", ["tests/test_startup.c", "app/source/lv1_patch.c", "app/source/switch_flow.c", "app/source/ui_layout.c", "app/source/sd_default.c", "app/source/plugin_lifecycle.c", "app/source/gamma.c", "app/source/gamma_ipc.c"]),
    ]:
        if "app/source/lv1_patch.c" in sources or name == "vsh-gamma":
            sources = sources + ["app/source/sysmgr.c"]
        exe = out / (name + (".exe" if os.name == "nt" else ""))
        test_flags = flags + (["-DTIMING_OFFLINE_TEST=1"] if name in ("sysmgr", "patch", "hd540-patch", "hd540-hdmi", "hd540-scale", "hd540-cache", "vsh-hd540") else [])
        if name == "vsh-hd540":
            test_flags += ["-DVSH_HD540_HOST_TEST=1"]
        if name == "vsh-video":
            test_flags += ["-DVSH_VIDEO_HOST_TEST=1"]
        if name == "vsh-gamma":
            test_flags += ["-DVSH_GAMMA_HOST_TEST=1"]
        subprocess.run(test_flags + sources + ([] if os.name == "nt" else ["-lm"]) + ["-o", str(exe)], cwd=ROOT, check=True)
        if name == "startup":
            for scenario in range(21):
                subprocess.run([str(exe), str(scenario)], cwd=ROOT, check=True)
            hd_exe = out / ("startup-hd540" + (".exe" if os.name == "nt" else ""))
            subprocess.run(test_flags + ["-DCRT_ENABLE_HD540=1"] + sources + ([] if os.name == "nt" else ["-lm"]) + ["-o", str(hd_exe)], cwd=ROOT, check=True)
            for scenario in (21, 22, 23):
                subprocess.run([str(hd_exe), str(scenario)], cwd=ROOT, check=True)
            continue
        command = [str(exe)]
        if name == "ui":
            command.append(str(out / "pal-menu.ppm"))
            command.append(str(out / "pattern-480.ppm"))
            command.append(str(out / "pattern-240.ppm"))
        subprocess.run(command, cwd=ROOT, check=True)
    # The VSH plugin loader contract is checked on the compiled artifact: a
    # module_start that returns normally hangs the Cobra loader forever.
    plugin_elf = ROOT / "plugin" / "vsh_240p.elf"
    if plugin_elf.exists():
        subprocess.run([sys.executable, str(ROOT / "tests" / "test_plugin.py")], cwd=ROOT, check=True)
    else:
        print("SKIP: plugin/vsh_240p.elf not built; run tools/build_plugin.py for the loader-contract check")
    if args.firmware:
        # Compare generated plans against the private input without copying any
        # firmware bytes to tracked files or printing a firmware dump.
        from elftools.elf.elffile import ELFFile
        import ctypes
        dll = out / ("patch-audit.dll" if os.name == "nt" else "patch-audit.so")
        subprocess.run(flags + ["-shared", "app/source/sysmgr.c", "app/source/lv1_patch.c", "app/source/hd540_patch.c", "app/source/hd540_hdmi.c", "app/source/hd540_scale.c", "app/source/hd540_cache.c", "tests/audit_plan.c", "-o", str(dll)], cwd=ROOT, check=True)
        audit = ctypes.CDLL(str(dll))
        audit.audit_count.restype = ctypes.c_int
        audit.audit_word.argtypes = [ctypes.c_int, ctypes.POINTER(ctypes.c_uint32), ctypes.POINTER(ctypes.c_uint64)]
        with args.firmware.open("rb") as stream:
            elf = ELFFile(stream)
            segments = [(s["p_vaddr"], s.data()) for s in elf.iter_segments() if s["p_type"] == "PT_LOAD"]
            regions = [(s['p_vaddr'], s['p_memsz'], s['p_flags']) for s in elf.iter_segments() if s['p_type']=='PT_LOAD']
        identity_dll = out / ("sysmgr-audit.dll" if os.name == "nt" else "sysmgr-audit.so")
        production_flags = [f for f in flags if f != "-DSYSMGR_SYNTHETIC_TEST=1"]
        subprocess.run(production_flags + ["-shared", "app/source/sysmgr.c", "tests/audit_sysmgr.c", "-o", str(identity_dll)], cwd=ROOT, check=True)
        identity = ctypes.CDLL(str(identity_dll))
        identity.audit_sysmgr_identity.argtypes = [ctypes.POINTER(ctypes.c_uint64)]
        va = 0x80005e78
        entry = next(data[va-base:va-base+32] for base,data in segments if base<=va and va+32<=base+len(data))
        words = (ctypes.c_uint64*4)(*(int.from_bytes(entry[i:i+8], "big") for i in range(0,32,8)))
        assert identity.audit_sysmgr_identity(words) == 1
        for byte in range(32):
            changed = (ctypes.c_uint64*4)(*words)
            changed[byte//8] ^= 1 << ((7-byte%8)*8)
            assert identity.audit_sysmgr_identity(changed) == 0
        print("PASS: production sysmgr entry fingerprint matches private 4.93 and rejects a change in every byte")
        count = audit.audit_count()
        for i in range(count):
            va, word = ctypes.c_uint32(), ctypes.c_uint64()
            assert audit.audit_word(i, ctypes.byref(va), ctypes.byref(word)) == 0
            data = next(data[va.value-base:va.value-base+8] for base, data in segments if base <= va.value < base+len(data))
            assert int.from_bytes(data, "big") == word.value, f"Stock target {i} differs from private firmware"
        print(f"PASS: all {count} stock timing/code words match the private 4.93 ELF")
        audit.audit_hd540_record.argtypes = [ctypes.c_uint, ctypes.POINTER(ctypes.c_uint64)]
        for index, profile in enumerate((7, 8)):
            record = b""
            for va, length in ((0x80039148 + profile * 24, 24), (0x80038b28 + profile * 56, 56)):
                record += next(data[va-base:va-base+length] for base, data in segments if base <= va and va+length <= base+len(data))
            words = (ctypes.c_uint64 * 10)(*(int.from_bytes(record[i:i+8], "big") for i in range(0, 80, 8)))
            assert audit.audit_hd540_record(index, words) == 1, f"HD profile {profile} fingerprint differs from private firmware"
        print("PASS: both complete 1080i metadata/raster fingerprints match the private 4.93 ELF")
        audit.audit_hd540_hdmi.argtypes = [ctypes.POINTER(ctypes.c_uint64)]
        va = 0x800399c0
        record = next(data[va-base:va-base+16] for base, data in segments if base <= va and va+16 <= base+len(data))
        words = (ctypes.c_uint64 * 2)(*(int.from_bytes(record[i:i+8], "big") for i in (0, 8)))
        assert audit.audit_hd540_hdmi(words) == 1, "HDMI template fingerprint differs from private firmware"
        words[0] ^= 1
        assert audit.audit_hd540_hdmi(words) == 0, "HDMI fingerprint accepted changed neighboring data"
        print("PASS: HDMI template fingerprint matches private 4.93 and rejects a changed neighboring byte")
        audit.audit_hd540_avi_code.argtypes = [ctypes.POINTER(ctypes.c_uint64)]
        va = 0x80010000
        record = next(data[va-base:va-base+16] for base, data in segments if base <= va and va+16 <= base+len(data))
        words = (ctypes.c_uint64 * 2)(*(int.from_bytes(record[i:i+8], "big") for i in (0, 8)))
        assert audit.audit_hd540_avi_code(words) == 1, "AVI builder fingerprint differs from private firmware"
        words[1] ^= 1
        assert audit.audit_hd540_avi_code(words) == 0, "AVI builder fingerprint accepted altered neighboring code"
        print("PASS: AVI builder fingerprint matches private 4.93 and rejects changed neighboring code")
        audit.audit_hd540_scale_code.argtypes = [ctypes.POINTER(ctypes.c_uint64)]
        addresses = (0x80002f48, 0x80002f50, 0x80003058, 0x80003060, 0x80003100, 0x80003108, 0x800036c8, 0x800036d0)
        record = b"".join(next(data[va-base:va-base+8] for base, data in segments if base <= va and va+8 <= base+len(data)) for va in addresses)
        words = (ctypes.c_uint64 * 8)(*(int.from_bytes(record[i:i+8], "big") for i in range(0,64,8)))
        assert audit.audit_hd540_scale_code(words) == 1, "Scaler code fingerprint differs from private firmware"
        words[1] ^= 1
        assert audit.audit_hd540_scale_code(words) == 0, "Scaler identity accepted changed neighboring code"
        print("PASS: source-window/scaler code identity matches private 4.93 and rejects changed neighboring code")
        audit.audit_hd540_callback.argtypes = [ctypes.POINTER(ctypes.c_uint64)]
        va = 0x8000d8e8
        record = next(data[va-base:va-base+16] for base,data in segments if base<=va and va+16<=base+len(data))
        words = (ctypes.c_uint64*2)(*(int.from_bytes(record[i:i+8],'big') for i in (0,8)))
        assert audit.audit_hd540_callback(words)==1
        words[0]^=1
        assert audit.audit_hd540_callback(words)==0
        for at, size, permission in ((0x8003f800,512,7),(0xc000de00,48,6)):
            assert any(base+length<=at and at+size<=(base+length+0xfff)&~0xfff and flags==permission
                       for base,length,flags in regions), 'Wrapper arena overlaps declared module memory'
        print('PASS: callback identity and unused mapped code/RW padding bounds match private 4.93')

if __name__ == "__main__":
    main()
