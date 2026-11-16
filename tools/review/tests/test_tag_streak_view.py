"""Camera-relative cinematic streaks must not cover unrelated Tag players."""
from pathlib import Path
import unittest
import test_r12_tag_crashes as probes
from test_tag_effect_lifecycle import function

class StreakViewTests(unittest.TestCase):
    def test_real_draw_filters_the_owner_but_preserves_positioned_world_effects(self):
        code=r'''
#include "battle/eft_t.h"
#include <assert.h>
#include <string.h>
static EftTCamView view;EftTCamView *gBtlCamView=&view;
static EftStreakMgr mgr;static EftStreakRoot root={&mgr};EftStreakRoot *gEftStreak=&root;
static int tag=1,shown,draws;
s32 TagBattle_Active(void){return tag;}
s32 BtlScene_IsCharInView(s32 id){return id==shown;}
void Mtx_StoreIdentity(Mtx44 *m){memset(m,0,sizeof(*m));for(int i=0;i<4;++i)m->m[i][i]=1;}
void Mtx_InverseRT(Mtx44 *a,Mtx44 *b){Mtx_StoreIdentity(a);}
void Mtx_MulVec4(Vec4 *a,Mtx44 *m,Vec4 *b){*a=*b;}
void Mtx_Mul(Mtx44 *a,Mtx44 *b,Mtx44 *c){Mtx_StoreIdentity(a);}
void Mtx_Translate(Mtx44 *a,Mtx44 *b,Vec4 *v){}
void Mtx_RotateZ(Mtx44 *a,Mtx44 *b,f32 v){}
void Vec4_Set(Vec4 *v,f32 x,f32 y,f32 z,f32 w){*v=(Vec4){x,y,z,w};}
void Vec4_Copy(Vec4 *a,Vec4 *b){*a=*b;}
void Vec3_Normalize(Vec4 *a,Vec4 *b){*a=*b;}
void Vec3_Sub(Vec4 *a,Vec4 *b,Vec4 *c){*a=(Vec4){0};}
void Vec3_Scale(Vec4 *a,Vec4 *b,f32 t){*a=*b;}
void Vec3_ScaleAdd(Vec4 *a,Vec4 *b,Vec4 *c,f32 t){*a=*c;}
f32 Vec3_Length(Vec4 *a){return 1;}
void BtlCharApi_GetNodePos(s32 a,s32 b,Vec4 *v){*v=(Vec4){0};}
f32 BtlCharApi_GetHeight(s32 a){return 2;}
s32 Mtx_ProjectPoint(EftTIVec *o,Mtx44 *m,Vec4 *v){o->x=o->y=0;o->z=1;return 1;}
void Vu0Cur_Push(void){++draws;}
void Vu0Cur_LoadMtx(Mtx44 *m){}
void Vu0Cur_Pop(void){}
void EftStreak_DrawWorld(Vec4 *v,EftTVec a,EftTVec b,EftTVec c,s32 d,s32 e,s32 f,EftStreakMgr *m){}
void EftStreak_DrawScreen(Vec4 *v,EftTVec c,s32 b,s32 n,s32 z,u64 tex){}
'''+function('src/battle/eft_t_b.c','EftStreak_InView')+'\n'+function('src/battle/eft_t_b.c','EftStreak_Draw')+r'''
int main(void){
 EftStreakDef def={0};EftStreakWork w={0};EftTTask t={0};w.def=&def;t.work=&w;
 for(shown=0;shown<10;++shown)for(int owner=0;owner<10;++owner){
  w.objId=owner;draws=0;w.flags=EFT_STREAK_SCREEN;EftStreak_Draw(&t);assert(draws==(shown==owner));
  draws=0;w.flags=EFT_STREAK_WORLD;EftStreak_Draw(&t);assert(draws==(shown==owner));
  draws=0;w.flags=EFT_STREAK_WORLD|EFT_STREAK_POS_SET;EftStreak_Draw(&t);assert(draws==1);
 }
 tag=0;shown=1;w.objId=0;w.flags=EFT_STREAK_SCREEN;draws=0;EftStreak_Draw(&t);assert(draws==1);
}
'''
        probes.ReportedTagCrashes.native(self,code)

if __name__=='__main__':unittest.main()
