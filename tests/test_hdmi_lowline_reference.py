"""Independent timing, packet and source-memory checks. No console access."""
import importlib.util
from fractions import Fraction
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("hdmi_lowline_reference", ROOT / "tools/hdmi_lowline_reference.py")
low = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = low
spec.loader.exec_module(low)


class LowlineTests(unittest.TestCase):
    def test_cta_ntsc_transport(self):
        r = low.reference("ntsc")
        self.assertEqual((r.vic, r.clock_hz, r.h, r.v),
                         (8, 27_000_000, (1440, 1478, 1602, 1716), (240, 244, 247, 262)))

    def test_cta_pal_transport(self):
        r = low.reference("pal")
        self.assertEqual((r.vic, r.clock_hz, r.h, r.v),
                         (23, 27_000_000, (1440, 1464, 1590, 1728), (288, 290, 293, 312)))

    def test_repeat_and_native_encoding_keep_line_and_frame_periods(self):
        for family in ("ntsc", "pal"):
            a, b = low.reference(family), low.reference(family, encoding="native2880")
            self.assertEqual(a.refresh, b.refresh)
            self.assertEqual(Fraction(a.clock_hz, a.h[3]), Fraction(b.clock_hz, b.h[3]))
            self.assertEqual((a.pixel_repeat, b.pixel_repeat), (1, 0))
            self.assertEqual((b.h[0], b.clock_hz), (2880, 54_000_000))

    def test_nominal_labels_do_not_hide_actual_integer_line_refresh(self):
        ntsc, pal = low.reference("ntsc"), low.reference("pal")
        self.assertAlmostEqual(float(ntsc.refresh), 60.054449, places=6)
        self.assertAlmostEqual(float(pal.refresh), 50.080128, places=6)
        self.assertNotEqual(ntsc.refresh, Fraction(60000, 1001))
        self.assertNotEqual(pal.refresh, 50)

    def test_all_vics_aspects_and_packet_checksums(self):
        for family, encoding, vic in (("ntsc", "repeat2", 8), ("pal", "repeat2", 23),
                                       ("ntsc", "native2880", 12), ("pal", "native2880", 27)):
            for aspect, offset in (("4:3", 0), ("16:9", 1)):
                r = low.reference(family, aspect, encoding)
                p = r.avi_packet()
                self.assertEqual((p[:3], len(p), sum(p) & 255), (bytes((0x82, 2, 13)), 17, 0))
                self.assertEqual(p[7], vic + offset)
                self.assertEqual(p[8] & 15, 1 if encoding == "repeat2" else 0)
                self.assertEqual((p[5] >> 4) & 3, 1 + offset)

    def test_alternate_rows_fit_and_preserve_source_extent(self):
        for family, last, height in (("ntsc", 478, 480), ("pal", 574, 576)):
            for pitch in (2880, 3072, 4096, 32760):
                b = low.reference(family).row_fetch_bounds(pitch)
                self.assertEqual((b["last_source_row"], b["source_height"]), (last, height))
                self.assertEqual(b["proposed_fetch_pitch"], pitch * 2)
                self.assertLessEqual(b["end_exclusive"], b["source_extent_bytes"])
                self.assertLess(b["proposed_fetch_pitch"], 65536)

    def test_invalid_pitch_and_formats_refused(self):
        r = low.reference("ntsc")
        for pitch in (0, -8, 2872, 2881, 32768, 65536, True, 3072.0):
            with self.assertRaises(ValueError):
                r.row_fetch_bounds(pitch)
        with self.assertRaises(ValueError):
            r.row_fetch_bounds(3072, 3)

    def test_unknown_firmware_refused_before_parsing(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "not-firmware.elf"
            path.write_bytes(b"self-authored unknown input")
            with self.assertRaisesRegex(ValueError, "differs from the audited"):
                low.audit_firmware(path)
            self.assertEqual(path.read_bytes(), b"self-authored unknown input")

    def test_invalid_mode_names(self):
        for args in (("other",), ("ntsc", "5:4"), ("pal", "4:3", "other")):
            with self.assertRaises(ValueError):
                low.reference(*args)


if __name__ == "__main__":
    unittest.main()
