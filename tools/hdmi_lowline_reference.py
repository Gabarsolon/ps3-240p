"""Offline 240p/288p HDMI references and optional private 4.93 ELF audit.

Never connects to a console or changes a display. Timings describe Linux's
CTA table entries, not a tested PS3 patch. See docs/HDMI_LOWLINE.md.
"""

import argparse
from dataclasses import dataclass
from fractions import Fraction
import hashlib
import json
from pathlib import Path


CTA_SOURCE = "https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/drm_edid.c"
AVI_SOURCE = "https://github.com/torvalds/linux/blob/v6.12/drivers/video/hdmi.c"
FIRMWARE_SHA256 = "07c0bd2a2403f65b6d741e11a15106114365cb78165833bac726476f671b64c9"


@dataclass(frozen=True)
class Reference:
    family: str
    aspect: str
    encoding: str
    vic: int
    sample_multiplier: int
    pixel_repeat: int  # AVI value: transmitted samples per logical pixel minus one.
    h: tuple
    v: tuple
    clock_hz: int

    @property
    def refresh(self):
        return Fraction(self.clock_hz, self.h[3] * self.v[3])

    @property
    def source_height(self):
        return self.v[0] * 2

    def avi_packet(self):
        """Self-authored RGB/BT.601 AVI reference; no Sony template bytes."""
        payload = bytearray(13)
        payload[0] = 0x10  # Active-format information present.
        payload[1] = 0x40 | ((1 if self.aspect == "4:3" else 2) << 4) | 8
        payload[3] = self.vic
        payload[4] = self.pixel_repeat
        packet = bytearray((0x82, 2, 13, 0)) + payload
        packet[3] = (-sum(packet)) & 0xff
        return bytes(packet)

    def row_fetch_bounds(self, pitch, bytes_per_pixel=4):
        """Bounds for a proposed alternate-row fetch, not an RSX patch."""
        if (type(pitch) is not int or type(bytes_per_pixel) is not int
                or bytes_per_pixel not in (2, 4) or pitch < 720 * bytes_per_pixel
                or pitch >= 32768 or pitch & 7):
            raise ValueError("Require aligned source pitch below 32768 and a 720-pixel row")
        last_row = (self.v[0] - 1) * 2
        end = last_row * pitch + 720 * bytes_per_pixel
        allocation = self.source_height * pitch
        if end > allocation:
            raise ValueError("Alternate-row fetch exceeds the original source extent")
        return {"source_width": 720, "source_height": self.source_height,
                "output_height": self.v[0], "source_pitch": pitch,
                "proposed_fetch_pitch": pitch * 2, "last_source_row": last_row,
                "end_exclusive": end, "source_extent_bytes": allocation}

    def report(self):
        return {"family": self.family, "aspect": self.aspect,
                "encoding": self.encoding, "vic": self.vic,
                "logical_source_width": 720, "transmitted_active_samples": self.h[0],
                "sample_multiplier": self.sample_multiplier, "avi_pixel_repeat": self.pixel_repeat,
                "horizontal": dict(zip(("active", "sync_start", "sync_end", "total"), self.h)),
                "vertical": dict(zip(("active", "sync_start", "sync_end", "total"), self.v)),
                "sample_clock_hz": self.clock_hz, "interlaced": False,
                "line_hz": float(Fraction(self.clock_hz, self.h[3])),
                "refresh_hz": float(self.refresh), "refresh_fraction": str(self.refresh),
                "avi_reference_hex": self.avi_packet().hex(),
                "example_fetch_bounds": self.row_fetch_bounds(3072)}


def reference(family, aspect="4:3", encoding="repeat2"):
    if family not in ("ntsc", "pal") or aspect not in ("4:3", "16:9"):
        raise ValueError("Unknown family or aspect")
    if encoding not in ("repeat2", "native2880"):
        raise ValueError("Unknown HDMI sample encoding")
    # PROVEN in the pinned Linux CTA table (not measured PS3 timings).
    horizontal = (720, 739, 801, 858) if family == "ntsc" else (720, 732, 795, 864)
    vertical = (240, 244, 247, 262) if family == "ntsc" else (288, 290, 293, 312)
    multiplier = 2 if encoding == "repeat2" else 4
    vic = (8 if family == "ntsc" else 23) if multiplier == 2 else (12 if family == "ntsc" else 27)
    return Reference(family, aspect, encoding, vic + (aspect == "16:9"), multiplier,
                     1 if multiplier == 2 else 0,
                     tuple(value * multiplier for value in horizontal), vertical,
                     13_500_000 * multiplier)


