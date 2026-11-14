"""Offline package/import/direct-syscall contracts for the isolated diagnostic."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "build/hen-diagnostic"
sys.path.insert(0, str(ROOT / "tools"))
from pkg_inspect import members
from elftools.elf.elffile import ELFFile

CONTENT_ID = "UP0001-CRTHEN001_00-CRTHENDIAG000001"
ALLOWED_VIDEO = {
    "cellVideoOutGetState", "cellVideoOutGetConfiguration",
    "cellVideoOutGetNumberOfDevice", "cellVideoOutGetDeviceInfo",
}
DENIED_SYMBOLS = (
    "cellVideoOutConfigure", "cellVideoOutSetupDisplay", "cellVideoOutDebugSetMonitorType",
    "cellVideoOutSetGamma", "cellGcm", "sys_rsx", "lv1_", "load_vsh_plugin",
    "unload_vsh_plugin", "sys_uart", "sys_sm", "sys_net",
)


def check_symbols(names):
    for name in names:
        clean = name.lstrip("._")
        assert not any(clean.startswith(bad) for bad in DENIED_SYMBOLS), f"Denied symbol: {name}"
        if clean.startswith("cellVideoOut"):
            assert clean in ALLOWED_VIDEO, f"Unexpected video import: {name}"


def sfo_values(data):
    assert data[:4] == b"\0PSF"
    _, key_at, data_at, count = struct.unpack_from("<IIII", data, 4)
    result = {}
    for i in range(count):
        key, fmt, length, maximum, offset = struct.unpack_from("<HHIII", data, 20 + i * 16)
        start = key_at + key
        name = data[start:data.index(0, start)].decode()
        value = data[data_at + offset:data_at + offset + length]
        result[name] = value.rstrip(b"\0").decode() if fmt == 0x204 else value
    return result


def executable_words(path):
    with path.open("rb") as f:
        elf = ELFFile(f)
        return [int.from_bytes(s.data()[at:at + 4], "big")
                for s in elf.iter_sections() if s["sh_flags"] & 4
                for at in range(0, len(s.data()) - 3, 4)]


def symbol_names(path, undefined_only=False):
    """Read ELF symbols directly; never launch BFD tools/plugin scanning."""
    with path.open("rb") as f:
        elf = ELFFile(f)
        result = set()
        for section_name in (".symtab", ".dynsym"):
            section = elf.get_section_by_name(section_name)
            if section is None:
                continue
            for symbol in section.iter_symbols():
                if symbol.name and (not undefined_only or symbol["st_shndx"] == "SHN_UNDEF"):
                    result.add(symbol.name)
        return result


def imports(path):
    with path.open("rb") as f:
        elf = ELFFile(f)
        def read(address, size):
            for sec in elf.iter_sections():
                if sec["sh_addr"] <= address and address + size <= sec["sh_addr"] + sec["sh_size"]:
                    return sec.data()[address - sec["sh_addr"]:address - sec["sh_addr"] + size]
            raise AssertionError(f"Unmapped import address {address:x}")
        result = []
        data = elf.get_section_by_name(".lib.stub").data()
        at = 0
        while at < len(data):
            length = data[at]
            assert length == 44 and at + length <= len(data)
            count = struct.unpack_from(">H", data, at + 6)[0]
            name, nids = struct.unpack_from(">II", data, at + 16)
            library = bytearray()
            while True:
                ch = read(name + len(library), 1)
                if ch == b"\0":
                    break
                library.extend(ch)
                assert len(library) < 80
            values = struct.unpack(">" + "I" * count, read(nids, count * 4))
            result.append({"library": library.decode(), "nids": [f"{n:08x}" for n in values]})
            at += length
        return result


def main():
    global OUT, CONTENT_ID
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", type=Path, default=os.environ.get("PS3DK", r"E:\Tools\ps3dk\ps3-sdk-v0.26.0-windows-x86_64"))
    parser.add_argument("--public-only", action="store_true")
    args = parser.parse_args()
    if args.public_only:
        OUT = ROOT / "build/hen-diagnostic-public"
        CONTENT_ID = "UP0001-CRTHEN002_00-CRTPUBDIAG000001"
    native = OUT / "native"
    pkg = OUT / ("deliverables/CRT-Public-Video-Diagnostic-0.1.0.pkg" if args.public_only else "deliverables/CRT-HEN-Diagnostic-0.1.0.pkg")
    raw = pkg.read_bytes()
    assert raw[48:96].split(b"\0", 1)[0].decode() == CONTENT_ID
    files = members(pkg)
    assert set(files) == {"PARAM.SFO", "ICON0.PNG", "USRDIR/EBOOT.BIN", "USRDIR/DIAGNOSTIC.txt"}
    for name, data in files.items():
        original = (OUT / "staging" / name).read_bytes()
        if name == "USRDIR/EBOOT.BIN":
            assert data.startswith(original) and not any(data[len(original):])
        else:
            assert data == original
    values = sfo_values(files["PARAM.SFO"])
    assert values["TITLE_ID"] == ("CRTHEN002" if args.public_only else "CRTHEN001")
    assert values["TITLE"] == ("CRT Public Video Diagnostic" if args.public_only else "CRT HEN Diagnostic")
    assert CONTENT_ID.encode() in files["USRDIR/EBOOT.BIN"], "NPDRM content ID mismatch"
    assert b"PS240P001" not in files["USRDIR/EBOOT.BIN"]

    names = symbol_names(native / "crt-hen-diagnostic")
    assert names, "Missing linked executable symbols"
    check_symbols(names)
    if args.public_only:
        assert not any(name.lstrip("._") == "hd_native_raw" for name in names), "Public-only binary must omit the raw transport"
    for expected in ALLOWED_VIDEO:
        assert expected in names or "." + expected in names, f"Missing expected getter {expected}"
    # Check that the denylist itself fails for a forbidden video setter.
    try:
        check_symbols({"cellVideoOutConfigure"})
    except AssertionError:
        pass
    else:
        raise AssertionError("Setter rejection did not fail closed")

    objects = native / "CMakeFiles/crt-hen-diagnostic.dir"
    transport = executable_words(objects / "native.c.obj")
    assert transport.count(0x44000002) == (0 if args.public_only else 1), "Wrong direct sc-0 transport count"
    if not args.public_only:
        site = transport.index(0x44000002)
        assert 0x39600008 in transport[max(0, site - 32):site], "Transport must load r11 with syscall 8"
    assert 0x44000022 not in transport, "Direct hypervisor call forbidden"
    for filename in ("main.c.obj", "diagnostic.c.obj"):
        words = executable_words(objects / filename)
        assert not any((word & 0xfc0007ff) == 0x44000002 for word in words), f"Unexpected syscall in {filename}"
    all_words = executable_words(native / "crt-hen-diagnostic.elf")
    assert 0x44000022 not in all_words, "Hypervisor call in linked executable"
    # Source side of the boundary: one direct syscall macro, never a runtime syscall number.
    source = (ROOT / "experiments/hen_diagnostic/native.c").read_text()
    assert re.findall(r"lv2syscall\d\s*\(([^)]*)\)", source) == ["8,op,a,b,c,d,e"]
    import_info = imports(native / "crt-hen-diagnostic.elf")
    sysutil = [item for item in import_info if item["library"] == "cellSysutil"]
    assert len(sysutil) == 1 and set(sysutil[0]["nids"]) == {
        "887572d5", "15b0b0cd", "75bbb672", "1e930eef"
    }, "Only the four declared video getter imports may enter cellSysutil"
    # SDK/newlib can retain unused socket helpers; task object references must
    # still contain no networking, helper loading or video-changing function.
    for filename in ("main.c.obj", "diagnostic.c.obj", "native.c.obj"):
        refnames = symbol_names(objects / filename, undefined_only=True)
        check_symbols(refnames)
        assert not refnames.intersection({"socket", "connect", "send", "recv", "gethostbyname"})
    assert not any((word & 0xfc0007ff) == 0x44000002 for word in executable_words(objects / "video_imports.S.obj"))
    result = {"passed": True, "content_id": CONTENT_ID, "title_id": values["TITLE_ID"],
              "package_sha256": hashlib.sha256(raw).hexdigest(), "imports": import_info,
              "direct_transport_sc0_sites": 0 if args.public_only else 1, "direct_hypervisor_sites": 0,
              "public_only": args.public_only,
              "symbol_reader": "pyelftools (no external binary tools)",
              "runtime_hen_validation": "UNKNOWN"}
    (OUT / "artifact-checks.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print("PASS: decrypted PKG identity/members; separate NPDRM identity; getter imports; zero custom syscall sites; no direct hypervisor calls" if args.public_only else "PASS: decrypted PKG identity/members; separate NPDRM identity; getter imports; one syscall-8 transport; no direct hypervisor calls")


if __name__ == "__main__":
    main()
