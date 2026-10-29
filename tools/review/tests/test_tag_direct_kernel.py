"""Compare the real native authority kernel with the shipped WASM implementation."""
from pathlib import Path
import importlib.util,shutil,tempfile,unittest
ROOT=Path(__file__).resolve().parents[3]

class TagDirectKernelTests(unittest.TestCase):
    def test_native_and_wasm_authority_frames_are_identical(self):
        if not (shutil.which('cc') or shutil.which('gcc')):self.skipTest('A C compiler is required')
        if not shutil.which('node'):self.skipTest('Node.js is required')
        source=ROOT/'port/tests/tag_direct_kernel/run.py'
        spec=importlib.util.spec_from_file_location('tag_kernel_parity',source)
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        with tempfile.TemporaryDirectory(prefix='bt4-kernel-parity-') as directory:
            result=module.run(Path(directory),2000)
        self.assertTrue(result['passed'])
        self.assertEqual(len(result['scenarios']),7)
        self.assertTrue(all(row['frames']==2000 and row['byte_parity'] for row in result['scenarios']))

if __name__=='__main__':unittest.main()