def audit_firmware(path):
    """Read a known private input; return addresses/hashes/decoded facts only."""
    from elftools.elf.elffile import ELFFile

    raw = Path(path).read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    if digest != FIRMWARE_SHA256:
        raise ValueError("Private ELF differs from the audited retail 4.93 sysmgr image")
    with Path(path).open("rb") as stream:
        elf = ELFFile(stream)
        if elf.little_endian or elf.elfclass != 64 or elf["e_machine"] != "EM_PPC64":
            raise ValueError("Expected big-endian PPC64 ELF")
        segments = [(s["p_vaddr"], s.data()) for s in elf.iter_segments() if s["p_type"] == "PT_LOAD"]

    def read(address, size):
        for base, data in segments:
            if base <= address and address + size <= base + len(data):
                return data[address - base:address - base + size]
        raise ValueError(f"Unmapped file-backed ELF address {address:#x}")

    def u32(address):
        return int.from_bytes(read(address, 4), "big")

    templates = []
    for index in range(25):  # Bound established by the checked 0x800105d0 selector.
        address = 0x8003997b + 12 * index
        data = read(address, 12)
        templates.append({"index": index, "address": hex(address), "vic": data[2],
                          "repeat_low_nibble": data[0] & 15,
                          "sha256": hashlib.sha256(data).hexdigest()})
    profiles = []
    for index in range(1, 7):
        meta, raster = 0x80039148 + index * 24, 0x80038b28 + index * 56
        profiles.append({"profile": index, "metadata_address": hex(meta), "raster_address": hex(raster),
                         "source_interlace": u32(meta + 4), "source_multiplier": u32(meta + 8),
                         "width_samples": u32(meta + 12), "source_height": u32(meta + 16),
                         "active_lines": u32(raster + 16) + 1, "total_lines": u32(raster + 20) + 1,
                         "sha256": hashlib.sha256(read(meta, 24) + read(raster, 56)).hexdigest()})
    existing = {row["vic"] for row in templates}
    return {"sysmgr_sha256": digest, "templates": templates, "sd_profiles": profiles,
            "lowline_vics_absent": sorted({8, 9, 12, 13, 23, 24, 27, 28} - existing),
            "sd_backend_routes": [
                {"family": family, "backend_id": backend,
                 "format_code": hex(u32(0xc00003fc + backend * 4)), "template_index": index,
                 "stock_vic": templates[index]["vic"], "reference_vic": vic}
                for family, backend, index, vic in (("ntsc", 0, 2, 8), ("pal", 5, 13, 23))],
            "live_support": "UNKNOWN; an ELF audit does not establish HDMI output or receiver support"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--family", choices=("ntsc", "pal", "both"), default="both")
    parser.add_argument("--aspect", choices=("4:3", "16:9"), default="4:3")
    parser.add_argument("--encoding", choices=("repeat2", "native2880"), default="repeat2")
    parser.add_argument("--firmware", type=Path, help="Optional private known 4.93 sysmgr_ss.elf; read only")
    args = parser.parse_args()
    data = {"status": "Offline reference only; PS3 HDMI 240p/288p NOT verified",
            "sources": [CTA_SOURCE, AVI_SOURCE],
            "modes": [reference(family, args.aspect, args.encoding).report()
                      for family in (("ntsc", "pal") if args.family == "both" else (args.family,))]}
    if args.firmware:
        try:
            data["firmware_audit"] = audit_firmware(args.firmware)
        except (OSError, ValueError) as exc:
            parser.error(str(exc))
    print(json.dumps(data, indent=2))


if __name__ == "__main__":
    main()
