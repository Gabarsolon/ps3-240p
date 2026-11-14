"""Read-only 4.93 sysmgr discovery through webMAN HTTP LV1 peeks.

No installers, payloads, mode changes, writes, or broad memory scan. Reads
the known 32-slot table and bounded process/domain/page metadata. Identifies
the supported sysmgr image by immutable entry hash plus two data pointers.
Raw responses stay under local_only; share report.json rather than HTML.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import html
import ipaddress
import json
from pathlib import Path
import re
from urllib.request import urlopen

LIMIT = 0x2000000
TABLE = 0x36CDB0
DOMAIN_VTABLE = 0x35ECB8
ENTRY = 0x80005E78
ENTRY_SHA256 = "3ab766ca914a9fe2948403f023f051ff59415322767210bdbeb431875ead0e7e"


def bounded(address, length, alignment=1):
    return 0x1000 <= address and 0 < length <= LIMIT and address <= LIMIT-length and address % alignment == 0


def parse_view(raw, base):
    plain = html.unescape(re.sub(r"<[^>]+>", " ", raw.decode("latin-1")))
    rows = {}
    for match in re.finditer(r"([0-9A-Fa-f]{8})\s+((?:[0-9A-Fa-f]{2} ){16})", plain):
        at, data = int(match[1], 16), bytes.fromhex(match[2])
        if at in rows:
            raise ValueError("Duplicate rows in LV1 response")
        rows[at] = data
    if not all(base+i in rows for i in range(0, 512, 16)):
        raise ValueError("Incomplete LV1 view; endpoint unavailable or unexpected response")
    return b"".join(rows[base+i] for i in range(0, 512, 16))


class Reader:
    def __init__(self, ip, folder):
        self.ip, self.folder, self.cache = str(ipaddress.IPv4Address(ip)), folder, {}
        folder.mkdir(parents=True, exist_ok=True)

    def read(self, address, length):
        if not bounded(address, length):
            raise ValueError("Refused read outside ordinary LV1 RAM bounds")
        out = bytearray()
        while length:
            base = address & ~511
            if not bounded(base, 512):
                raise ValueError("Refused out-of-bounds HTTP view")
            if base not in self.cache:
                with urlopen(f"http://{self.ip}/peek.lv1?{base:x}", timeout=10) as response:
                    raw = response.read(2*1024*1024+1)
                if len(raw) > 2*1024*1024:
                    raise ValueError("Oversized LV1 response")
                (self.folder/f"{base:08x}.html").write_bytes(raw)
                self.cache[base] = parse_view(raw, base)
            take = min(length, base+512-address)
            out += self.cache[base][address-base:address-base+take]
            address += take
            length -= take
        return bytes(out)

    def u64(self, address):
        if not bounded(address, 8, 8):
            raise ValueError("Invalid aligned LV1 pointer")
        return int.from_bytes(self.read(address, 8), "big")


def process(reader, slot, pointer):
    result = {"slot": slot, "process": f"0x{pointer:x}", "identity_match": False}
    if not bounded(pointer, 0x28, 8):
        raise ValueError("Process pointer is outside bounds or unaligned")
    header = reader.read(pointer, 0x28)
    pid, domain = int.from_bytes(header[:4], "big"), int.from_bytes(header[0x18:0x20], "big")
    result.update(pid=pid, domain=f"0x{domain:x}")
    if pid != slot:
        result["error"] = "Process header PID differs from table slot"
        return result
    if not bounded(domain, 0x40, 8):
        result["error"] = "Invalid domain pointer"
        return result
    data = reader.read(domain, 0x40)
    vtable = int.from_bytes(data[:8], "big")
    result["domain_vtable"] = f"0x{vtable:x}"
    if vtable != DOMAIN_VTABLE:
        return result
    node, sentinel = int.from_bytes(data[0x3c:0x40], "big"), domain+0x28
    pages, seen = [], set()
    while node != sentinel:
        if len(seen) >= 512 or node in seen or not bounded(node, 32, 8):
            raise ValueError("Invalid, cyclic, or excessive protection-page list")
        seen.add(node)
        data = reader.read(node, 32)
        physical, va = int.from_bytes(data[:8], "big"), int.from_bytes(data[8:12], "big")
        shift = data[24]
        if not 12 <= shift <= 32:
            raise ValueError("Unsupported protection-page size")
        pages.append((physical, va, 1 << shift))
        node = int.from_bytes(data[20:24], "big")
    result["page_count"] = len(pages)

    def virtual(address, length):
        matches = [(p, v, size) for p, v, size in pages if v <= address and address+length <= v+size]
        if len(matches) != 1:
            raise ValueError("Missing or ambiguous identity mapping")
        physical, va, size = matches[0]
        if not bounded(physical, size) or not bounded(physical+address-va, length):
            raise ValueError("Identity target is not bounded ordinary RAM")
        return reader.read(physical+address-va, length)

    entry = virtual(ENTRY, 32)
    result["entry_sha256"] = hashlib.sha256(entry).hexdigest()
    pointers = [int.from_bytes(virtual(va, 8), "big") for va in (0xC000AF68, 0xC000AF70)]
    result["anchor_pointers"] = [f"0x{value:x}" for value in pointers]
    result["identity_match"] = result["entry_sha256"] == ENTRY_SHA256 and pointers == [0x80038AD8, 0x80038B10]
    # A cached snapshot is diagnostic evidence, never write authorization.
    return result


def scan(reader):
    table = reader.read(TABLE, 512)
    report = {"layout": "4.93 supported table; other layouts UNKNOWN", "table": f"0x{TABLE:x}",
              "all_zero": not any(table), "processes": [], "matching_slots": []}
    for slot in range(32):
        pointer = int.from_bytes(table[slot*16+8:slot*16+16], "big")
        if not pointer:
            continue
        try:
            row = process(reader, slot, pointer)
        except (ValueError, OSError) as error:
            row = {"slot": slot, "process": f"0x{pointer:x}", "identity_match": False, "error": str(error)}
        report["processes"].append(row)
        if row["identity_match"]:
            report["matching_slots"].append(slot)
    count = len(report["matching_slots"])
    report["result"] = "unique identity match (snapshot only)" if count == 1 else "no identity match" if not count else "ambiguous identity matches"
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ip", required=True, help="Console IPv4 address; webMAN must already be running")
    args = parser.parse_args()
    stamp = datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S")
    folder = Path(__file__).resolve().parents[1]/"local_only/sysmgr-diagnostic"/stamp
    reader = Reader(args.ip, folder/"raw")
    try:
        report = scan(reader)
    except (ValueError, OSError) as error:
        report = {"result": "diagnostic failed", "error": str(error)}
    report.update(time=datetime.now(timezone.utc).isoformat(), read_only=True,
                  warning="Snapshot only; no alternate PID or firmware layout has been patched")
    path = folder/"report.json"
    path.write_text(json.dumps(report, indent=2)+"\n")
    print(json.dumps(report, indent=2))
    print(f"Share this report: {path}")
    return 0 if len(report.get("matching_slots", [])) == 1 else 1


if __name__ == "__main__":
    raise SystemExit(main())
