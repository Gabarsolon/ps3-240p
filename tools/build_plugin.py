"""Reproducible build of the VSH resident 240p enforcer plugin.

Offline only: compiles, packs and signs plugin/vsh_240p.sprx. It never
contacts a console.

The linker script and the PRX packer/signer are reused read-only from the
sibling project (buzz-pad-bridge/ps3/plugin), which owns the proven
implementation of the native PRX container and the VSH self-signing.
"""
import argparse
import hashlib
import json
import os
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
PLUGIN = ROOT / "plugin"
SIBLING = pathlib.Path(r"C:\Users\remus\buzz-pad-bridge\ps3\plugin")
SDK = pathlib.Path(os.environ.get("PS3DK", r"E:\Tools\ps3dk\ps3-sdk-v0.26.0-windows-x86_64"))

SOURCE = PLUGIN / "vsh_240p.c"
ELF = PLUGIN / "vsh_240p.elf"
PRX = PLUGIN / "vsh_240p.prx"
SPRX = PLUGIN / "vsh_240p.sprx"

# Module-info version byte. The packer's generic fallback is 4, but the only
# VSH plugin proven to load on this console/toolchain (the sibling helper
# ps3buzz_vsh) uses 3. Match the proven value instead of the untested one.
MODULE_INFO_MINOR = 3

# Same flags as the verified build: strict warnings, no unwind tables, and a
# 16-byte page size so the native PRX packer's segment layout stays valid.
CFLAGS = [
    "-O2", "-Wall", "-Wextra", "-Werror", "-Wno-misleading-indentation",
    "-fno-asynchronous-unwind-tables", "-fno-unwind-tables",
]


def run(*args, env=None):
    subprocess.run([str(a) for a in args], cwd=ROOT, check=True, env=env)


def sha256(path):
    return hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest()


def main():
    global ELF, PRX, SPRX
    parser = argparse.ArgumentParser()
    variants = parser.add_mutually_exclusive_group()
    variants.add_argument("--experimental-hd540", action="store_true",
                        help="Build experimental helper under build/hd540; preserve the release helper")
    variants.add_argument("--pid-compatibility", action="store_true",
                          help="Build the SD PID compatibility helper under build/pid-compat")
    parser.add_argument("--name", default="vsh_240p")
    parser.add_argument("--keep-elf", action="store_true",
                        help="Keep the intermediate ELF (default: keep it, it is the packer input)")
    args = parser.parse_args()
    if args.experimental_hd540:
        from build_hd540_cache import build
        build()
    if args.experimental_hd540 or args.pid_compatibility:
        directory = ROOT / "build" / ("hd540" if args.experimental_hd540 else "pid-compat") / "plugin"
        directory.mkdir(parents=True, exist_ok=True)
        ELF, PRX, SPRX = (directory / ("vsh_240p" + suffix) for suffix in (".elf", ".prx", ".sprx"))

    gcc = SDK / "ppu" / "bin" / "ppu-gcc.exe"
    linker = SIBLING / "game" / "native-prx.ld"
    for required in (gcc, linker, SDK / "ppu" / "lib" / "lv2-prx.specs",
                     SDK / "ppu" / "lib" / "lv2-prx-crt.o", SOURCE):
        if not required.exists():
            sys.exit(f"missing build input: {required}")

    compile_cmd = [
        gcc, *CFLAGS,
        f"-specs={SDK / 'ppu' / 'lib' / 'lv2-prx.specs'}",
        "-nostartfiles",
        "-Wl,-q", "-Wl,-z,max-page-size=16",
        f"-Wl,-T,{linker.as_posix()}",
        "-Wl,--gc-sections",
        SOURCE,
        PLUGIN / "vsh_video.c",
        PLUGIN / "vsh_gamma.c",
        ROOT / "app/source/sysmgr.c",
        PLUGIN / "gamma_service.c",
        SDK / "ppu" / "lib" / "lv2-prx-crt.o",
        "-lio",
        "-o", ELF,
    ]
    if args.experimental_hd540:
        compile_cmd[1:1] = ["-DCRT_ENABLE_HD540=1"]
        compile_cmd += [PLUGIN / "vsh_hd540.c", PLUGIN / "hd540_service.c",
                        ROOT / "app/source/hd540_patch.c", ROOT / "app/source/hd540_hdmi.c", ROOT / "app/source/hd540_scale.c", ROOT / "app/source/hd540_cache.c",
                        ROOT / "app/source/lv1_patch.c"]
    print("compiling:", " ".join(str(c) for c in compile_cmd))
    # The SDK's specs file resolves its own include paths from PS3DK.
    env = dict(os.environ)
    env["PS3DK"] = str(SDK)
    run(*compile_cmd, env=env)

    run(sys.executable, "-B", SIBLING / "tools" / "native_prx.py",
        ELF, PRX, "--name", args.name,
        "--start", "module_start", "--stop", "module_stop")

    # Align the module-info version byte with the proven VSH helper.
    layout = json.loads(pathlib.Path(str(PRX) + ".layout.json").read_text())
    info = layout["module_info_file_offset"]
    prx = bytearray(PRX.read_bytes())
    if prx[info + 3] != MODULE_INFO_MINOR:
        prx[info + 3] = MODULE_INFO_MINOR
        PRX.write_bytes(bytes(prx))
        print(f"module-info minor version set to {MODULE_INFO_MINOR}")
    else:
        print(f"module-info minor version already {MODULE_INFO_MINOR}")

    run(sys.executable, "-B", SIBLING / "tools" / "sign_native_self.py", PRX, SPRX, "--vsh")

    print()
    print("=" * 60)
    print(f"VSH plugin built: {SPRX}")
    print(f"Bytes  : {SPRX.stat().st_size}")
    print(f"SHA-256: {sha256(SPRX)}")
    print("=" * 60)


if __name__ == "__main__":
    main()
