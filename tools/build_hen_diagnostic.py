"""Build an isolated query-only HEN diagnostic. Offline; never contacts a PS3."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "experiments/hen_diagnostic"
OUT = ROOT / "build/hen-diagnostic"
TITLE_ID = "CRTHEN001"
TITLE = "CRT HEN Diagnostic"
CONTENT_ID = "UP0001-CRTHEN001_00-CRTHENDIAG000001"
VERSION = "0.1.0"
DEFAULT_SDK = r"E:\Tools\ps3dk\ps3-sdk-v0.26.0-windows-x86_64"


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    global OUT, TITLE_ID, TITLE, CONTENT_ID
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", type=Path, default=os.environ.get("PS3DK", DEFAULT_SDK))
    parser.add_argument("--public-only", action="store_true", help="Build distinct CRTHEN002 CFW control without custom syscall-8 queries")
    args = parser.parse_args()
    if args.public_only:
        OUT = ROOT / "build/hen-diagnostic-public"
        TITLE_ID = "CRTHEN002"
        TITLE = "CRT Public Video Diagnostic"
        CONTENT_ID = "UP0001-CRTHEN002_00-CRTPUBDIAG000001"
    report_path = "/dev_hdd0/tmp/crt_hen_diagnostic_public.json" if args.public_only else "/dev_hdd0/tmp/crt_hen_diagnostic.json"
    variant_args = ["--public-only"] if args.public_only else []
    sdk = args.sdk.resolve()
    env = os.environ.copy()
    env.update(PS3DK=str(sdk), PS3DEV=sdk.as_posix(), PSL1GHT=sdk.as_posix(), PYTHONDONTWRITEBYTECODE="1")
    env["PATH"] = os.pathsep.join((str(sdk / "bin"), str(sdk / "ppu/bin"), env.get("PATH", "")))
    OUT.mkdir(parents=True, exist_ok=True)
    commands = []

    def run(*command):
        command = [str(item) for item in command]
        commands.append(command)
        subprocess.run(command, cwd=ROOT, env=env, check=True)

    # Prove query refusal/limits before compiling any PS3 artifact.
    run(sys.executable, "-I", ROOT / "tests/test_hen_diagnostic.py", "--host-only", *variant_args)
    native = OUT / "native"
    run("cmake", "-S", SOURCE, "-B", native, "-G", "Ninja",
        f"-DCMAKE_TOOLCHAIN_FILE={sdk / 'cmake/ps3-ppu-toolchain.cmake'}",
        f"-DHD_PUBLIC_ONLY={'ON' if args.public_only else 'OFF'}")
    run("cmake", "--build", native, "--target", "crt-hen-diagnostic")

    # Delete only our checked, fixed staging directory, never a user input path.
    staging = OUT / "staging"
    assert staging.resolve().parent == OUT.resolve() and staging.name == "staging"
    if staging.exists():
        shutil.rmtree(staging)
    usr = staging / "USRDIR"
    usr.mkdir(parents=True)
    deliver = OUT / "deliverables"
    deliver.mkdir(exist_ok=True)
    elf = native / "crt-hen-diagnostic.elf"
    run(sdk / "bin/make_self_npdrm.exe", elf, usr / "EBOOT.BIN", CONTENT_ID)
    run(sdk / "bin/sfo.exe", "--title", TITLE, "--appid", TITLE_ID, "-f", sdk / "bin/sfo.xml", staging / "PARAM.SFO")
    run(sdk / "bin/sfo.exe", "validate", staging / "PARAM.SFO")
    shutil.copyfile(sdk / "bin/ICON0.PNG", staging / "ICON0.PNG")
    (usr / "DIAGNOSTIC.txt").write_text(
        f"{TITLE} {VERSION}\n" +
        ("Public video control; HEN identification compiled out. Run once from XMB.\n" if args.public_only else "For an already enabled, matching PS3HEN environment. Run once from XMB.\n") +
        "No on-screen UI: collects getter replies and exits normally.\n"
        f"Report: {report_path}\n"
        "LV1 access is UNTESTED. No display configuration or plugin loading.\n"
        "Runtime/HEN compatibility remains UNKNOWN until tested on real hardware.\n",
        encoding="utf-8", newline="\n")
    pkg = deliver / (f"CRT-Public-Video-Diagnostic-{VERSION}.pkg" if args.public_only else f"CRT-HEN-Diagnostic-{VERSION}.pkg")
    run(sdk / "bin/pkg.exe", "--contentid", CONTENT_ID, f"{staging}/", pkg)
    run(sdk / "bin/package_finalize.exe", pkg)
    run(sys.executable, "-I", ROOT / "tests/test_hen_diagnostic_artifact.py", "--sdk", sdk, *variant_args)

    sys.path.insert(0, str(ROOT / "tools"))
    from pkg_inspect import members
    files = members(pkg)
    manifest = {
        "title": TITLE, "title_id": TITLE_ID, "content_id": CONTENT_ID, "version": VERSION,
        "package": pkg.name, "sha256": digest(pkg), "bytes": pkg.stat().st_size,
        "install_directory": f"/dev_hdd0/game/{TITLE_ID}",
        "report": report_path, "public_only": args.public_only,
        "hen_probe": "UNTESTED (compiled out)" if args.public_only else "Enabled only for matching audited HEN dispatcher",
        "console_contacted": False, "runtime_hen_validation": "UNKNOWN",
        "lv1_capability": "UNTESTED", "validation": "Host protocol/JSON tests; compiled transport/import checks; decrypted package acceptance",
        "files": [{"name": name, "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()} for name, data in sorted(files.items())],
        "sources": [{"name": path.relative_to(ROOT).as_posix(), "sha256": digest(path)} for path in sorted(SOURCE.glob("*")) if path.is_file()],
        "commands": commands,
    }
    (deliver / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"package": str(pkg), "sha256": manifest["sha256"], "runtime_hen_validation": "UNKNOWN"}, indent=2))


if __name__ == "__main__":
    main()
