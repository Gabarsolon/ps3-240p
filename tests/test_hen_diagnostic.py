"""Offline behavioral checks for the actual diagnostic core; no console access."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "experiments/hen_diagnostic"
OUT = ROOT / "build/hen-diagnostic/tests"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host-only", action="store_true", help="Run host checks without requiring a built package")
    parser.add_argument("--public-only", action="store_true")
    args = parser.parse_args()
    out = ROOT / ("build/hen-diagnostic-public/tests" if args.public_only else "build/hen-diagnostic/tests")
    compiler = shutil.which("clang") or shutil.which("gcc")
    if not compiler and Path("C:/Program Files/LLVM/bin/clang.exe").exists():
        compiler = "C:/Program Files/LLVM/bin/clang.exe"
    if not compiler:
        raise RuntimeError("A host C compiler is required")
    out.mkdir(parents=True, exist_ok=True)
    exe = out / ("diagnostic-host.exe" if os.name == "nt" else "diagnostic-host")
    subprocess.run([compiler, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-Wno-misleading-indentation",
                    "-D_CRT_SECURE_NO_WARNINGS", *(["-DHD_PUBLIC_ONLY=1"] if args.public_only else []),
                    str(SOURCE / "diagnostic.c"), str(SOURCE / "host_test.c"), "-o", str(exe)], check=True)
    reports = []
    for scenario in range(1 if args.public_only else 13):
        result = subprocess.run([str(exe), str(scenario)], check=True, capture_output=True, text=True)
        r = json.loads(result.stdout)
        assert r["lv1"] == {"capability": "UNTESTED", "operations_requested": 0}
        assert r["title_id"] == ("CRTHEN002" if args.public_only else "CRTHEN001")
        assert len(r["slots"]) == 6
        assert len(r["public_video"]["device"]["modes"]) <= 32
        assert "/dev_hdd0/home/" not in result.stdout and "other_process_not_logged" not in result.stdout
        assert "ksv" not in result.stdout.lower() and "\"path\"" not in result.stdout
        if args.public_only:
            assert r["mode"] == "public-only" and r["hen"]["status"] == "UNTESTED"
            assert r["hen"]["identity"] is None and r["ps3mapi"]["core"] is None
            assert all(slot["rc"] is None for slot in r["slots"])
            assert r["public_video"]["state_rc"] == 0
            reports.append(r)
            continue
        if scenario == 1:
            assert not r["hen"]["matched"] and r["ps3mapi"]["core"] is None
        if scenario == 2:
            assert not r["ps3mapi"]["identity_accepted"] and r["vsh_read"]["rc"] is None
        if scenario in (4, 5, 6, 12):
            assert r["vsh_read"]["rc"] is None
            assert r["vsh_read"]["status"] == "UNTESTED"
        if scenario == 5:
            assert r["vsh_read"]["matching_processes"] == 2
        if scenario in (7, 8):
            assert not r["vsh_read"]["elf64_big_endian"]
        if scenario == 9:
            assert r["vsh_read"]["matching_processes"] == 1
        reports.append(r)
    if not args.public_only:
        assert reports[0]["slots"][0]["name"] == 'own"plugin\n'
        assert reports[0]["slots"][0]["filename"] == "opaque.sprx"
        assert reports[0]["vsh_read"]["elf64_big_endian"]
        assert len(reports[10]["public_video"]["device"]["modes"]) == 32
        assert reports[11]["vsh_read"]["status"] == "HEADER_MATCH"
    (out / "host-results.json").write_text(json.dumps({"scenarios": len(reports), "passed": True, "public_only": args.public_only}, indent=2) + "\n")
    print("PASS: public-only collector invokes zero custom transports, even when a transport is supplied" if args.public_only else "PASS: 13 diagnostic scenarios; denied transports, exact VSH aliases, missing/ambiguous VSH, one fixed header read, JSON escaping/redaction and 32-mode limit")
    if not args.host_only:
        subprocess.run([os.sys.executable, "-I", str(ROOT / "tests/test_hen_diagnostic_artifact.py"), *(["--public-only"] if args.public_only else [])], check=True)


if __name__ == "__main__":
    main()
