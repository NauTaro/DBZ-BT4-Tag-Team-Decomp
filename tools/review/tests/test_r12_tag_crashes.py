"""Native regressions for the two fault paths from the R1.1 crash reports."""
from pathlib import Path
import os, shutil, subprocess, tempfile, unittest
from test_tag_effect_lifecycle import function

ROOT=Path(__file__).resolve().parents[3]

class ReportedTagCrashes(unittest.TestCase):
    def native(self,code):
        cc=shutil.which('gcc') or shutil.which('cc')
        if not cc:self.skipTest('C compiler required')
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp);src=p/'case.c';exe=p/('case.exe' if os.name=='nt' else 'case')
            src.write_text(code,encoding='utf-8')
            flags={'creationflags':0x08000000} if os.name=='nt' else {}
            prefix=['rtk','proxy'] if shutil.which('rtk') else []
            for args in [[cc,'-std=gnu99','-DPORT','-I'+str(ROOT/'include'),str(src),'-o',str(exe)],[str(exe)]]:
                run=subprocess.run(prefix+args,capture_output=True,text=True,timeout=30,**flags)
                self.assertEqual(run.returncode,0,run.stdout+run.stderr)

    def test_every_effect_kind_keeps_all_ten_actor_slots_separate(self):
        source=(ROOT/'src/battle/eft_aa.c').read_text()
        begin=source.index('static s32 EftCharSlot_Valid(')
        end=source.index('/* ---- blade trail',begin)
        code=r'''
#include <assert.h>
#include <string.h>
#include "battle/eft_aa.h"
#include "battle/eft_r.h"
static struct {EftCharSlots slots;unsigned canary;} memory;
EftCharSlots *gEftCharSlot=&memory.slots;
''' + source[begin:end] + r'''
typedef struct EftCharaFxWork {unsigned char pad[0x830];unsigned flags;} EftCharaFxWork;
''' + function('src/battle/eft_r.c','EftCharaFx_Stop') + r'''
int main(void){
  EftRTask tasks[5][10]={0};EftCharaFxWork work[10]={0};
  void (*put[5])(s32,void *)={EftCharSlot_Set0,EftCharSlot_Set1,EftCharSlot_Set2,EftCharSlot_Set3,EftCharSlot_SetAbsorb};
  void *(*get[5])(s32)={EftCharSlot_Get0,EftCharSlot_Get1,EftCharSlot_Get2,EftCharSlot_Get3,EftCharSlot_GetAbsorb};
  void (*clear[5])(s32)={EftCharSlot_Clear0,EftCharSlot_Clear1,EftCharSlot_Clear2,EftCharSlot_Clear3,EftCharSlot_ClearAbsorb};
  memory.canary=0xdecafbad;
  for(int k=0;k<5;++k)for(int a=0;a<10;++a)put[k](a,&tasks[k][a]);
  for(int k=0;k<5;++k)for(int a=0;a<10;++a)assert(get[k](a)==&tasks[k][a]);
  for(int a=0;a<10;++a){work[a].flags=1;tasks[3][a].work=&work[a];assert(EftCharaFx_Stop(a));assert(work[a].flags==3);}
  tasks[3][6].work=0;assert(!EftCharaFx_Stop(6));
  for(int k=0;k<5;++k){
    put[k](-1,&tasks[k][0]);put[k](10,&tasks[k][0]);clear[k](-1);clear[k](10);
    assert(!get[k](-1)&&!get[k](10));
    for(int a=0;a<10;++a){clear[k](a);assert(!get[k](a));}
  }
  assert(memory.canary==0xdecafbad);gEftCharSlot=0;
  for(int k=0;k<5;++k){put[k](0,0);clear[k](0);assert(!get[k](0));}
  assert(!EftCharaFx_Stop(0));
}
'''
        self.native(code)

    def test_simultaneous_draft_can_confirm_any_seat_before_the_first(self):
        code=r'''
#include <assert.h>
#include <string.h>
typedef int s32;
typedef struct {void *tex;} MTexRes;
typedef struct {int chara;} Member;
typedef struct {int memberCount,cost;Member member[5];} Side;
static struct {int battleType;Side *side[2];void *teamTex[2][8];int *chipPack;} screen;
static __typeof__(screen) *gTeamSel=&screen;
static MTexRes resources[6];static int looked[6];
static void *pack_at(int *pack,int entry){assert(entry>0&&entry<=5);++looked[entry];return &resources[entry];}
#define MPACK_AT(pack,entry) pack_at(pack,entry)
static int ChrTbl_GetCost(int id){assert(id>=0&&id<5);return id+1;}
''' + function('src/menu/menu_e_b.c','TeamSel_SetTeamTex') + r'''
int main(void){
  Side sides[2]={0};int slots[]={1,4,5,6,7};
  for(int i=1;i<=5;++i)resources[i].tex=&resources[i];
  for(int mask=0;mask<32;++mask)for(int team=0;team<2;++team){
    screen.battleType=2;screen.side[team]=&sides[team];Side *s=&sides[team];s->memberCount=0;
    int cost=0;
    for(int i=0;i<5;++i){s->member[i].chara=mask&(1<<i)?i:-1;if(s->member[i].chara>=0){s->memberCount=i+1;cost+=i+1;}}
    TeamSel_SetTeamTex(team);assert(s->cost==cost);
    for(int i=0;i<5;++i)assert(screen.teamTex[team][slots[i]]==(mask&(1<<i)?resources[i+1].tex:0));
  }
}
'''
        self.native(code)

if __name__=='__main__':unittest.main()
