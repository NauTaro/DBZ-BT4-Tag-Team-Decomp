from pathlib import Path
import os,shutil,subprocess,unittest
ROOT=Path(__file__).resolve().parents[3]
class TagNetworkKernelTests(unittest.TestCase):
    def test_missing_input_centres_stick_and_preserves_other_clients(self):
        node=shutil.which('node')
        if not node:self.skipTest('Node.js is required for the bundled WebAssembly kernel')
        flags={'creationflags':0x08000000} if os.name=='nt' else {}
        p=subprocess.run([node,str(Path(__file__).parent/'fixtures/tag_network_input.mjs'),str(ROOT/'cloudflare/kernel.wasm')],capture_output=True,text=True,timeout=30,**flags)
        self.assertEqual(p.returncode,0,p.stdout+p.stderr)
if __name__=='__main__':unittest.main()
