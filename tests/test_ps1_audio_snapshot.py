"""Safety boundaries using self-authored synthetic process memory only."""
import hashlib
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("ps1_audio_snapshot", Path(__file__).resolve().parents[1] / "tools/ps1_audio_snapshot.py")
audio = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audio)

def context(**changes):
    fields = dict(port=0, padding=0, index=0x20010010, status=1,
                  channels=2, blocks=8, size=16384, buffer=0x20020000,
                  writer=0x20020000, tail=0)
    fields.update(changes)
    return struct.pack(">4I2Q4I", *fields.values())

class Fake:
    def __init__(self, cfg=None):
        self.reads = []
        self.cfg = cfg or context()
        self.identity = b"self-authored audio initializer fixture"
    def processes(self):
        return [(42, "main_ps1_emu.self")]
    def read(self, pid, at, length):
        self.reads.append((at, length))
        if at == audio.INIT_VA:
            return self.identity
        if at == audio.CONTEXT_POINTER:
            return (0x1900000).to_bytes(4, "big")
        if at == 0x1900000:
            return self.cfg
        if at == 0x20010010:
            return (3).to_bytes(8, "big")
        if at == 0x20020000:
            return struct.pack(">2f", .25, -.25) * 2048
        raise AssertionError("Unexpected process read")

class SnapshotTests(unittest.TestCase):
    def test_cobra_pid_prefix_must_match_actual_process(self):
        self.assertEqual(audio.process_name(42, "0000002a_main_ps1_emu.self"), "main_ps1_emu.self")
        self.assertEqual(audio.process_name(42, "0000002A_main_ps1_emu.self"), "main_ps1_emu.self")
        self.assertNotEqual(audio.process_name(43, "0000002a_main_ps1_emu.self"), "main_ps1_emu.self")
    def test_cached_ready_is_not_live_playback_status(self):
        self.assertEqual(audio.port_config(context())["status"], 1)
        self.assertEqual(audio.port_config(context(status=2))["status"], 2)

    def test_bad_geometry_and_pointers_are_rejected(self):
        for changes in ({"status": 0x1010}, {"channels": 8}, {"blocks": 32},
                        {"size": 1}, {"index": 0}, {"buffer": 0x7ffffff8},
                        {"writer": 0x20024000}, {"writer": 0x20020004}, {"port": 8}):
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                audio.port_config(context(**changes))

    def test_unrecognized_firmware_stops_before_context_read(self):
        client = Fake()
        with tempfile.TemporaryDirectory() as folder, self.assertRaises(ValueError):
            audio.snapshot(client, Path(folder), 1, 0)
        self.assertEqual(client.reads, [(audio.INIT_VA, audio.INIT_SIZE)])

    def test_bad_context_stops_before_pcm_read(self):
        client = Fake(context(buffer=0))
        with patch.object(audio, "INIT_SHA256", hashlib.sha256(client.identity).hexdigest()):
            with tempfile.TemporaryDirectory() as folder, self.assertRaises(ValueError):
                audio.snapshot(client, Path(folder), 1, 0)
        self.assertFalse(any(at == 0x20020000 for at, _ in client.reads))

    def test_slow_read_is_never_reported_as_atomic_audio(self):
        client = Fake()
        with patch.object(audio, "INIT_SHA256", hashlib.sha256(client.identity).hexdigest()), patch.object(audio.time, "monotonic", side_effect=[0, 0, 2, 2]):
            with tempfile.TemporaryDirectory() as folder:
                report = audio.snapshot(client, Path(folder), 1, 0)
        self.assertIn("non-atomic", report["scope"])
        self.assertTrue(report["samples"][0]["exceeds_nominal_48khz_ring_period"])
        self.assertFalse(report["samples"][0]["cursor_changed_during_read"])
        self.assertEqual(report["samples"][0]["rms"], [.25, .25])

if __name__ == "__main__":
    unittest.main()
