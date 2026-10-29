"""Exercise the real POSIX VoiceWs worker's queued-send scheduling."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import unittest

ROOT=Path(__file__).resolve().parents[3]


class NativeTransportTests(unittest.TestCase):
    def test_queued_game_frames_drain_and_backpressure_waits(self):
        compiler=os.environ.get('CXX') or shutil.which('c++') or shutil.which('g++')
        if not compiler:
            self.skipTest('A C++ compiler is required')
        result=subprocess.run([sys.executable,str(ROOT/'port/tests/native_transport_queue/run.py'),
                               '--cxx',compiler],check=True,capture_output=True,text=True)
        self.assertEqual(result.stdout.count('result=PASS'),2,result.stdout)
        self.assertIn('sent=200 attempts=200 rx-polls=200 waits=0',result.stdout)
        self.assertIn('sent=200 attempts=207 rx-polls=207 waits=7',result.stdout)


if __name__=='__main__':
    unittest.main()
