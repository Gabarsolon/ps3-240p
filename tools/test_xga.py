"""Offline XGA guard tests. Never contacts a console or capture device."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--emulate", action="store_true",
                        help="Also run own-payload PPC64 emulation in one Ghidra JVM")
    args = parser.parse_args()
    if args.emulate:
        from build_xga_cache import build
        build()
    compiler = shutil.which("clang") or shutil.which("gcc")
    if not compiler and Path("C:/Program Files/LLVM/bin/clang.exe").exists():
        compiler = "C:/Program Files/LLVM/bin/clang.exe"
    if not compiler:
        parser.error("A host C compiler is required")
    output = ROOT / "build/tests"
    output.mkdir(parents=True, exist_ok=True)
    flags = [compiler, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
             "-Wno-misleading-indentation", "-D_CRT_SECURE_NO_WARNINGS",
             "-Iapp/source", "-Itests"]
    cache = ["tests/test_hd540_cache.c", "app/source/lv1_patch.c",
             "app/source/switch_flow.c", "app/source/hd540_patch.c",
             "app/source/hd540_cache.c", "app/source/sysmgr.c"]
    for name, definitions, sources in [
        ("cache-default", ["TIMING_OFFLINE_TEST", "SYSMGR_SYNTHETIC_TEST"], cache),
        ("cache-xga", ["TIMING_OFFLINE_TEST", "SYSMGR_SYNTHETIC_TEST", "CRT_XGA_PROBE"], cache),
        ("xga-source", ["XGA_SOURCE_HOST_TEST"],
         ["tests/test_xga_source.c", "experiments/display_probe/xga_source.c"]),
        ("xga-patch", ["XGA_PATCH_HOST_TEST"],
         ["tests/test_xga_patch.c", "experiments/display_probe/xga_patch.c"]),
        ("xga-service", ["CRT_XGA_PROBE"],
         ["tests/test_display_probe.c", "experiments/display_probe/service.c"]),
        ("display-native", ["DP_NATIVE_HOST_TEST"],
         ["tests/test_display_probe_native.c", "experiments/display_probe/native.c"]),
    ]:
        executable = output / (name + (".exe" if os.name == "nt" else ""))
        subprocess.run(flags + ["-D" + d + "=1" for d in definitions] + sources +
                       ["-o", str(executable)], cwd=ROOT, check=True)
        subprocess.run([str(executable)], cwd=ROOT, check=True)
    for test in ("tests/test_xga_cache.py", "tests/test_display_probe_auto.py"):
        subprocess.run([sys.executable, "-B", test], cwd=ROOT, check=True)
    if args.emulate:
        subprocess.run([sys.executable, "-B", "tests/test_xga_cache_ghidra.py"],
                       cwd=ROOT, check=True)


if __name__ == "__main__":
    main()
