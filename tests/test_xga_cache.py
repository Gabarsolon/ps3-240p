"""Offline proof that the assembled wrapper flushes every edited code line."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


class CacheCoverage(unittest.TestCase):
    def test_instruction_lines_and_argument_preservation(self):
        source = (ROOT / "experiments/display_probe/xga_patch.c").read_text()
        targets = source.split("targets[XGA_WORDS]={", 1)[1].split("};", 1)[0]
        addresses = [int(x, 16) for x in re.findall(r"0x[0-9a-f]+", targets)]
        expected = {a & ~127 for a in addresses if 0x80000000 <= a < 0x80038000}
        header = (ROOT / "experiments/display_probe/xga_cache_payload.h").read_text()
        data = b"".join(int(x, 16).to_bytes(8, "big")
                        for x in re.findall(r"UINT64_C\(0x([0-9a-f]+)\)", header))
        self.assertLessEqual(len(data), 1024)
        dcbst, icbi, reg11, callback = [], [], 0, False
        for offset in range(0, len(data), 4):
            word = int.from_bytes(data[offset:offset+4], "big")
            if word == 0x4e800421:  # bctrl to the original callback
                callback = True
                break
            opcode, rt, ra = word >> 26, (word >> 21) & 31, (word >> 16) & 31
            immediate = word & 65535
            signed = immediate - 65536 if immediate & 32768 else immediate
            if opcode in (14, 15):  # addi / addis, including lis
                self.assertNotIn(rt, range(3, 11), "callback argument was clobbered")
                if rt == 11:
                    self.assertIn(ra, (0, 11))
                    reg11 = (0 if ra == 0 else reg11) + (signed << (16 if opcode == 15 else 0))
            elif opcode == 24:  # ori: destination is RA
                self.assertNotIn(ra, range(3, 11))
                if ra == 11:
                    self.assertEqual(rt, 11)
                    reg11 |= immediate
            elif opcode == 31 and (word >> 1) & 1023 in (54, 982):
                self.assertEqual(ra, 0)
                self.assertEqual((word >> 11) & 31, 11)
                (dcbst if (word >> 1) & 1023 == 54 else icbi).append(reg11 & 0xffffff80)
        self.assertTrue(callback)
        self.assertEqual(set(dcbst), expected)
        self.assertEqual(icbi, list(reversed(dcbst)))
        self.assertEqual(len(dcbst), len(expected))


if __name__ == "__main__":
    unittest.main()
