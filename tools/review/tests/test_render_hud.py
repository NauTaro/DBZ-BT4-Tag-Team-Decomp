"""A HUD quad must never become a motion partner of an identical scene quad."""
from pathlib import Path
import os,re,shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[3]
class RenderHudTests(unittest.TestCase):
    def test_hud_is_excluded_from_scene_motion_pairs(self):
        cc=shutil.which('cc') or shutil.which('gcc')
        if not cc:self.skipTest('A C compiler is required')
        source=(ROOT/'port/src/gs/gs_draw.c').read_text(encoding='utf8')
        header=(ROOT/'port/src/gs/gs_draw.h').read_text(encoding='utf8')
        struct=lambda text,name:re.search(r'typedef\s+struct\s+'+name+r'\s*\{.*?\}\s*'+name+r';',text,re.S).group()
        routines=source[source.index('static int interp_prim_cmp('):source.index('/* Which kinds of piece stand still')]
        macro=re.search(r'^#define INTERP_SCISSOR_ID.*$',source,re.M).group()
        code='#include <stdint.h>\n#include <stdlib.h>\n#include <string.h>\n#include <assert.h>\ntypedef uintptr_t GsTex;\n'
        code+='\n'.join([struct(header,'Vtx'),struct(header,'GsTarget'),struct(header,'GsDraw'),struct(source,'InterpPrim')])
        code+='\n#define INTERP_MAX_PRIMS 32\n#define INTERP_PIECE_MAX 8\n'+macro+'\n'
        code+='static GsDraw gsDraws[2];static Vtx gsVerts[12];static GsTarget gsTargets[1];static unsigned gsDrawCount=2;static int gsTargetCount=1,gGsMainFbp=0;\n'
        code+='static char *test_no_environment(const char *name){(void)name;return NULL;}\n#define getenv test_no_environment\n'
        code+=routines+r'''
int main(void){
 InterpPrim out[32];unsigned i,h;
 const float xy[12]={0,0,1,0,0,1,0,1,1,0,1,1};
 for(i=0;i<12;++i){gsVerts[i].x=xy[(i%6)*2];gsVerts[i].y=xy[(i%6)*2+1];}
 gsDraws[0].count=gsDraws[1].count=6;gsDraws[1].first=6;
 gsDraws[0].tex=gsDraws[1].tex=123;gsDraws[0].pipeline=gsDraws[1].pipeline=4;
 assert(interp_prims(out)==2);
 for(h=1;h<=10;++h){
   gsDraws[0].hud=h;
   assert(interp_prims(out)==1);
   assert(out[0].first==6);
 }
 gsDraws[1].hud=2;assert(interp_prims(out)==0);
 return 0;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);c=root/'hud.c';exe=root/('hud.exe' if os.name=='nt' else 'hud');c.write_text(code,encoding='utf8')
            flags={'creationflags':0x08000000} if os.name=='nt' else {}
            p=subprocess.run([cc,'-std=c99',str(c),'-o',str(exe)],capture_output=True,text=True,timeout=30,**flags)
            self.assertEqual(p.returncode,0,p.stderr)
            p=subprocess.run([str(exe)],capture_output=True,text=True,timeout=15,**flags)
            self.assertEqual(p.returncode,0,p.stdout+p.stderr)
if __name__=='__main__':unittest.main()
