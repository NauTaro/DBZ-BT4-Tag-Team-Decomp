"""BT4 ISO identification uses program contents, not an ISO filename or BT3 ID."""
from pathlib import Path
import hashlib,importlib.util,struct,tempfile,unittest
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[3]
spec=importlib.util.spec_from_file_location('bt4_disc',ROOT/'port/tools/bt4_disc.py')
disc=importlib.util.module_from_spec(spec);spec.loader.exec_module(disc)

def record(name,sector,size,directory=False):
    n=33+len(name)+(len(name)%2==0);data=bytearray(n);data[0]=n
    struct.pack_into('<I',data,2,sector);struct.pack_into('<I',data,10,size)
    data[25]=2 if directory else 0;data[32]=len(name);data[33:33+len(name)]=name
    return data

def image(path,bt3=False):
    elf=bytearray(80);elf[:6]=b'\x7fELF\x01\x01';struct.pack_into('<I',elf,32,52)
    struct.pack_into('<HH',elf,46,24,1);struct.pack_into('<6I',elf,52,0,1,2,0x100000,76,4);elf[76:]=b'pose'
    programs=[bytes(elf),b'fake DBZP program',b'fake DBZ4 program'];out=bytearray(26*2048)
    out[16*2048:16*2048+7]=b'\x01CD001\x01'
    row=record(b'\0',20,2048,True);out[16*2048+156:16*2048+156+len(row)]=row
    rows=record(b'SLUS_216.78;1' if bt3 else b'SLUS_219.78;1',22,len(programs[0]))+record(b'BIN',21,2048,True)
    out[20*2048:20*2048+len(rows)]=rows
    rows=record(b'DBZP.BIN;1',23,len(programs[1]))+record(b'DBZ4.BIN;1',24,len(programs[2]))
    out[21*2048:21*2048+len(rows)]=rows
    for sector,data in zip((22,23,24),programs):out[sector*2048:sector*2048+len(data)]=data
    path.write_bytes(out)
    return (hashlib.sha1(disc.flat_image(programs[0])).hexdigest(),hashlib.sha1(programs[1]).hexdigest(),hashlib.sha1(programs[2]).hexdigest())

class Bt4DiscTests(unittest.TestCase):
    def test_each_language_is_selected_by_verified_program_contents(self):
        with tempfile.TemporaryDirectory() as d:
            iso=Path(d)/'unrelated-name.iso';fingerprint=image(iso)
            for language in ['es','en']:
                with patch.dict(disc.PROFILES,{language:fingerprint},clear=True):self.assertEqual(disc.identify(iso),language)
    def test_bt3_id_is_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            iso=Path(d)/'BT4.iso';image(iso,bt3=True)
            with self.assertRaisesRegex(disc.DiscError,'Missing: SLUS_219.78'):disc.identify(iso)
    def test_unsupported_programs_and_truncated_extents_are_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            iso=Path(d)/'BT4.iso';image(iso)
            with self.assertRaisesRegex(disc.DiscError,'Unsupported BT4'):disc.identify(iso)
            iso.write_bytes(iso.read_bytes()[:22*2048])
            with self.assertRaisesRegex(disc.DiscError,'outside the image'):disc.identify(iso)
if __name__=='__main__':unittest.main()
