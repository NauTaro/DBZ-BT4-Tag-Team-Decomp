"""Saved camera distance is delivered as shared input, independently of focus mode."""
from pathlib import Path
import unittest
import test_r12_tag_crashes as probes
from test_tag_effect_lifecycle import function

class SavedCameraTests(unittest.TestCase):
    def test_production_camera_input_uses_each_actors_delivered_preference(self):
        code=r'''
#define BT4_TAG_TEAM_EXPANDED 1
#include "battle/btl_char_cam.h"
#include <assert.h>
#include <string.h>
static u8 sTagOnlinePads[10][18];static int network,focus,localDistance,pressed;
static unsigned char object[0xFC0];static Vec4 pose;
s32 TagBattle_DevCombatActive(void){return 1;}
s32 TagBattle_Network(void){return network;}
s32 TagBattle_CameraMode(s32 side){return focus;}
s32 BattleSide_GetOptionA(s32 side){return localDistance;}
s32 BtlInput_IsPressed(ChrCamChr *c,s32 b){return pressed;}
s32 BtlChar_TestFlag(ChrCamChr *c,s32 f){return 0;}
void ChrCam_UpdateLockOnYaw(ChrCamChr *c){}
void ChrCam_UpdateFreeYaw(ChrCamChr *c){}
s32 BtlAnim_GetId(ChrCamChr *c){return 0;}
void *BtlChar_GetObj(ChrCamChr *c){return object;}
void Vec4_Copy(Vec4 *a,Vec4 *b){*a=*b;}
'''+function('src/battle/tag_team_online.inc','TagBattle_NetCameraDistance')+'\n'+function('src/battle/btl_char_cam_modes.c','ChrCam_UpdateInput')+r'''
int main(void){
 *(Vec4 **)(object+0xFA0)=&pose;
 ChrCamChr c={0};
 for(int owner=0;owner<10;++owner)for(int chosen=0;chosen<3;++chosen){
  memset(sTagOnlinePads,0,sizeof(sTagOnlinePads));sTagOnlinePads[owner][10]=0x80|chosen;c.side=owner;
  for(localDistance=0;localDistance<3;++localDistance){
   network=1;focus=0;ChrCam_UpdateInput(&c);assert(c.cam.distMode==chosen);
   for(focus=1;focus<3;++focus){ChrCam_UpdateInput(&c);assert(c.cam.distMode==focus);}
   network=0;focus=0;ChrCam_UpdateInput(&c);assert(c.cam.distMode==localDistance);
  }
 }
 network=1;focus=0;c.side=0;
 for(int value=0;value<256;++value){sTagOnlinePads[0][10]=value;ChrCam_UpdateInput(&c);assert(c.cam.distMode==(value>=128&&value<=130?value-128:0));}
 assert(TagBattle_NetCameraDistance(-1)==0&&TagBattle_NetCameraDistance(10)==0);
}
'''
        probes.ReportedTagCrashes.native(self,code)

if __name__=='__main__':unittest.main()
