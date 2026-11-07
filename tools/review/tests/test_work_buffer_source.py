"""Pending GPU writes own an effect buffer before its first submission."""
from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
from test_potara import function
ROOT=Path(__file__).resolve().parents[3]
class WorkBufferSourceTests(unittest.TestCase):
 def test_first_use_current_writes_upload_reuse_and_snapshots(self):
  cc=shutil.which('gcc') or shutil.which('cc')
  if not cc:self.skipTest('C compiler required')
  source=(ROOT/'port/src/gs/gs_draw.c').read_text(encoding='utf8')
  code=r'''
#include <assert.h>
#include <stdint.h>
static struct {unsigned fbp,gen,draws;int cleared;}gsTargets[2];
static unsigned gGsPageGen[512];
'''+function(source,'static int target_has_image(')+r'''
int main(void){
 uint64_t tex0=0x2a00;
 gsTargets[0].fbp=0x150;gsTargets[0].gen=5;gGsPageGen[0x150]=5;
 assert(!target_has_image(-1,0,tex0));assert(!target_has_image(0,0,tex0));
 /* The clear/copy has been recorded, but the backend has not run yet. */
 gsTargets[0].draws=1;assert(!gsTargets[0].cleared);assert(target_has_image(0,0,tex0));
 gsTargets[0].cleared=1;gsTargets[0].draws=0;assert(target_has_image(0,0,tex0));
 /* An actual upload reuses that address for a texture, not the GPU image. */
 ++gGsPageGen[0x150];assert(!target_has_image(0,0,tex0));
 gsTargets[0].gen=6;gsTargets[0].draws=1;assert(target_has_image(0,0,tex0));
 assert(!target_has_image(0,0,tex0+8));
 assert(target_has_image(0,1,tex0));
}
'''
  with tempfile.TemporaryDirectory() as tmp:
   p=Path(tmp);src=p/'probe.c';exe=p/('probe.exe' if os.name=='nt' else 'probe');src.write_text(code)
   flags={'creationflags':0x08000000} if os.name=='nt' else {}
   build=subprocess.run([cc,str(src),'-o',str(exe)],capture_output=True,text=True,**flags)
   self.assertEqual(build.returncode,0,build.stdout+build.stderr)
   subprocess.run([str(exe)],check=True,timeout=5,**flags)
