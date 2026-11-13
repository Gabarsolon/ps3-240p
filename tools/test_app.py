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
             "-Wno-misleading-indentation", "-D_CRT_SECURE_NO_WARNINGS", "-Iapp/source", "-Itests"]
    for name, sources in [
        ("patch", ["tests/test_patch.c", "app/source/lv1_patch.c", "app/source/switch_flow.c"]),
        ("raster", ["tests/test_raster.c", "tests/raster_sim.c"]),
        ("ui", ["tests/test_ui.c", "app/source/ui_layout.c"]),
        ("startup", ["tests/test_startup.c", "app/source/lv1_patch.c", "app/source/switch_flow.c", "app/source/ui_layout.c"]),
    ]:
        exe = out / (name + (".exe" if os.name == "nt" else ""))
        test_flags = flags + (["-DTIMING_OFFLINE_TEST=1"] if name == "patch" else [])
        subprocess.run(test_flags + sources + ["-o", str(exe)], cwd=ROOT, check=True)
        if name == "startup":
            for scenario in range(17):
                subprocess.run([str(exe), str(scenario)], cwd=ROOT, check=True)
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
        subprocess.run(flags + ["-shared", "app/source/lv1_patch.c", "tests/audit_plan.c", "-o", str(dll)], cwd=ROOT, check=True)
        audit = ctypes.CDLL(str(dll))
        audit.audit_count.restype = ctypes.c_int
        audit.audit_word.argtypes = [ctypes.c_int, ctypes.POINTER(ctypes.c_uint32), ctypes.POINTER(ctypes.c_uint64)]
        with args.firmware.open("rb") as stream:
            elf = ELFFile(stream)
            segments = [(s["p_vaddr"], s.data()) for s in elf.iter_segments() if s["p_type"] == "PT_LOAD"]
        count = audit.audit_count()
        for i in range(count):
            va, word = ctypes.c_uint32(), ctypes.c_uint64()
            assert audit.audit_word(i, ctypes.byref(va), ctypes.byref(word)) == 0
            data = next(data[va.value-base:va.value-base+8] for base, data in segments if base <= va.value < base+len(data))
            assert int.from_bytes(data, "big") == word.value, f"Stock target {i} differs from private firmware"
        print(f"PASS: all {count} stock timing/code words match the private 4.93 ELF")

if __name__ == "__main__":
    main()
