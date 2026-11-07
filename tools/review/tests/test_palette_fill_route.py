"""Replay the actual GS sprite routing branch with aliased glow pages."""
from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[3]
class PaletteFillRouteTests(unittest.TestCase):
 def test_uploaded_glow_pages_still_use_gpu_but_palette_fills_remain_cpu(self):
  cc=shutil.which('gcc') or shutil.which('cc')
  if not cc:self.skipTest('C compiler required')
  source=(ROOT/'port/src/gs/gs_core.c').read_text(encoding='utf8')
  start=source.index('            uint32_t fb =',source.index('switch (type)'))
  end=source.index('            gs.vcount = 0;',start)
  branch=source[start:end]
  code=r'''
#include <assert.h>
#include <stdint.h>
static struct {uint64_t frame[2],prim;int vtx[2];}gs;
static unsigned gGsPageGen[512];static int sGpu=1,cpu,gpu;
static void draw_sprite(int sf,int ctx,const int*a,const int*b){++cpu;}
static void GsGpu_Draw(int kind,int ctx,const int*v){++gpu;}
static void route(int kick,int ctx){int sf=0;
'''+branch+r'''
}
int main(void){
 /* Character-palette colour/alpha fills still update CPU GS memory. */
 gs.frame[0]=(1ull<<16)|0x1e0;gGsPageGen[0x1e0]=7;gs.prim=0x46;
 route(1,0);assert(cpu==1&&gpu==0&&gGsPageGen[0x1e0]==8);
 /* The stage/character upload previously touched both glow work pages.
    Their textured blur copies must reach the GPU, including context 2. */
 for(int ctx=0;ctx<2;++ctx){
  unsigned fb=ctx?0x1f8:0x1f6;gGsPageGen[fb]=42;
  gs.frame[ctx]=(1ull<<16)|fb;gs.prim=ctx?0x356:0x156;
  route(1,ctx);assert(gGsPageGen[fb]==42);
 }
 assert(cpu==1&&gpu==2);
 route(0,0);assert(cpu==1&&gpu==2);
 /* An ordinary wide render target and a never-uploaded narrow target. */
 gs.prim=0x46;gs.frame[0]=(8ull<<16);route(1,0);
 gs.frame[0]=(1ull<<16)|0x1c0;route(1,0);assert(gpu==4);
}
'''
  with tempfile.TemporaryDirectory() as tmp:
   p=Path(tmp);src=p/'route.c';exe=p/('route.exe' if os.name=='nt' else 'route');src.write_text(code)
   flags={'creationflags':0x08000000} if os.name=='nt' else {}
   r=subprocess.run([cc,str(src),'-o',str(exe)],capture_output=True,text=True,**flags)
   self.assertEqual(r.returncode,0,r.stdout+r.stderr)
   subprocess.run([str(exe)],check=True,timeout=5,**flags)
