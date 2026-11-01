from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[3]

class MenuPanelTests(unittest.TestCase):
    def test_full_coverage_without_stretching_caps_or_sampling_outside_texture(self):
        cc=shutil.which('cc') or shutil.which('gcc')
        if not cc:self.skipTest('A C compiler is required')
        code=r'''
#include <assert.h>
#include <math.h>
#include <stdint.h>
typedef struct {float x,y,z,s,t,q;uint8_t r,g,b,a;} Vtx;
#include "menu_panel_layout.h"
int main(void){
    Vtx source[6]={{0,0,0,0,0,1,80,80,80,128},{512,0,0,1,0,1,80,80,80,128},
        {0,80,0,0,1,1,80,80,80,128},{512,0,0,1,0,1,80,80,80,128},
        {512,80,0,1,1,1,80,80,80,128},{0,80,0,0,1,1,80,80,80,128}};
    float scales[]={1,.75f,4.0f/7.0f,.375f,4.0f/30.0f};
    for(unsigned k=0;k<sizeof(scales)/sizeof(scales[0]);k++){
        Vtx out[768];unsigned n=menu_panel_expand(out,source,6,scales[k]);float area=0;
        assert(n>0&&n<768&&n%3==0);
        for(unsigned i=0;i<n;i++){
            assert(isfinite(out[i].x)&&isfinite(out[i].s));
            assert(out[i].x>=-.001f&&out[i].x<=512.001f);
            assert(out[i].s>=-.001f&&out[i].s<=1.001f&&out[i].t>=-.001f&&out[i].t<=1.001f);
            assert(out[i].q==1&&out[i].a==128);
            if(out[i].x<=96*scales[k]+.001f)assert(fabsf(out[i].x/scales[k]-out[i].s*512)<.01f);
            /* The repeated body's final UV is independent of the cap's first
             * UV at their shared geometric seam. Check cap interiors. */
            if(out[i].x>512-96*scales[k]+.001f)assert(fabsf((512-out[i].x)/scales[k]-(1-out[i].s)*512)<.01f);
        }
        for(unsigned i=0;i<n;i+=3){Vtx a=out[i],b=out[i+1],c=out[i+2];area+=fabsf((b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x))*.5f;}
        assert(fabsf(area-512*80)<.2f);
    }
}
'''
        flags={'creationflags':0x08000000} if os.name=='nt' else {}
        with tempfile.TemporaryDirectory() as directory:
            folder=Path(directory);src=folder/'panel.c';exe=folder/('panel.exe' if os.name=='nt' else 'panel');src.write_text(code)
            p=subprocess.run([cc,'-std=c99','-I'+str(ROOT/'port/src/gs'),str(src),'-lm','-o',str(exe)],capture_output=True,text=True,**flags)
            self.assertEqual(p.returncode,0,p.stderr)
            p=subprocess.run([str(exe)],capture_output=True,text=True,**flags)
            self.assertEqual(p.returncode,0,p.stdout+p.stderr)

if __name__=='__main__':unittest.main()
