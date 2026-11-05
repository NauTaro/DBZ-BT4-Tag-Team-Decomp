"""Exercise the production body transaction and sequential model-load protocol."""
from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[3]

class BodyChangeTests(unittest.TestCase):
    def test_two_bodies_health_queue_backpressure_and_reset(self):
        cc=os.environ.get('CC') or shutil.which('cc') or shutil.which('gcc')
        if not cc:self.skipTest('A C compiler is required')
        code=r'''
#define PORT 1
#include "bt4_body_change.c"
#include <assert.h>
#include <stdio.h>
static BtlMgrChr fighters[10];static BtlMgrObj objects[10];
static int count=10,allowQueue=1,loadActor=-1,requests,done,requested[10];
s32 BtlChar_GetCount(void){return count;}
BtlMgrChr *BtlChar_Get(s32 i){assert(i>=0&&i<count);return &fighters[i];}
s32 BtlAct_GetCurrent(BtlMgrChr *p){return p->action;}
BtlMgrMember *BtlMember_GetActive(BtlMgrChr *p){return &p->members[p->curMember];}
void BtlAct_Request(BtlMgrChr *p,s32 id){p->action=id;}
void BtlAct_SetFormCurrent(BtlMgrChr *p){(void)p;}
void BtlChar_SavePlacement(BtlMgrChr *p){(void)p;}
void BtlMember_ClearQueuedDamage(BtlMgrChr *p){(void)p;}
void BtlChar_SetFlag(BtlMgrChr *p,s32 flag){(void)p;(void)flag;}
void BtlChar_ClearFlag(BtlMgrChr *p,s32 flag){(void)p;(void)flag;}
s32 BtlChange_TryRequestChara(s32 actor,s32 chara,s32 costume,s32 variant,s32 anim,s32 unk,s32 voice){
    assert(actor>=0&&actor<count&&chara==sBody[actor].chara&&costume==sBody[actor].costume);
    assert(variant==sBody[actor].variant&&anim==-1&&unk==-1&&voice==-1);
    if(!allowQueue)return 0;++requests;++requested[actor];return 1;
}
s32 BtlChange_IsLoadedFor(s32 actor){return actor==loadActor;}
void BtlChange_SetReady(s32 actor){assert(actor==loadActor);}
void BtlChange_SetDone(s32 actor){assert(actor==loadActor);++done;loadActor=-1;}
void BtlAnim_Request(BtlMgrChr *p,s32 anim,f32 frame){(void)p;assert(anim==0&&frame==0);}
void BtlObj_BindCommonTables(BtlMgrObj *o){(void)o;}
BtlMgrObj *BtlChar_GetObj(BtlMgrChr *p){return &objects[p->side];}
void BtlMember_LoadParams(BtlMgrChr *p,s32 index,s32 a,f32 b,s32 c){
    BtlMgrGauge *g=&BtlMember_GetActive(p)->gauge;
    assert(index==p->curMember&&!a&&b==0&&!c);
    /* Model loading normally resets these. The exchange must restore HP. */
    g->healthMax=g->health=99999;g->kiMax=100;g->blastMax=200;
}
static void finish_load(int actor){
    loadActor=actor;Bt4BodyChange_Action(&fighters[actor],1);
    assert(sBody[actor].state==BODY_READY);
    assert(Bt4BodyChange_OnModelLoaded(&fighters[actor]));
    Bt4BodyChange_Action(&fighters[actor],1);assert(sBody[actor].state==BODY_DONE);
}
static void leave_pair(int a,int b){
    Bt4BodyChange_Action(&fighters[a],1);assert(fighters[a].action==0xB);
    Bt4BodyChange_Action(&fighters[a],3);
    Bt4BodyChange_Action(&fighters[b],1);assert(fighters[b].action==0xB);
    Bt4BodyChange_Action(&fighters[b],3);Bt4BodyChange_Update();
    assert(!Bt4BodyChange_Active(a)&&!Bt4BodyChange_Active(b));
}
int main(void){
    for(int i=0;i<10;++i){
        BtlMgrMember *m;fighters[i].side=i;fighters[i].pad=9-i;fighters[i].curMember=i%2;
        m=BtlMember_GetActive(&fighters[i]);m->chara=100+i;m->costume=i;
        m->gauge.health=1000+i*3000;m->gauge.healthMax=20000+i*10000;
        m->gauge.ki=40+i*30;m->gauge.blast=80+i*90;m->gauge.unk20=i%2;
    }
    Bt4BodyChange_Reset();BtlMgrChr untouched=fighters[3];
    BtlMgrMember originalA=*BtlMember_GetActive(&fighters[0]),originalB=*BtlMember_GetActive(&fighters[1]);
    fighters[0].action=0x12F;fighters[1].action=0x13B;
    Bt4BodyChange_Queue(-1,1);Bt4BodyChange_Queue(0,10);Bt4BodyChange_Queue(0,0);
    assert(!Bt4BodyChange_Active(0));
    Bt4BodyChange_Queue(0,1);Bt4BodyChange_Update();assert(!requests);
    fighters[0].action=0xB;fighters[1].action=0xD0;Bt4BodyChange_Update();
    assert(fighters[0].action==0x103&&fighters[1].action==0x103);
    allowQueue=0;Bt4BodyChange_Action(&fighters[0],0);Bt4BodyChange_Action(&fighters[0],1);
    assert(!requests&&sBody[0].state==BODY_REQUEST);
    assert(!memcmp(&originalA,BtlMember_GetActive(&fighters[0]),sizeof(originalA)));
    allowQueue=1;Bt4BodyChange_Action(&fighters[0],1);Bt4BodyChange_Action(&fighters[1],0);Bt4BodyChange_Action(&fighters[1],1);
    assert(requests==2&&requested[0]==1&&requested[1]==1);
    finish_load(0);Bt4BodyChange_Action(&fighters[0],1);assert(fighters[0].action==0x103);
    finish_load(1);leave_pair(0,1);
    BtlMgrMember *a=BtlMember_GetActive(&fighters[0]),*b=BtlMember_GetActive(&fighters[1]);
    assert(a->chara==originalB.chara&&a->costume==originalB.costume&&a->gauge.unk20==originalB.gauge.unk20);
    assert(b->chara==originalA.chara&&b->costume==originalA.costume&&b->gauge.unk20==originalA.gauge.unk20);
    assert(a->gauge.health==originalB.gauge.health&&a->gauge.healthMax==originalB.gauge.healthMax);
    assert(b->gauge.health==originalA.gauge.health&&b->gauge.healthMax==originalA.gauge.healthMax);
    assert(a->gauge.ki==originalA.gauge.ki&&b->gauge.ki==originalB.gauge.ki);
    assert(fighters[0].pad==9&&fighters[1].pad==8&&!memcmp(&untouched,&fighters[3],sizeof(untouched)));
    /* Reverse casting order and non-adjacent actors must not use actor XOR 1. */
    Bt4BodyChange_Queue(9,2);Bt4BodyChange_Update();
    Bt4BodyChange_Action(&fighters[9],1);Bt4BodyChange_Action(&fighters[2],1);
    finish_load(2);finish_load(9);leave_pair(9,2);
    assert(BtlMember_GetActive(&fighters[9])->chara==102&&BtlMember_GetActive(&fighters[2])->chara==109);
    /* No revival if the victim dies before the exchange begins. */
    Bt4BodyChange_Queue(0,1);b->gauge.health=0;Bt4BodyChange_Update();
    assert(!Bt4BodyChange_Active(0)&&!Bt4BodyChange_Active(1)&&!b->gauge.health);
    Bt4BodyChange_Queue(4,5);Bt4BodyChange_Reset();
    for(int i=0;i<10;++i)assert(!Bt4BodyChange_Active(i));
    puts("Body change: both bodies and exact HP, non-adjacent actors, saturated queue, no revival, controls and reset PASS");
}
'''
        with tempfile.TemporaryDirectory() as temp:
            d=Path(temp);(d/'probe.c').write_text(code)
            exe=d/('probe.exe' if os.name=='nt' else 'probe')
            subprocess.run([cc,'-std=gnu99','-O2','-I'+str(ROOT/'include'),'-I'+str(ROOT/'port/include'),'-I'+str(ROOT/'src/battle'),str(d/'probe.c'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)

if __name__=='__main__':unittest.main()
