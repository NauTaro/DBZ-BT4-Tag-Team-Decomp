"""Exercise production anchors and outfit visibility across native selectors."""
from pathlib import Path
import os, shutil, subprocess, tempfile, unittest
ROOT=Path(__file__).resolve().parents[3]

class MenuComposites(unittest.TestCase):
    def run_c(self, source, extra=()):
        cc=shutil.which('cc') or shutil.which('gcc')
        if not cc:self.skipTest('A C compiler is required')
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory);c=p/'test.c';exe=p/('test.exe' if os.name=='nt' else 'test')
            c.write_text(source,encoding='utf8')
            flags={'creationflags':0x08000000} if os.name=='nt' else {}
            r=subprocess.run([cc,'-std=c99',str(c),*[str(x) for x in extra],'-o',str(exe)],capture_output=True,text=True,**flags)
            self.assertEqual(r.returncode,0,r.stderr)
            r=subprocess.run([str(exe)],capture_output=True,text=True,**flags)
            self.assertEqual(r.returncode,0,r.stdout+r.stderr)

    def test_a_long_row_and_its_window_do_not_split_at_the_screen_midpoint(self):
        self.run_c(r'''
#include <assert.h>
void Dma_AddData(void *p,int n){}
int Port_MenusWide(void){return 1;}
int Port_PageAspectMilli(void){return 2333;}
int Port_MenuPartNow(void),Port_MenuClipPart(const char *,int,int),Port_MenuMessageX(int);
void Port_MenuPage(int);
int main(void){
    Port_MenuPage(37);
    /* All native glyphs, including ones beyond x=250, inherit one anchor. */
    assert(Port_MenuPartNow()==4);
    assert(Port_MenuClipPart("mc_dammy_text_1",100,1)==-1);
    assert(Port_MenuClipPart("mc_item_details",250,1)==-1);
    assert(Port_MenuClipPart("mc_list_plate_1",100,1)==-1);
    Port_MenuPage(0);
    int screens[]={15,19,23,24,25};
    for(int i=0;i<5;i++){
        Port_MenuPage(screens[i]);
        assert(Port_MenuClipPart("mc_color_plate_1",100,1)==4);
        assert(Port_MenuClipPart("mc_color_name_text",100,1)==4);
        assert(Port_MenuClipPart("mc_yajirusi_left",88,1)==4);
        assert(Port_MenuClipPart("mc_yajirusi_right",372,1)==4);
        assert(Port_MenuClipPart("mc_yajirusi_up",239,1)==5);
        /* Native inset does not move the footer's text into its ornament. */
        assert(Port_MenuMessageX(0)==0);
        Port_MenuPage(0);
    }
}
''',[ROOT/'port/src/gs_marker.c'])

    def test_each_of_eight_costumes_has_one_visible_plate_and_no_stale_centering(self):
        for filename,who,selected in [('menu_i_d.c','gEntrySel','ES_CUR.costume'),('menu_l_b.c','gSoloSel','SS_CUR.costume'),('menu_m_b.c','gUbTeamSel','UT_CUR.color')]:
            s=(ROOT/'src/menu'/filename).read_text(encoding='utf8')
            end=s.index('\n    flash =',s.index('        /* costume plates */'))
            start=s.rfind('    for (i =',0,s.index('        /* costume plates */'))
            code=s[start:end].replace(who+'->colorCount','colors').replace(selected,'selected')
            self.run_c(r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int visible[8],offsets[8];
typedef struct {int id;} Ref;
static void Flash_FindLabel(void *f,const char *parent,const char *name,Ref *r){
    const char *n=parent?parent:name;r->id=atoi(n+15)-1;assert(r->id>=0&&r->id<8);
}
static void Flash_ClipSetFlags(void *f,Ref *r,int flags,int value){visible[r->id]=value;}
static void Flash_ClipSetOffset(void *f,Ref *r,int x,int y){assert(x==0&&y==0);offsets[r->id]++;}
static void Flash_ClipSetUv(void *f,Ref *r,void *uv){}
int main(void){
  for(int colors=1;colors<=8;colors++)for(int selected=0;selected<colors;selected++){
    int i;void *flash=0;Ref ref;char name[64];struct {int x0,y0,x1,y1;} uv;
    memset(visible,0,sizeof(visible));memset(offsets,0,sizeof(offsets));
'''+code+r'''
    int count=0;for(i=0;i<8;i++){count+=visible[i];if(i==selected)assert(offsets[i]==1);}
    assert(count==1&&visible[selected]);
  }
}
''')

if __name__=='__main__':unittest.main()
