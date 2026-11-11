"""Packaging integrity tests. Synthetic signatures are never trust evidence."""
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest
import zipfile

spec=importlib.util.spec_from_file_location('authenticode',Path(__file__).resolve().parents[1]/'tools/windows_authenticode.py')
auth=importlib.util.module_from_spec(spec)
spec.loader.exec_module(auth)

def program(marker):
    data=bytearray(521)
    data[:2]=b'MZ'
    struct.pack_into('<I',data,60,64)
    data[64:68]=b'PE\0\0'
    struct.pack_into('<H',data,88,0x20b)
    data[500:500+len(marker)]=marker
    return bytes(data)

def certificate_shape(data):
    result=bytearray(data)
    checksum,security=auth.pe_fields(data)
    result.extend(b'\0'*(-len(result)%8))
    offset=len(result)
    # Intentionally not a real certificate. Tests only exercise payload changes.
    result.extend(struct.pack('<IHH',16,0x0200,0x0002)+b'TESTONLY')
    struct.pack_into('<I',result,checksum,1234)
    struct.pack_into('<II',result,security,offset,16)
    return bytes(result)

class PackagingTests(unittest.TestCase):
    def test_only_certificate_and_checksum_may_change(self):
        source=program(b'game')
        signed=certificate_shape(source)
        auth.same_program(source,signed)
        for position in (100,300,500):
            damaged=bytearray(signed)
            damaged[position]^=1
            with self.assertRaises(ValueError):
                auth.same_program(source,damaged)
        with self.assertRaises(ValueError):
            auth.same_program(source,source)

    def test_repack_preserves_dependencies_and_installer_alias(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)
            archive=root/'release.zip'
            originals={'BT4decomp/BT4decomp.exe':program(b'game'),
                'BT4decomp/BT4decomp-setup.exe':program(b'setup'),
                'BT4decomp/Install.exe':program(b'setup'),
                'BT4decomp/SDL3.dll':b'upstream library bytes',
                'BT4decomp/BT4decomp.exe.mem':b'original data'}
            with zipfile.ZipFile(archive,'w') as package:
                for name,value in originals.items():package.writestr(name,value)
            staging=root/'staging'
            auth.stage(archive,staging)
            for file in staging.iterdir():file.write_bytes(certificate_shape(file.read_bytes()))
            auth.replace(archive,staging)
            with zipfile.ZipFile(archive) as package:
                self.assertEqual(set(package.namelist()),set(originals))
                self.assertEqual(package.read('BT4decomp/Install.exe'),package.read('BT4decomp/BT4decomp-setup.exe'))
                for name in ('BT4decomp/SDL3.dll','BT4decomp/BT4decomp.exe.mem'):
                    self.assertEqual(package.read(name),originals[name])
            (staging/'unexpected.exe').write_bytes(b'not part of the build')
            with self.assertRaises(ValueError):auth.replace(archive,staging)

    def test_malformed_or_previously_signed_input_is_rejected(self):
        with self.assertRaises(ValueError):auth.pe_fields(b'not a PE executable')
        source=program(b'game')
        signed=certificate_shape(source)
        with self.assertRaises(ValueError):auth.same_program(signed,certificate_shape(signed))

if __name__=='__main__':unittest.main()
