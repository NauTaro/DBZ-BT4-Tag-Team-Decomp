"""Whole portrait masks and bounded energy-light source sampling."""
from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
from test_potara import function
ROOT=Path(__file__).resolve().parents[3]
class PortraitEnergyTests(unittest.TestCase):
 def compile(self,code):
  cc=shutil.which('gcc') or shutil.which('cc')
  if not cc:self.skipTest('C compiler required')
  with tempfile.TemporaryDirectory() as tmp:
   p=Path(tmp);src=p/'probe.c';exe=p/('probe.exe' if os.name=='nt' else 'probe');src.write_text(code)
   flags={'creationflags':0x08000000} if os.name=='nt' else {}
   r=subprocess.run([cc,'-std=c99',str(src),'-o',str(exe)],capture_output=True,text=True,**flags)
   self.assertEqual(r.returncode,0,r.stdout+r.stderr)
   subprocess.run([str(exe)],check=True,**flags)
 def test_atlas_pieces_share_one_mask_without_touching_other_ui(self):
  header=(ROOT/'port/src/gs/gs_portrait_fade.h').as_posix()
  code='#include <assert.h>\n#include <string.h>\ntypedef struct {float x;}Vtx;typedef struct {int vu;unsigned first,count;float portrait[2];}GsDraw;\n#include "'+header+'"\n'+r'''
int main(void){
 Vtx v[10]={{100},{200},{150},{400},{600},{750},{650},{900},{0},{1000}};
 GsDraw d[5]={{0,0,2,{-1,0}},{0,2,2,{-1,0}},{0,4,2,{1,0}},{0,6,2,{1,0}},{0,8,2,{0,0}}};
 GsPortrait_Finalize(d,5,v,2);
 assert(d[0].portrait[0]==200&&d[0].portrait[1]>71.99f&&d[0].portrait[1]<72.01f);
 assert(!memcmp(d[0].portrait,d[1].portrait,sizeof(d[0].portrait)));
 assert(d[2].portrait[0]==1800&&d[2].portrait[1]<-71.99f&&d[2].portrait[1]>-72.01f);
 assert(!memcmp(d[2].portrait,d[3].portrait,sizeof(d[2].portrait)));
 assert(d[4].portrait[0]==0&&d[4].portrait[1]==0);
 /* Replaying a frame must not expand or move its mask. */
 float saved[2];memcpy(saved,d[0].portrait,sizeof(saved));GsPortrait_Finalize(d,5,v,2);
 assert(!memcmp(saved,d[0].portrait,sizeof(saved)));
}
'''
  self.compile(code)
 def test_native_gpu_grade_support_excludes_unimplemented_backends(self):
  source=(ROOT/'port/src/gs/gs_draw.c').read_text(encoding='utf8')
  code='#include <assert.h>\n#define _WIN32 1\nstatic int sVulkanBackend,sDirect3D12Backend,sDirect3D11Backend,sGlBackend,sDirect3D9Backend;static int *sBackend;\n'+function(source,'static int grade_supported(void) {')+r'''
int main(void){sBackend=&sVulkanBackend;assert(grade_supported());sBackend=&sDirect3D12Backend;assert(grade_supported());sBackend=&sDirect3D11Backend;assert(grade_supported());sBackend=&sGlBackend;assert(!grade_supported());sBackend=&sDirect3D9Backend;assert(!grade_supported());sBackend=0;assert(!grade_supported());}
'''
  self.compile(code)
 def test_hd_hue_reads_at_most_64_source_texels_without_decoding_an_image(self):
  source=(ROOT/'port/src/gs/gs_draw.c').read_text(encoding='utf8')
  code=r'''
#include <assert.h>
#include <stdint.h>
static unsigned reads,seen;static int transparent,indexed;
static uint32_t Gs_Expand(uint32_t c,uint32_t p){return c;}
static uint32_t Gs_VramRead(uint32_t bp,uint32_t bw,uint32_t p,uint32_t x,uint32_t y){
 ++reads;if(bp==200){assert(x<8&&y<2);seen|=1u<<(x+y*8);return transparent?0x00FF0000:0x80FF0000;}
 assert(x<4096&&y<4096);return indexed?x%16:transparent?0x00FF0000:0x80FF0000;
}
'''+function(source,'static void texture_light_average(uint32_t tbp,uint32_t tbw,uint32_t psm,uint32_t tw,uint32_t th,')+r'''
int main(void){
 float c[3];texture_light_average(100,1,0,4096,4096,200,0,32,c);
 assert(reads==64&&c[0]==0&&c[1]==0&&c[2]==1);
 reads=0;indexed=1;texture_light_average(100,1,0x14,16,16,200,0,4,c);
 assert(reads==128&&(seen&0xFF00)&&c[2]==1);
 reads=0;transparent=1;texture_light_average(100,1,0x14,16,16,200,0,4,c);
 assert(reads==128&&c[0]==1&&c[1]==1&&c[2]==1);
}
'''
  self.compile(code)
