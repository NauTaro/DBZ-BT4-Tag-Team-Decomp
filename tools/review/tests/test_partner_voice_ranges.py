"""Native partner animation-event lines use the BT4 character voice archives."""
from pathlib import Path
import unittest
import test_r12_tag_crashes as probes
from test_tag_effect_lifecycle import function
ROOT=Path(__file__).resolve().parents[3]

class PartnerVoiceTests(unittest.TestCase):
    def test_both_voice_sets_and_every_character(self):
        source=function('src/battle/btl_char_fx_c.c','BtlPartner_HandleEvents')
        body=source[source.index('#ifdef PORT\n    /* BT4'):]
        code=r'''
#include "sys/adx.h"
#include <assert.h>
#include <stdint.h>
typedef struct {int flags;} Save;
static Save save;static Save *gSaveData=&save;
static int current,cls,request[4],count;static uint64_t events;
typedef struct {int chara;} Object;
static int BtlChar_TestFlag(void *chr,int flag){return 0;}
static int BtlAct_GetCurrentClass(void *chr){return cls;}
static int BtlObjAnim_TestEvent(Object *o,uint64_t mask){return (events&mask)!=0;}
static int *BtlMember_GetActive(void *chr){return &current;}
static void BtlObj_SetSubState(Object *o,int a,int b){}
static void BtlCharSnd_RequestAt(void *p,int kind,int id,float near,float far){assert(kind==7);request[count++]=id;}
'''+function('src/sys/adx.c','Voice_CharaFile')+r'''
static void play(Object *obj){void *chr=0;int pos=0,sound,kind,mask,on;
'''+body+r'''
int main(void){
 for(int lang=0;lang<2;++lang)for(current=0;current<250;++current)for(cls=2;cls<=4;++cls){
  save.flags=lang;count=0;events=0x8000ULL|0x8000000000ULL;Object obj={current};play(&obj);
  int base=lang?VOICE_CHARA_BASE_ALT:VOICE_CHARA_BASE,line=0x57+(cls-2)*2;
  assert(count==2&&request[0]==base+current*100+line&&request[1]==request[0]+1);
 }
}
'''
        probes.ReportedTagCrashes.native(self,code)

if __name__=='__main__':unittest.main()
