"""Offline synthetic discovery/transport checks; never contacts a console."""
import hashlib
import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/"tools"))
import diagnose_sysmgr as probe


class Memory:
    def __init__(self):
        self.data = {}
        self.reads = []

    def put(self, address, data):
        self.data.update((address+i, value) for i, value in enumerate(data))

    def number(self, address, value, size=8):
        self.put(address, value.to_bytes(size, "big"))

    def read(self, address, length):
        assert probe.bounded(address, length)
        self.reads.append((address, length))
        return bytes(self.data.get(address+i, 0) for i in range(length))


class Discovery(unittest.TestCase):
    def setUp(self):
        self.old_hash = probe.ENTRY_SHA256
        self.addCleanup(setattr, probe, "ENTRY_SHA256", self.old_hash)
        self.entry = bytes(range(32))  # Own synthetic bytes, no firmware.
        probe.ENTRY_SHA256 = hashlib.sha256(self.entry).hexdigest()
        self.mem = Memory()
        self.mem.number(probe.TABLE+3*16+8, 0x100000)
        self.mem.number(0x100000, 3, 4)
        self.mem.number(0x100018, 0x110000)
        self.mem.number(0x110000, probe.DOMAIN_VTABLE)
        self.mem.number(0x11003c, 0x120000, 4)
        for i, (physical, va) in enumerate(((0x1800000, 0x80005000), (0x1500000, 0xc000a000))):
            at = 0x120000+i*32
            self.mem.number(at, physical)
            self.mem.number(at+8, va, 4)
            self.mem.number(at+20, 0x110028 if i else at+32, 4)
            self.mem.put(at+24, bytes([12]))
        self.mem.put(0x1800e78, self.entry)
        self.mem.number(0x1500f68, 0x80038ad8)
        self.mem.number(0x1500f70, 0x80038b10)

    def test_shifted_slot_and_decoy(self):
        self.mem.number(probe.TABLE+10*16+8, 0x140000)
        self.mem.number(0x140000, 10, 4)
        self.mem.number(0x140018, 0x141000)
        self.mem.number(0x141000, 0x3602b8)
        report = probe.scan(self.mem)
        self.assertEqual(report["matching_slots"], [3])
        self.assertIn("unique", report["result"])

    def test_unknown_code(self):
        self.mem.put(0x1800e78, bytes(32))
        self.assertEqual(probe.scan(self.mem)["matching_slots"], [])

    def test_duplicate_identity_is_ambiguous(self):
        self.mem.number(probe.TABLE+10*16+8, 0x140000)
        self.mem.number(0x140000, 10, 4)
        self.mem.number(0x140018, 0x110000)
        report = probe.scan(self.mem)
        self.assertEqual(report["matching_slots"], [3, 10])
        self.assertIn("ambiguous", report["result"])

    def test_page_cycle_stops(self):
        self.mem.number(0x120034, 0x120000, 4)
        report = probe.scan(self.mem)
        self.assertIn("cyclic", report["processes"][0]["error"])
        self.assertLess(len(self.mem.reads), 10)

    def test_no_mmio_or_invalid_pointer_dereference(self):
        self.mem.number(0x120000, 0x28000680000)
        report = probe.scan(self.mem)
        self.assertIn("ordinary RAM", report["processes"][0]["error"])
        self.mem.number(probe.TABLE+3*16+8, 0xfffffffffffffff8)
        report = probe.scan(self.mem)
        self.assertIn("bounds", report["processes"][0]["error"])

    def test_all_zero(self):
        report = probe.scan(Memory())
        self.assertTrue(report["all_zero"])
        self.assertEqual(report["matching_slots"], [])

    def test_pid_mismatch(self):
        self.mem.number(0x100000, 10, 4)
        self.assertIn("PID differs", probe.scan(self.mem)["processes"][0]["error"])

    def test_view_parser_and_incomplete_duplicate_refusal(self):
        base = 0x36cc00
        rows = [f"{base+i:08X} "+"01 "*16 for i in range(0, 512, 16)]
        self.assertEqual(probe.parse_view("<br>".join(rows).encode(), base), b"\1"*512)
        for raw in ("<br>".join(rows[:-1]), "<br>".join(rows+rows[:1])):
            with self.assertRaises(ValueError):
                probe.parse_view(raw.encode(), base)
        self.assertFalse(probe.bounded(probe.LIMIT-7, 8))
        self.assertFalse(probe.bounded(0x28000680000, 8))


if __name__ == "__main__":
    unittest.main()
