"""Display buffer rotation preserves the camera; other targets remain isolated."""
from pathlib import Path
import os,re,shutil,subprocess,tempfile,unittest
from test_potara import function
ROOT=Path(__file__).resolve().parents[3]
class CameraTargetTests(unittest.TestCase):
 def test_display_buffer_rotation_and_offscreen_isolation(self):
  cc=shutil.which('gcc') or shutil.which('cc')
  if not cc:self.skipTest('C compiler required')
  source=(ROOT/'port/src/gs/gs_draw.c').read_text(encoding='utf8')
  macro=re.search(r'^#define INTERP_SCISSOR_ID\(d\).+$',source,re.M)[0]
  code='#include <stdint.h>\n#include <assert.h>\ntypedef struct {int scissor[4],target;} GsDraw;\nstatic int gsTargetCount=4,gGsMainFbp;static struct {uint32_t fbp;} gsTargets[4];\n'+function(source,'static uint32_t interp_target_role(const GsDraw *d) {')+'\n'+macro+r'''
int main(void){
 GsDraw d={{0,0,512,448},0};
 gsTargets[0].fbp=0;gsTargets[1].fbp=224;gsTargets[2].fbp=448;gsTargets[3].fbp=672;
 gGsMainFbp=0;uint32_t main=INTERP_SCISSOR_ID(&d);
 d.target=1;gGsMainFbp=224;assert(INTERP_SCISSOR_ID(&d)==main);
 d.target=2;uint32_t offscreen=INTERP_SCISSOR_ID(&d);assert(offscreen!=main);
 d.target=3;assert(INTERP_SCISSOR_ID(&d)!=offscreen&&INTERP_SCISSOR_ID(&d)!=main);
 d.target=1;d.scissor[2]=256;assert(INTERP_SCISSOR_ID(&d)!=main);
 d.scissor[0]=256;assert(INTERP_SCISSOR_ID(&d)!=main);
 return 0;
}
'''
  with tempfile.TemporaryDirectory() as tmp:
   d=Path(tmp);src=d/'probe.c';exe=d/('probe.exe' if os.name=='nt' else 'probe');src.write_text(code)
   kwargs={'creationflags':0x08000000} if os.name=='nt' else {}
   subprocess.run([cc,str(src),'-o',str(exe)],check=True,**kwargs)
   subprocess.run([str(exe)],check=True,**kwargs)
