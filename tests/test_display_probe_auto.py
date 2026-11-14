"""Offline inbox-ownership checks; no console/camera is contacted."""
import importlib.util
import io
from pathlib import Path
import struct
import sys
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import display_probe_test as probe


class FakeFTP:
    def __init__(self,files):
        self.files=files;self.mutations=[];self.corrupt=False;self.lose_ack=False;self.answer_cancel=False
    def storbinary(self,command,stream):
        path=command.removeprefix('STOR ');self.files[path]=b'bad' if self.corrupt else stream.read()
        self.mutations.append(('stage',path))
    def rename(self,source,target):
        if target in self.files:raise probe.ftplib.error_perm('550 exists')
        self.files[target]=self.files.pop(source);self.mutations.append(('rename',source,target))
        if target==probe.REQUEST:
            magic,seq,action,value=struct.unpack('>4I',self.files[target])
            if self.answer_cancel and action==5:self.files[probe.REPLY]=struct.pack('>4I',magic,seq,5,1)
            if self.lose_ack:raise OSError('Connection lost after successful rename')


class ProbePublicationTests(unittest.TestCase):
    def console(self,folder):
        c=probe.DisplayConsole.__new__(probe.DisplayConsole);c.folder=Path(folder);c.host='test';c.pending=None
        c.ftp=FakeFTP({probe.REQUEST:struct.pack('>4I',probe.MAGIC,10,1,1),
                       probe.REPLY:struct.pack('>4I',probe.MAGIC,10,5,0)})
        def read(path,optional=False):
            if path in c.ftp.files:return c.ftp.files[path]
            if optional:return None
            raise FileNotFoundError(path)
        c.read=read;c.preflight=lambda:None
        return c
    def test_acknowledged_inbox_preserved_before_replacement(self):
        with tempfile.TemporaryDirectory() as tmp:
            c=self.console(tmp);old=c.read(probe.REQUEST);seq=c.publish(2)
            self.assertEqual(struct.unpack('>4I',c.read(probe.REQUEST)),(probe.MAGIC,seq,2,1))
            self.assertEqual(c.read(f'{probe.REQUEST}.{seq}.prev'),old)
            self.assertEqual(c.pending,seq)
    def test_partial_or_wrong_ack_causes_zero_mutation(self):
        with tempfile.TemporaryDirectory() as tmp:
            for reply in (b'partial',struct.pack('>4I',probe.MAGIC,9,5,0)):
                c=self.console(tmp);c.ftp.files[probe.REPLY]=reply
                with self.assertRaises(RuntimeError):c.publish(4)
                self.assertEqual(c.ftp.mutations,[])
    def test_bad_staging_leaves_original_owned_inbox(self):
        with tempfile.TemporaryDirectory() as tmp:
            c=self.console(tmp);old=c.read(probe.REQUEST);c.ftp.corrupt=True
            with self.assertRaises(RuntimeError):c.publish(4)
            self.assertEqual(c.read(probe.REQUEST),old);self.assertIsNone(c.pending)
    def test_uncertain_rename_keeps_recovery_armed(self):
        with tempfile.TemporaryDirectory() as tmp:
            c=self.console(tmp);c.ftp.lose_ack=True
            with self.assertRaises(OSError):c.publish(4)
            self.assertIsNotNone(c.pending)
            self.assertEqual(struct.unpack('>4I',c.read(probe.REQUEST))[1],c.pending)
    def test_cancel_uses_its_own_acknowledged_sequence(self):
        with tempfile.TemporaryDirectory() as tmp:
            c=self.console(tmp);c.pending=10;c.ftp.files[probe.REPLY]=struct.pack('>4I',probe.MAGIC,10,3,0)
            c.ftp.answer_cancel=True;c.cancel_pending();self.assertIsNone(c.pending)
            q=struct.unpack('>4I',c.read(probe.REQUEST));r=c.reply()
            self.assertEqual(q[2],5);self.assertNotEqual(q[1],10);self.assertEqual(q[1],r['sequence'])
    def test_latched_recovery_never_sends_another_command(self):
        with tempfile.TemporaryDirectory() as tmp:
            c=self.console(tmp);c.pending=10;c.ftp.files[probe.REPLY]=struct.pack('>4I',probe.MAGIC,10,7,5)
            with self.assertRaises(RuntimeError):c.cancel_pending()
            self.assertEqual(c.ftp.mutations,[])


if __name__=='__main__':unittest.main()
