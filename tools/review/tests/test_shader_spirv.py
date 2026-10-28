import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
spec = importlib.util.spec_from_file_location('shader_spirv', ROOT / 'port/tools/shader_spirv.py')
shader_spirv = importlib.util.module_from_spec(spec)
spec.loader.exec_module(shader_spirv)


class ShaderCacheTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.cache = self.root / 'port/src/gs/generated/spirv'
        self.cache.mkdir(parents=True)
        self.gen = self.root / 'gen'
        self.gen.mkdir()
        self.source = self.root / 'test.vert'
        self.source.write_bytes(b'void main() {}\n')
        self.binary = b'\x03\x02\x23\x07' + bytes(12)
        (self.cache / 'test.vert.spv').write_bytes(self.binary)
        (self.cache / 'manifest.json').write_text(json.dumps({'test.vert': {
            'source_sha256': shader_spirv.source_hash(self.source.read_bytes()),
            'spv_sha256': hashlib.sha256(self.binary).hexdigest(),
        }}))

    def test_platform_line_endings_keep_verified_bytecode(self):
        self.source.write_bytes(b'void main() {}\r\n')
        self.assertTrue(shader_spirv.prepare(self.root, self.gen, self.source))
        self.assertEqual((self.gen / 'test.vert.spv').read_bytes(), self.binary)

    def test_changed_source_is_rejected_before_copy(self):
        self.source.write_bytes(b'void main() { changed(); }\n')
        with self.assertRaisesRegex(RuntimeError, 'source changed'):
            shader_spirv.prepare(self.root, self.gen, self.source)
        self.assertFalse((self.gen / 'test.vert.spv').exists())

    def test_corrupt_bytecode_is_rejected_before_copy(self):
        (self.cache / 'test.vert.spv').write_bytes(self.binary[:-1] + b'x')
        with self.assertRaisesRegex(RuntimeError, 'Invalid checked-in'):
            shader_spirv.prepare(self.root, self.gen, self.source)
        self.assertFalse((self.gen / 'test.vert.spv').exists())

    def test_unrecorded_shader_is_rejected(self):
        other = self.root / 'other.vert'
        other.write_bytes(self.source.read_bytes())
        with self.assertRaisesRegex(RuntimeError, 'source changed'):
            shader_spirv.prepare(self.root, self.gen, other)
