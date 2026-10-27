"""Compile the actual resize transition without a GPU or game data."""
from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[3]
class RenderResolutionTests(unittest.TestCase):
    def test_resolution_retires_saved_pictures(self):
        cc=shutil.which('cc') or shutil.which('gcc')
        if not cc:self.skipTest('A C compiler is required')
        text=(ROOT/'port/src/gs/gs_draw.c').read_text(encoding='utf8')
        clear=text[text.index('static void interp_scale_changed(void) {'):text.index('static void interp_counts_restore(void) {')]
        resize=text[text.index('static int scale_apply(void) {'):text.index('/* The list of menu labels')]
        code='''#include <assert.h>
#include <stdio.h>
#include <string.h>
static int gsScale=2,gsPendingScale,gsTargetCount=7;
static int sBootQualityScale=-1,sBootQualityHD,sMenuHD=1;
static int sInterpNext=2,sInterpRepeat=1,sInterpPrevCount=12,sInterpPrevPrimCount=7,sInterpPrevVertCount=40;
static int sInterpSaved[16];
static int active=1,rebuilds;
static int Port_GameProfileMenuActive(void){return active;}
static void rebuilt(void){++rebuilds;}
static struct {void (*scaleChanged)(void);} backend={rebuilt},*sBackend=&backend;
'''+clear+resize+'''
static void remember(void){sInterpNext=3;sInterpRepeat=1;sInterpPrevCount=10;sInterpPrevPrimCount=20;sInterpPrevVertCount=40;memset(sInterpSaved,1,sizeof(sInterpSaved));}
static void retired(void){int i;assert(!sInterpNext&&!sInterpRepeat&&!sInterpPrevCount&&!sInterpPrevPrimCount&&!sInterpPrevVertCount);for(i=0;i<16;++i)assert(sInterpSaved[i]==0);}
int main(void){
 remember();assert(scale_apply()==1);assert(gsScale==3&&rebuilds==1&&gsTargetCount==0);retired();
 remember();assert(scale_apply()==0);assert(sInterpNext==3&&sInterpPrevCount==10&&rebuilds==1);
 active=0;assert(scale_apply()==1);assert(gsScale==2&&rebuilds==2&&sMenuHD==1&&sBootQualityScale==-1);retired();
 remember();gsPendingScale=4;assert(scale_apply()==1);assert(gsScale==4&&rebuilds==3);retired();
 return 0;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);source=root/'resize.c';exe=root/('resize.exe' if os.name=='nt' else 'resize')
            source.write_text(code,encoding='utf8');flags={'creationflags':0x08000000} if os.name=='nt' else {}
            p=subprocess.run([cc,'-std=c99',str(source),'-o',str(exe)],capture_output=True,text=True,timeout=30,**flags)
            self.assertEqual(p.returncode,0,p.stderr)
            p=subprocess.run([str(exe)],capture_output=True,text=True,timeout=15,**flags)
            self.assertEqual(p.returncode,0,p.stdout+p.stderr)
if __name__=='__main__':unittest.main()
