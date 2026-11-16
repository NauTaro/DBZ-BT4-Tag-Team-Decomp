"""Local split-view requests cannot change controller or room ownership."""
from pathlib import Path
import os, shutil, subprocess, tempfile, unittest
from test_potara import function

ROOT=Path(__file__).resolve().parents[3]

class PresentationSplitTests(unittest.TestCase):
    def c(self,code):
        cc=shutil.which('gcc') or shutil.which('cc')
        if not cc:self.skipTest('C compiler required')
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp);source=path/'probe.c';exe=path/('probe.exe' if os.name=='nt' else 'probe')
            source.write_text(code,encoding='utf-8')
            opts={'creationflags':0x08000000} if os.name=='nt' else {}
            result=subprocess.run([cc,'-w',str(source),'-o',str(exe)],capture_output=True,text=True,**opts)
            self.assertEqual(result.returncode,0,result.stderr)
            result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=5,**opts)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)

    def test_local_online_target_ally_and_fused_spectator_views(self):
        source=(ROOT/'port/src/mods_game.c').read_text(encoding='utf-8')
        code=r'''
#include <assert.h>
#define BT4_TAG_TEAM_EXPANDED 1
static int sSplitDrawing,sSplitActors[2],tag,split,net=-1,human=1,own=6,target=9,mode=1,absent=-1,local=6;
int Port_NetView(){return net;}int Battle_GetHumanSide(){return human;}
int TagBattle_Active(){return tag;}int Battle_IsSplitScreen(){return split;}
int TagBattle_ViewActor(){return own;}int TagBattle_Count(){return 10;}
int TagBattle_Target(int a){assert(a==own||own==42);return target;}
int TagBattle_Allied(int a,int b){return a%2==b%2;}
int TagBattle_SplitViewActor(int p){return p?7:4;}
int TagBattle_LocalActor(){return local;}int TagFusion_Absent(int a){return a==absent;}
int BtlCharApi_GetHp(int a){return a==0?0:40000;}
int Mods_SplitScreen(){return mode;}
'''+function(source,'int Port_PresentationSplitActor(')+r'''
int main(){
assert(Port_PresentationSplitActor(0)==1&&Port_PresentationSplitActor(1)==0);
net=0;assert(Port_PresentationSplitActor(0)==0&&Port_PresentationSplitActor(1)==1);
net=1;assert(Port_PresentationSplitActor(0)==1&&Port_PresentationSplitActor(1)==0);
split=1;net=-1;assert(Port_PresentationSplitActor(0)==0&&Port_PresentationSplitActor(1)==1);
tag=1;assert(Port_PresentationSplitActor(0)==4&&Port_PresentationSplitActor(1)==7);
split=0;assert(Port_PresentationSplitActor(0)==6&&Port_PresentationSplitActor(1)==9);
mode=2;assert(Port_PresentationSplitActor(1)==2); /* live ally, excludes dead actor 0 */
absent=2;assert(Port_PresentationSplitActor(1)==4); /* excludes absorbed actor */
absent=local;assert(Port_PresentationSplitActor(1)==own); /* fusion spectator cannot watch others */
absent=-1;mode=1;target=-1;assert(Port_PresentationSplitActor(1)==own);
target=9;Port_PresentationSplitActor(0);Port_PresentationSplitActor(1);
sSplitDrawing=1;split=1;own=8;target=3;
assert(Port_PresentationSplitActor(0)==6&&Port_PresentationSplitActor(1)==9); /* scope cannot remap inputs */
return 0;}
'''
        self.c(code)

    def test_camera_and_projection_transaction_restores_native_data(self):
        source=(ROOT/'src/battle/btl_cam.c').read_text(encoding='utf-8')
        code=r'''
#include <assert.h>
#include <string.h>
typedef int s32;typedef struct{float m[16];}Mtx44;
typedef struct{float x,y,z,w;}Vec4;
typedef struct{int layout,index;Vec4 pos,rot;}BtlCamView;
typedef struct{BtlCamView views[2],*cur;}BtlCam;
typedef BtlCamView View;
static BtlCam work,*gBtlCam=&work;static View *gBtlCamView;
static s32 sPortZoomView[2]={800,900},sPortZoomOn[2]={1,1};
static Mtx44 sPortBase[2][2],sPortZoomed[2][2];
static struct{BtlCam camera;View*current;s32 zoom[2];Mtx44 base[2][2],zoomed[2][2];s32 zoomOn[2];s32 active;}sPresentationSplitCamera;
static int request=1,scope,calls;
#define VIEW_LAYOUT_LEFT 1
#define VIEW_LAYOUT_RIGHT 2
int Mods_SplitScreen(){return request;}
int Port_PresentationSplitActor(int pane){assert(!scope);return pane?9:6;}
int Port_PresentationSplitDrawing(int n){if(n>=0)scope=n;return scope;}
int BattleSide_GetObjId(int a){return 100+a;}
void BtlCam_SelectView(int p){gBtlCam->cur=&gBtlCam->views[p];gBtlCamView=gBtlCam->cur;}
void BtlCam_SetViewLayout(BtlCamView*v,int layout){v->layout=layout;}
void BtlCharApi_GetCamPose(int a,Vec4*p,Vec4*r){p->x=a;}
void BtlCam_PortMods(BtlCamView*v,int side,int id){assert(scope&&side<2);++calls;sPortZoomView[side]=750;}
void BtlCam_BuildView(BtlCamView*v,int apply){sPortZoomOn[v->index]=0;sPortBase[v->index][0].m[0]=22;}
'''+function(source,'s32 BtlCam_PresentationSplitBegin(')+function(source,'void BtlCam_PresentationSplitEnd(')+r'''
int main(){
work.views[0].index=0;work.views[1].index=1;work.cur=&work.views[0];gBtlCamView=work.cur;
BtlCam original=work;View*view=gBtlCamView;Mtx44 base[2][2];memcpy(base,sPortBase,sizeof(base));
request=0;assert(!BtlCam_PresentationSplitBegin()&&!scope&&!calls);
request=1;assert(BtlCam_PresentationSplitBegin()&&scope&&calls==2);
assert(work.views[0].layout==1&&work.views[1].layout==2&&work.views[0].pos.x==106&&work.views[1].pos.x==109);
assert(!BtlCam_PresentationSplitBegin());
BtlCam_PresentationSplitEnd();assert(!scope&&!memcmp(&work,&original,sizeof(work))&&gBtlCamView==view);
assert(sPortZoomView[0]==800&&sPortZoomView[1]==900&&sPortZoomOn[0]==1&&!memcmp(base,sPortBase,sizeof(base)));
BtlCam_PresentationSplitEnd();return 0;}
'''
        self.c(code)
