"""Online chase overrides retain the whole local view and advance once per blank."""
from pathlib import Path
import os, shutil, subprocess, tempfile, unittest
from test_potara import function

ROOT = Path(__file__).resolve().parents[3]

class OnlineChaseTests(unittest.TestCase):
    def test_full_screen_both_seats_and_presentation_reentry(self):
        cc = shutil.which('gcc') or shutil.which('cc')
        if not cc:
            self.skipTest('C compiler required')
        source = (ROOT/'src/battle/btl_cam.c').read_text(encoding='utf8')
        code = r'''
#include <assert.h>
#include <stdint.h>
typedef int32_t s32;
typedef uint32_t u32;
typedef struct {float x,y,z,w;} Vec4;
typedef struct {int index;Vec4 pos,rot;} BtlCamView;
static struct {int valid,side,obj,zoom;u32 blank;Vec4 pos,rot;} sPortCameraPose[2];
static int gPortResim;static u32 gPortLiveBlanks;
static int sPortZoomView[2],calls,collisions,natural;
static struct {int flags;} battle;
#define BATTLE_FLAG_PAUSE 1
static void *Battle_GetWork(void);
/* The extracted game code requires the actual flags field. */
#define Battle_GetWork() (&battle)
static void Port_NaturalFightCamera(s32 id,s32 pos,s32 rot){++natural;}
static void Port_ModsCamera(s32 side,s32 id,s32 pos,s32 rot,s32 paused){++calls;}
static s32 Port_ModsCamZoom(s32 side){return 800;}
static s32 Port_ModsCamCollide(s32 side){return 1;}
static void BtlCam_PortCollide(BtlCamView *v,s32 id){++collisions;v->pos.x+=1;}
static int selected,split=1,applied=-1;
static void BtlCam_SelectView(s32 side){selected=side;}
static void BtlCam_UpdateView(s32 side){assert(side==selected);split=1;applied=1;}
static void BtlCam_SetLayout(s32 value,s32 apply){split=value;if(apply)applied=value;}
static void BtlCam_ApplyView(s32 scissor){assert(scissor);applied=split;}
'''
        code += function(source, 'static void BtlCam_PortMods(') + '\n'
        code += function(source, 'static void BtlCam_PortChaseView(') + r'''
int main(void){
    for(int side=0;side<2;++side){
        split=1;BtlCam_PortChaseView(side);
        assert(selected==side && split==0 && applied==0);
        BtlCam_PortChaseView(side);assert(split==0 && applied==0);
    }
    BtlCamView a={0,{5,0,0,1},{0}},b={1,{10,0,0,1},{0}};
    BtlCam_PortMods(&a,0,5);assert(calls==1&&collisions==1&&a.pos.x==6);
    a.pos.x=5;BtlCam_PortMods(&a,0,5);
    assert(calls==1&&natural==1&&collisions==1&&a.pos.x==6&&sPortZoomView[0]==800);
    BtlCam_PortMods(&b,1,6);assert(calls==2&&collisions==2&&b.pos.x==11);
    /* Replayed blanks cannot move host smoothing or collision state. */
    gPortResim=1;a.pos.x=100;BtlCam_PortMods(&a,0,5);
    assert(calls==2&&collisions==2&&a.pos.x==100);
    gPortResim=0;++gPortLiveBlanks;a.pos.x=5;BtlCam_PortMods(&a,0,5);
    assert(calls==3&&collisions==3&&a.pos.x==6);
    /* Changing the camera actor is not mistaken for a duplicate callback. */
    BtlCam_PortMods(&a,0,9);assert(calls==4&&collisions==4&&a.pos.x==7);
}
'''
        with tempfile.TemporaryDirectory() as temp:
            p=Path(temp);src=p/'probe.c';exe=p/('probe.exe' if os.name=='nt' else 'probe')
            src.write_text(code)
            flags={'creationflags':0x08000000} if os.name=='nt' else {}
            build=subprocess.run([cc,'-w',str(src),'-o',str(exe)],capture_output=True,text=True,**flags)
            self.assertEqual(build.returncode,0,build.stdout+build.stderr)
            subprocess.run([str(exe)],check=True,timeout=5,**flags)
