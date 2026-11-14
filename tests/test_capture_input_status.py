"""Offline live-size decoding checks; no USB or console access."""
import sys,unittest
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import capture_input_status as status
class TimingTests(unittest.TestCase):
 def decode(self,scan,width,height):
  replies={0xe180:bytes([scan]),0xe184:width.to_bytes(2,'little'),0xe18c:height.to_bytes(2,'little')}
  calls=[]
  def read(device,address,length):
   calls.append((address,length));self.assertEqual(len(replies[address]),length);return replies[address]
  with patch.object(status,'_read_report',side_effect=read):result=status.read_timing_once(None)
  self.assertEqual(calls,[(0xe180,1),(0xe184,2),(0xe18c,2)])
  return result
 def test_progressive_1200(self):
  r=self.decode(7,1920,1200);self.assertEqual((r['live_reported_width'],r['live_reported_height']),(1920,1200));self.assertTrue(r['live_reported_signal_present'])
 def test_interlace_fields(self):
  r=self.decode(15,1920,540);self.assertEqual(r['live_reported_height'],1080)
 def test_unknown_is_not_signal_proof(self):
  self.assertIsNone(self.decode(6,1920,1200)['live_reported_signal_present'])
if __name__=='__main__':unittest.main()
