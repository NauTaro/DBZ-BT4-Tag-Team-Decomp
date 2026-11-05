from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
from test_potara import function
ROOT=Path(__file__).resolve().parents[3]

class PauseShaderTests(unittest.TestCase):
    def run_c(self,code):
        cc=os.environ.get('CC') or shutil.which('gcc') or shutil.which('cc')
        if not cc:self.skipTest('A C compiler is required')
        with tempfile.TemporaryDirectory() as tmp:
            d=Path(tmp);(d/'probe.c').write_text(code,encoding='utf8');exe=d/('probe.exe' if os.name=='nt' else 'probe')
            subprocess.run([cc,'-std=gnu99','-I'+str(ROOT/'port/src/gs'),str(d/'probe.c'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)

    def test_rim_uses_exact_model_transform_and_preserves_its_shader(self):
        self.run_c(r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "gs_draw.h"
#include "gs_rim_interp.h"
int main(void){
 Vu0Uniform real[6]={0},blend[6]={0};uint16_t kind[]={65,70,65,70,65,70};
 uint32_t obj[]={10,10,11,11,12,12},view[]={1,1,1,2,1,1};
 for(int i=0;i<6;i++){
  real[i].boneA[0]=real[i].boneA[5]=real[i].boneA[10]=real[i].boneA[15]=1;
  real[i].misc[3]=kind[i]&63;real[i].light2[3]=1.4f;blend[i]=real[i];
 }
 // Intermediate model geometry uses a rigidly rotated bone; the independently
 // blended rim has the old linear transform. The next pair belongs to a
 // different view, and the final pair has genuinely different source bones.
 blend[0].boneA[0]=0.7071f;blend[0].boneA[1]=0.7071f;blend[0].light[0]=0.5f;
 real[5].boneA[12]=3;blend[1].color0[0]=71;blend[1].target[0]=2;
 Vu0Uniform rim=blend[1],different_view=blend[3],different_mesh=blend[5];
 assert(GsRim_Align(blend,real,kind,obj,view,6)==1);
 assert(!memcmp(blend[0].boneA,blend[1].boneA,60*sizeof(float)));
 assert(!memcmp(blend[1].color0,rim.color0,sizeof(Vu0Uniform)-60*sizeof(float)));
 assert(!memcmp(&blend[3],&different_view,sizeof(Vu0Uniform)));
 assert(!memcmp(&blend[5],&different_mesh,sizeof(Vu0Uniform)));
 assert(!GsRim_Align(blend,real,kind,obj,view,0));
 obj[1]=99;assert(!GsRim_Align(blend,real,kind,obj,view,2));
 puts("Rim interpolation: exact model depth/pose/light, shader values, view/actor/mesh isolation PASS");
}
''')

    def test_pause_flag_ownership_does_not_leak_into_vblank(self):
        source=(ROOT/'src/battle/dev_rules.inc').read_text(encoding='utf8')
        self.run_c(r'''
#include <assert.h>
#include <stdio.h>
typedef int s32;enum{BATTLE_FLAG_PAUSE=0x100};static struct{unsigned flags;}work;
#define Battle_GetWork() (&work)
'''+function(source,'static s32 DevPause_Begin(')+function(source,'static void DevPause_End(')+r'''
int main(void){
 for(unsigned f=0;f<1024;f++)for(int paused=0;paused<2;paused++){
  work.flags=f;int own=DevPause_Begin(paused);
  assert(work.flags==(paused?(f|BATTLE_FLAG_PAUSE):f));
  DevPause_End(own);assert(work.flags==f);
 }
 work.flags=0;assert(DevPause_Begin(1));DevPause_End(1);assert(!work.flags);
 // A subsequent resumed or single-step frame must see an unpaused baseline.
 assert(!DevPause_Begin(0));assert(!work.flags);
 puts("Dev pause: original pause preserved, resumed/step frames and snapshot boundary clean PASS");
}
''')

if __name__=='__main__':unittest.main()
