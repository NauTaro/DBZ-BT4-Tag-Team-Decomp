"""Exercise the actual renderer icon generator and its alpha coverage."""
from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[3]
class RankedBallArtTests(unittest.TestCase):
    def test_all_seven_icons_have_smooth_unclipped_edges_and_distinct_stars(self):
        cc=shutil.which('gcc') or shutil.which('cc')
        if not cc:self.skipTest('C compiler required')
        code=r'''
#include <assert.h>
#include "ranked_ball_art.h"
int main(){uint64_t hashes[8]={0};
assert(!RankedBall_Pixels(-1)&&!RankedBall_Pixels(8));
for(int n=0;n<8;n++){
 uint32_t *p=RankedBall_Pixels(n);assert(p);int partial=0,opaque=0,red=0;
 for(int y=0;y<128;y++)for(int x=0;x<128;x++){
  uint32_t pixel=p[y*128+x],a=pixel>>24;
  assert(a<=128);if(a>0&&a<128)partial++;if(a==128)opaque++;
  if(!x||!y||x==127||y==127)assert(a==0);
  if(a==128&&(pixel&255)>100&&((pixel>>8)&255)<35)red++;
  hashes[n]=(hashes[n]^pixel)*1099511628211ull;
 }
 assert(partial>80&&opaque>8000);if(n)assert(red>10);
 for(int previous=0;previous<n;previous++)assert(hashes[n]!=hashes[previous]);
 free(p);
}}
'''
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp);src=p/'art.c';exe=p/('art.exe' if os.name=='nt' else 'art')
            src.write_text(code,encoding='utf-8');opts={'creationflags':0x08000000} if os.name=='nt' else {}
            result=subprocess.run((['rtk','proxy'] if shutil.which('rtk') else [])+[cc,'-O2','-I'+str(ROOT/'port/src/gs'),str(src),'-o',str(exe),'-lm'],capture_output=True,text=True,**opts)
            self.assertEqual(result.returncode,0,result.stderr)
            result=subprocess.run((['rtk','proxy'] if shutil.which('rtk') else [])+[str(exe)],capture_output=True,text=True,timeout=15,**opts)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
