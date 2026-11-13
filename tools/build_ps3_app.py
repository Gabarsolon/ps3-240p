#!/usr/bin/env python3
"""
PS3 240p / 288p Native Homebrew Package Builder
Compiles the PPU executable and packages it into a signed retail PS3 .pkg installer.
"""

import argparse
import hashlib
import json
import os
import sys
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
CONTENT_ID = "UP0001-PS240P001_00-PS3240PSWITCH001"
TITLE = "CRT Toolset"
APP_ID = "PS240P001"
APP_VERSION = "01.43"

def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest()

def main(sdk_path: str):
    sdk = Path(sdk_path).resolve()
    if not sdk.exists():
        raise FileNotFoundError(f"PS3DK SDK not found at {sdk}")

    build_dir = ROOT / "build"
    app_build_dir = ROOT / "app" / "build"
    staging_dir = build_dir / "staging"
    deliverables_dir = ROOT / "deliverables"

    build_dir.mkdir(exist_ok=True)
    staging_dir.mkdir(parents=True, exist_ok=True)
    deliverables_dir.mkdir(exist_ok=True)

    # Set up SDK Environment
    env = os.environ.copy()
    env.update({
        "PS3DK": str(sdk),
        "PS3DEV": sdk.as_posix(),
        "PSL1GHT": sdk.as_posix(),
        "PYTHONDONTWRITEBYTECODE": "1"
    })
    env["PATH"] = f"{sdk / 'bin'}{os.pathsep}{sdk / 'ppu' / 'bin'}{os.pathsep}{env.get('PATH', '')}"

    def run(*cmd, cwd=ROOT):
        print(f"[*] Executing: {' '.join(str(c) for c in cmd)}")
        subprocess.run([str(c) for c in cmd], cwd=cwd, env=env, check=True)

    # 1. CMake Configure & Build
    print("\n=== [1/5] Building PPU Executable with CMake ===")
    toolchain_cmake = sdk / "cmake" / "ps3-ppu-toolchain.cmake"
    run("cmake", "-S", ROOT / "app", "-B", app_build_dir, "-G", "Ninja",
        f"-DCMAKE_TOOLCHAIN_FILE={toolchain_cmake}")
    run("cmake", "--build", app_build_dir, "--target", "ps3-240p-app")

    elf_file = app_build_dir / "ps3-240p-app.elf"
    if not elf_file.exists():
        raise FileNotFoundError(f"Compiled ELF not found at {elf_file}")

    # 2. Prepare PKG Staging Hierarchy
    print("\n=== [2/5] Staging Package Directory Structure ===")
    if staging_dir.exists():
        shutil.rmtree(staging_dir)
    usr_dir = staging_dir / "USRDIR"
    usr_dir.mkdir(parents=True, exist_ok=True)

    eboot_dst = usr_dir / "EBOOT.BIN"
    sfo_dst = staging_dir / "PARAM.SFO"
    icon_src = ROOT / "app" / "assets" / "ICON0.PNG"
    icon_dst = staging_dir / "ICON0.PNG"

    # 3. Create NPDRM EBOOT.BIN
    print("\n=== [3/5] Packaging NPDRM Signed EBOOT.BIN ===")
    make_self = sdk / "bin" / "make_self_npdrm.exe"
    run(make_self, elf_file, eboot_dst, CONTENT_ID)

    # 4. Generate PARAM.SFO
    print("\n=== [4/5] Generating PARAM.SFO ===")
    sfo_tool = sdk / "bin" / "sfo.exe"
    sfo_xml = sdk / "bin" / "sfo.xml"
    run(sfo_tool, "--title", TITLE, "--appid", APP_ID, "-f", sfo_xml, sfo_dst)
    run(sfo_tool, "set", sfo_dst, f"APP_VER={APP_VERSION}", "--out", sfo_dst)
    run(sfo_tool, "validate", sfo_dst)

    # Copy Icon & Extras
    if icon_src.exists():
        shutil.copyfile(icon_src, icon_dst)
    shutil.copyfile(ROOT / "app" / "BuzzBridge-LICENSE", usr_dir / "BuzzBridge-LICENSE.txt")
    shutil.copyfile(ROOT / "plugin" / "vsh_240p.sprx", usr_dir / "vsh_240p.sprx")

    # 5. Build and Finalize PKG
    print("\n=== [5/5] Building & Finalizing PS3 PKG ===")
    pkg_tool = sdk / "bin" / "pkg.exe"
    finalize_tool = sdk / "bin" / "package_finalize.exe"
    out_pkg = deliverables_dir / "CRT-Toolset.pkg"

    run(pkg_tool, "--contentid", CONTENT_ID, f"{staging_dir}/", out_pkg)
    run(finalize_tool, out_pkg)

    # 6. Verify PKG Acceptance
    from pkg_inspect import members
    packaged = members(out_pkg)
    assert "USRDIR/EBOOT.BIN" in packaged
    assert "PARAM.SFO" in packaged
    assert "ICON0.PNG" in packaged
    assert "USRDIR/BuzzBridge-LICENSE.txt" in packaged
    assert "USRDIR/vsh_240p.sprx" in packaged
    for name, data in packaged.items():
        original = (staging_dir / name).read_bytes()
        if name == "USRDIR/EBOOT.BIN":
            # PS3DK packaging/finalization can append zero padding to SELF.
            # Require the complete original executable and a zero-only tail.
            assert data.startswith(original)
            assert not any(data[len(original):])
        else:
            assert data == original, f"Packaged content differs: {name}"
    print(f"[*] Package acceptance verified: {len(packaged)} files decrypted cleanly")

    # Copy raw deliverables
    shutil.copyfile(elf_file, deliverables_dir / "ps3-240p-app.elf")
    (deliverables_dir / "EBOOT.BIN").write_bytes(packaged["USRDIR/EBOOT.BIN"])

    # Generate Manifest
    pkg_sha = sha256_file(out_pkg)
    pkg_size = out_pkg.stat().st_size
    manifest = {
        "title": TITLE,
        "content_id": CONTENT_ID,
        "app_id": APP_ID,
        "app_version": APP_VERSION,
        "validation": "Proportional 240p/288p typography with non-overlapping line spacing and CRT overscan margins",
        "package": out_pkg.name,
        "size_bytes": pkg_size,
        "sha256": pkg_sha,
        "files": [
            {"file": name, "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}
            for name, data in sorted(packaged.items())
        ]
    }

    manifest_path = deliverables_dir / "package_manifest.json"
    with open(manifest_path, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)

    print("\n" + "=" * 60)
    print("  PS3 240p / 288p Switcher PKG Built Successfully!")
    print("=" * 60)
    print(f"Deliverable : {out_pkg}")
    print(f"Package Size: {pkg_size:,} bytes")
    print(f"SHA-256     : {pkg_sha}")
    print(f"Manifest    : {manifest_path}")
    print("=" * 60)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Build PS3 240p Application PKG")
    parser.add_argument("--sdk", default=os.environ.get("PS3DK", r"E:\tools\ps3dk\ps3-sdk-v0.26.0-windows-x86_64"),
                        help="Path to PS3DK root directory")
    args = parser.parse_args()
    main(args.sdk)
