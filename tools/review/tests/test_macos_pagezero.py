import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[3]
spec=importlib.util.spec_from_file_location('bt4_package_native',ROOT/'port/tools/package_native.py')
native=importlib.util.module_from_spec(spec);spec.loader.exec_module(native)


class MacPageZeroTests(unittest.TestCase):
    def check_image(self,size):
        image=bytearray(104)
        struct.pack_into('<I',image,0,0xFEEDFACF)
        struct.pack_into('<II',image,16,1,72)
        struct.pack_into('<II16sQQ',image,32,0x19,72,b'__PAGEZERO',0,size)
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder)/'runtime';path.write_bytes(image)
            native.require_mac_pagezero(path)

    def test_upstream_fixed_header(self):self.check_image(0x100000000)
    def test_zero_length_rejected(self):
        with self.assertRaises(RuntimeError):self.check_image(0)
    def test_short_pagezero_rejected(self):
        with self.assertRaises(RuntimeError):self.check_image(4096)
