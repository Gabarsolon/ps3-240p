"""Offline checks for capture classification and conservative inbox publication."""
import importlib.util
import io
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("auto", ROOT / "tools/hd540_auto_test.py")
auto = importlib.util.module_from_spec(spec)
spec.loader.exec_module(auto)


class FakeFTP:
    def __init__(self, files):
        self.files, self.mutations = files, []
        self.drop_stage = self.lose_rename_ack = False

    def storbinary(self, command, stream):
        path = command.removeprefix("STOR ")
        self.files[path] = b"bad" if self.drop_stage else stream.read()
        self.mutations.append(("stage", path))

    def rename(self, source, target):
        if target in self.files:
            raise auto.ftplib.error_perm("550 destination exists")
        self.files[target] = self.files.pop(source)
        self.mutations.append(("rename", source, target))
        if self.lose_rename_ack and target == auto.REQUEST:
            raise OSError("connection lost after rename")


class AutomationTests(unittest.TestCase):
    def make_console(self, folder, state=7):
        c = auto.Console.__new__(auto.Console)
        c.host, c.folder, c.pending = "test", Path(folder), None
        c.ftp = FakeFTP({auto.REQUEST: struct.pack(">4I", auto.MAGIC, 10, 4, 1),
                         auto.REPLY: struct.pack(">4I", auto.MAGIC, 10, state, 0)})
        def read(path, optional=False):
            if path in c.ftp.files:
                return c.ftp.files[path]
            if optional:
                return None
            raise FileNotFoundError(path)
        c.read = read
        c.preflight = c.idle
        return c

    def test_pixels_do_not_call_flat_color_a_picture(self):
        for frame in (np.zeros((100, 100, 3)), np.full((100, 100, 3), 80)):
            self.assertFalse(auto.image_stats(frame)["picture_detected"])
        grid = np.zeros((100, 100, 3))
        grid[:, ::2] = 100
        self.assertTrue(auto.image_stats(grid)["picture_detected"])

    def test_active_requires_mature_multiple_visible_frames(self):
        rows = [dict(state=3, elapsed=n, picture_detected=n > 1) for n in range(8)]
        self.assertTrue(auto.summarize_active(rows)["picture_detected"])
        for row in rows:
            row["picture_detected"] = False
        self.assertFalse(auto.summarize_active(rows)["picture_detected"])
        self.assertFalse(auto.summarize_active(rows[:3])["picture_detected"])

    def test_checked_publication_preserves_acknowledged_inbox(self):
        with tempfile.TemporaryDirectory() as folder:
            c = self.make_console(folder)
            old = c.ftp.files[auto.REQUEST]
            with patch.object(auto.secrets, "randbelow", return_value=20):
                seq = c.publish(6)
            self.assertEqual(seq, 21)
            self.assertEqual(struct.unpack(">4I", c.ftp.files[auto.REQUEST]), (auto.MAGIC, 21, 6, 1))
            self.assertEqual(c.ftp.files[auto.REQUEST + ".auto-21.prev"], old)
            self.assertEqual(c.pending, seq)

    def test_busy_or_unacknowledged_inbox_never_replaced(self):
        with tempfile.TemporaryDirectory() as folder:
            c = self.make_console(folder, state=3)
            with self.assertRaises(RuntimeError):
                c.publish(4)
            self.assertFalse(c.ftp.mutations)
            c = self.make_console(folder)
            c.ftp.files[auto.REQUEST] = struct.pack(">4I", auto.MAGIC, 11, 4, 1)
            with self.assertRaises(RuntimeError):
                c.publish(4)
            self.assertFalse(c.ftp.mutations)

    def test_failed_staging_leaves_original_inbox(self):
        with tempfile.TemporaryDirectory() as folder:
            c = self.make_console(folder)
            old = c.ftp.files[auto.REQUEST]
            c.ftp.drop_stage = True
            with self.assertRaises(RuntimeError):
                c.publish(4)
            self.assertEqual(c.ftp.files[auto.REQUEST], old)
            self.assertIsNone(c.pending)

    def test_uncertain_rename_keeps_recovery_armed(self):
        with tempfile.TemporaryDirectory() as folder:
            c = self.make_console(folder)
            c.ftp.lose_rename_ack = True
            with self.assertRaises(OSError):
                c.publish(4)
            self.assertIsNotNone(c.pending)
            self.assertEqual(struct.unpack(">4I", c.ftp.files[auto.REQUEST])[1], c.pending)

    def test_partial_reply_not_accepted_and_failed_recovery_not_hidden(self):
        with tempfile.TemporaryDirectory() as folder:
            c = self.make_console(folder, state=9)
            c.pending = 10
            with self.assertRaisesRegex(RuntimeError, "recovery failed"):
                c.cancel_pending()
            c.ftp.files[auto.REPLY] = b"partial"
            self.assertIsNone(c.reply())

    def test_live_slots_parse_and_missing_rows_fail_closed(self):
        rows = ''.join(f'<tr><td>{i}<td>{"WWWD" if i == 1 else "NULL"}'
                       f'<td><form action="/vshplugin.ps3mapi"></form></tr>' for i in range(7))
        self.assertEqual(auto.parse_slots(rows)[2], {"name": "NULL", "path": None})
        with self.assertRaises(RuntimeError):
            auto.parse_slots('<tr><td>2<td>NULL</tr>')

    def test_stale_ready_file_does_not_prove_resident_helper(self):
        with tempfile.TemporaryDirectory() as folder:
            c = self.make_console(folder)
            c.verify_files = lambda: None
            c.slots = lambda: {i: {"name": "NULL", "path": None} for i in range(7)}
            c.ftp.files[auto.READY] = b''
            c.preflight = lambda: auto.Console.preflight(c)
            with self.assertRaisesRegex(RuntimeError, "resident helper"):
                c.preflight()

    def test_bootstrap_never_evicts_an_occupied_plugin(self):
        with tempfile.TemporaryDirectory() as folder:
            c = self.make_console(folder)
            c.verify_files = lambda: None
            c.slots = lambda: {i: {"name": "other", "path": "/other.sprx"} for i in range(7)}
            with patch.object(auto, 'urlopen') as network:
                with self.assertRaisesRegex(RuntimeError, "No empty"):
                    c.ensure_helper()
                network.assert_not_called()


if __name__ == "__main__":
    unittest.main()
