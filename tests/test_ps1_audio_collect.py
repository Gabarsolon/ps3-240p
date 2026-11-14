"""Resident container validation with synthesized float PCM only."""
import importlib.util
from pathlib import Path
import struct
import sys
import unittest

TOOLS = Path(__file__).resolve().parents[1] / "tools"
sys.path.insert(0, str(TOOLS))
spec = importlib.util.spec_from_file_location("ps1_audio_collect", TOOLS / "ps1_audio_collect.py")
collect = importlib.util.module_from_spec(spec)
spec.loader.exec_module(collect)

def container():
    pcm = struct.pack(">2f", .25, -.25) * 2048
    return bytearray(b"".join(struct.pack(">6I2Q", 0x41554452, 42, 99, i % 8, i % 8, i,
                                         i * 20000, i * 20000 + 100) + pcm for i in range(64)))

class ContainerTests(unittest.TestCase):
    def test_stable_snippets_are_not_called_continuous_audio(self):
        report = collect.analyze(container(), 42)
        self.assertEqual(report["consumer_stable_records"], 64)
        self.assertEqual(report["max_read_us"], 100)
        self.assertIn("not continuous", report["scope"])
        self.assertEqual(report["rows"][0]["rms"], [.25, .25])

    def test_incomplete_container_or_wrong_request_is_rejected(self):
        with self.assertRaises(ValueError):
            collect.analyze(container()[:-1], 42)
        with self.assertRaises(ValueError):
            collect.analyze(container(), 43)

    def test_bad_cursor_identity_or_timestamp_is_rejected(self):
        for offset, fmt, value in ((12, ">I", 8), (16, ">I", 8),
                                   (collect.RECORD_BYTES + 8, ">I", 100), (24, ">Q", 999999)):
            data = container()
            struct.pack_into(fmt, data, offset, value)
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                collect.analyze(data, 42)

    def test_invalid_pcm_is_rejected(self):
        for value in (float("nan"), float("inf"), 1e20):
            data = container()
            struct.pack_into(">f", data, collect.RECORD.size, value)
            with self.subTest(value=value), self.assertRaises(ValueError):
                collect.analyze(data, 42)

if __name__ == "__main__":
    unittest.main()
