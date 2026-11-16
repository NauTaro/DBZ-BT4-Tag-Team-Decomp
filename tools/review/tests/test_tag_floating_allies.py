"""Shared world-space ally health, bounded to the viewer and uncluttered panes."""
from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
from test_potara import function

ROOT=Path(__file__).resolve().parents[3]
class FloatingAlliesTests(unittest.TestCase):
    def run_c(self,code):
        cc=shutil.which('gcc') or shutil.which('cc')
        if not cc:self.skipTest('C compiler required')
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp);src=p/'test.c';exe=p/('test.exe' if os.name=='nt' else 'test')
            src.write_text(code,encoding='utf-8')
            opts={'creationflags':0x08000000} if os.name=='nt' else {}
            result=subprocess.run([cc,'-w','-I'+str(ROOT/'include'),str(src),'-o',str(exe),'-lm'],capture_output=True,text=True,**opts)
            self.assertEqual(result.returncode,0,result.stderr)
            result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=5,**opts)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)

    def test_viewport_bounds_overlap_and_ui_safe_space(self):
        self.run_c(r'''
#include <assert.h>
#include <math.h>
#include "battle/tag_team_hud_math.h"
int main(){float out[2],placed[8]={100,120,160,180,200,220,40,250};
assert(ThAllyPlate(100,144,0,256,placed,0,out)&&out[0]==100&&out[1]==120);
assert(!ThAllyPlate(0,125,0,256,placed,0,out));
assert(!ThAllyPlate(255,125,0,256,placed,0,out));
assert(!ThAllyPlate(100,50,0,256,placed,0,out));
assert(!ThAllyPlate(100,400,0,256,placed,0,out));
assert(!ThAllyPlate(NAN,125,0,256,placed,0,out));
assert(!ThAllyPlate(100,144,0,256,placed,1,out));
assert(ThAllyPlate(169,144,0,256,placed,1,out));
assert(!ThAllyPlate(300,125,0,256,placed,0,out));
assert(ThAllyPlate(300,125,256,512,placed,0,out));
assert(!ThAllyPlate(100,125,0,256,placed,4,out));
/* Widescreen's outer page changes must preserve the centre divider. */
assert(ThAllyPlate(-50,125,-85,256,placed,0,out));
assert(!ThAllyPlate(250,125,-85,256,placed,0,out));
}
''')

    def test_only_local_allies_with_live_visible_anchors_are_drawn(self):
        source=(ROOT/'src/battle/tag_team_hud.inc').read_text(encoding='utf-8')
        self.run_c(r'''
#include <assert.h>
#include "battle/tag_team_hud_math.h"
typedef int s32;typedef float f32;typedef unsigned u32;
static int mode=1,fills,feeds,drawn[10],lastActor,hidden=3,absorbed=2,dead=4;
float Ps_Left(){return 0;}int Port_TagOfflineMode(){return mode;}
int TagBattle_ViewActor(){return 0;}
int TagBattle_Allied(int a,int b){return a%2==b%2;}
int TagFusion_Absent(int a){return a==absorbed;}
int BtlCharApi_GetHp(int a){return a==dead?0:30000;}
int TagBattle_HudAllyProject(int pane,int actor,float *out){
    lastActor=actor;if(actor==hidden)return 0;
    out[0]=(pane?256:0)+30+actor*20;out[1]=100+actor*24;return 1;
}
void Port_TagAllyHudFeed(int pane,int viewer,int actor,int x,int y){assert(actor==lastActor&&pane>=0&&pane<2&&x>0&&x<100000&&y>0&&y<100000);++feeds;}
void TagHud_NativeGauge(int actor,float x,float y,float scale,int small,int mirror){assert(actor==lastActor&&scale==.28f&&small&&!mirror);++fills;++drawn[lastActor];}
void Port_GsMarker(int marker){assert(((unsigned)marker>>28)==5&&(((unsigned)marker>>24)&15)==lastActor);}
'''+function(source,'static void TagHud_FloatingAllies(')+r'''
int main(){
TagHud_FloatingAllies(0,0,10);
assert(fills==2&&feeds==2&&drawn[6]==1&&drawn[8]==1);
/* The enemy's camera does not turn the enemy team into your allies. */
TagHud_FloatingAllies(1,1,10);
assert(fills==4&&drawn[6]==2&&drawn[8]==2&&!drawn[1]&&!drawn[3]&&!drawn[5]);
/* In real local two-player mode each player sees their own team. */
mode=2;TagHud_FloatingAllies(1,1,10);
assert(fills==7&&drawn[5]==1&&drawn[7]==1&&drawn[9]==1&&!drawn[3]);
/* An ally already represented by this viewport's main HUD is omitted. */
mode=1;TagHud_FloatingAllies(1,6,10);assert(fills==8&&drawn[6]==2&&drawn[8]==3);
}
''')

    def test_both_camera_matrices_behind_camera_and_occlusion(self):
        source=(ROOT/'src/battle/btl_cam.c').read_text(encoding='utf-8')
        self.run_c(r'''
#include <assert.h>
#include <string.h>
#include "battle/tag_team_hud_math.h"
typedef int s32;typedef float f32;typedef struct{float x,y,z,w;}Vec4;
typedef struct{float world2screen[16],nearZ;Vec4 pos;}View;
static struct{struct{View view;}views[2];}work,*gBtlCam=&work;
static Vec4 fighter={0,0,100,1};static float wall=1;static int rays;
int TagBattle_Count(){return 10;}int BattleSide_GetObjId(int a){return a;}
void BtlCharApi_GetBasePos(int a,Vec4*out){*out=fighter;}
float BtlCharApi_GetHeight(int a){return 20;}
float SpecCamera_StageRay(const float *from,const float*to){++rays;assert(to[1]==-21);return wall;}
'''+function(source,'s32 TagBattle_HudAllyProject(')+r'''
int main(){float out[3];
for(int pane=0;pane<2;++pane){
    View *v=&work.views[pane].view;v->nearZ=.3f;
    v->world2screen[0]=100;v->world2screen[5]=100;
    v->world2screen[8]=1792+(pane?384:128);v->world2screen[9]=2048;v->world2screen[11]=1;
    assert(TagBattle_HudAllyProject(pane,2,out));assert(out[0]==(pane?384:128)&&out[1]==203);
}
wall=.5f;assert(!TagBattle_HudAllyProject(0,2,out));
fighter.z=-100;int before=rays;assert(!TagBattle_HudAllyProject(0,2,out)&&rays==before);
assert(!TagBattle_HudAllyProject(-1,2,out)&&!TagBattle_HudAllyProject(2,2,out));
assert(!TagBattle_HudAllyProject(0,10,out));gBtlCam=0;assert(!TagBattle_HudAllyProject(0,2,out));
}
''')

    def test_lua_anchor_feed_is_bounded_and_reset_between_scene_draws(self):
        source=(ROOT/'port/src/fight_panels.c').read_text(encoding='utf-8')
        self.run_c(r'''
#include <assert.h>
#include <string.h>
typedef struct{int actor,x,y;}TagHudFloating;
static struct { int count[2],viewer[2];TagHudFloating item[2][4]; } sFloatingAllies;
'''+function(source,'void Port_TagAllyHudReset(')+function(source,'void Port_TagAllyHudFeed(')+r'''
int main(){
Port_TagAllyHudReset();
Port_TagAllyHudFeed(-1,0,1,100,100);Port_TagAllyHudFeed(2,0,1,100,100);
Port_TagAllyHudFeed(0,0,10,100,100);Port_TagAllyHudFeed(0,0,1,-1,100);
Port_TagAllyHudFeed(0,0,1,100,100001);assert(!sFloatingAllies.count[0]&&!sFloatingAllies.count[1]);
for(int n=0;n<10;++n)Port_TagAllyHudFeed(0,3,n,40000,50000);
assert(sFloatingAllies.count[0]==4&&sFloatingAllies.viewer[0]==3&&!sFloatingAllies.count[1]);
assert(sFloatingAllies.item[0][3].actor==3&&sFloatingAllies.item[0][0].x==40000);
Port_TagAllyHudFeed(1,6,7,70000,50000);assert(sFloatingAllies.count[1]==1&&sFloatingAllies.viewer[1]==6);
Port_TagAllyHudReset();assert(!sFloatingAllies.count[0]&&!sFloatingAllies.count[1]);
assert(sFloatingAllies.item[0][0].actor==0);
}
''')

    def test_render_list_anchors_replace_old_positions_and_expire_each_picture(self):
        header=(ROOT/'port/src/gs/tag_hud_anchor.h').as_posix()
        self.run_c(r'''
#include <assert.h>
#include <math.h>
'''+ '#include "'+header+'"\n'+r'''
int main(){ThaState state={0};float x,y;
assert(!ThaGet(&state,0,2,&x,&y));
assert(!ThaMarker(&state,0x40000000u));
ThaMarker(&state,0x50000000u|(2u<<24)|(900u<<11)|800u);
assert(ThaGet(&state,0,2,&x,&y)&&fabsf(x-900/4095.0f)<1e-6f);
/* A HUD built with the previous position follows the current picture. */
ThaMarker(&state,0x50000000u|(2u<<24)|(1200u<<11)|900u);
assert(ThaGet(&state,0,2,&x,&y)&&fabsf(x-1200/4095.0f)<1e-6f&&fabsf(y-900/2047.0f)<1e-6f);
ThaMarker(&state,0x50000000u|(8u<<24)|(1u<<23)|(3000u<<11)|1000u);
assert(ThaGet(&state,1,8,&x,&y)&&!ThaGet(&state,0,8,&x,&y));
assert(!ThaGet(&state,-1,2,&x,&y)&&!ThaGet(&state,2,2,&x,&y)&&!ThaGet(&state,0,10,&x,&y));
ThaMarker(&state,0x5f000000u); /* malformed actor is ignored */
ThaMarker(&state,0x60000000u);
assert(!ThaGet(&state,0,2,&x,&y)&&!ThaGet(&state,1,8,&x,&y));
}
''')
