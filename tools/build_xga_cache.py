"""Assemble the self-authored XGA cache wrapper. Offline; no firmware input."""
from pathlib import Path
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SDK = Path(os.environ.get("PS3DK", r"E:\Tools\ps3dk\ps3-sdk-v0.26.0-windows-x86_64"))


def build(custom=False):
    folder = ROOT / ("build/hdmi-custom/cache" if custom else "build/xga-probe/cache")
    folder.mkdir(parents=True, exist_ok=True)
    obj, binary = folder / "cache.o", folder / "cache.bin"
    subprocess.run([str(SDK / "ppu/bin/ppu-as.exe"), "-m64", "-mppc64"] +
                   (["--defsym", "CRT_CUSTOM_HDMI=1"] if custom else []) + [
                    str(ROOT / "experiments/display_probe/xga_cache.S"), "-o", str(obj)], check=True)
    subprocess.run([str(SDK / "ppu/bin/ppu-objcopy.exe"), "-O", "binary", "-j", ".text",
                    str(obj), str(binary)], check=True)
    data = binary.read_bytes()
    # Audited ELF memory ends at8003f6c8; mapped-page padding ends at80040000.
    # The wrapper starts at8003f800. Runtime also requires the whole arena empty.
    assert len(data) % 8 == 0 and len(data) <= 1024
    # The shared, tested lifecycle uses these identifiers for either wrapper.
    header = ("/* SPDX-License-Identifier: MIT. Generated from our xga_cache.S. */\n"
              "#ifndef XGA_CACHE_PAYLOAD_H\n#define XGA_CACHE_PAYLOAD_H\n#include <stdint.h>\n"
              f"#define HD540_CACHE_QWORDS {len(data)//8}u\n"
              "static const uint64_t hd540_cache_words[HD540_CACHE_QWORDS]={\n")
    header += "".join(f" UINT64_C(0x{int.from_bytes(data[i:i+8], 'big'):016x}),\n"
                      for i in range(0, len(data), 8))
    header += "};\n#endif\n"
    target = ROOT / ("experiments/hdmi_custom/cache_payload.h" if custom else "experiments/display_probe/xga_cache_payload.h")
    if not target.exists() or target.read_text() != header:
        target.write_text(header)
    print(f"Self-authored XGA cache wrapper: {len(data)} bytes")
    return binary


if __name__ == "__main__":
    build()
