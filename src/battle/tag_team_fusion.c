/* GPL-3.0-or-later. Cooperative fusion for local and shared private-room play. Native form parameters,
 * animation and streaming are retained. Ten fixed records hold member state,
 * not copies of fighter models. Online consent uses delivered shared inputs. */
#ifdef BT4_TAG_TEAM_EXPANDED
#include "common.h"
#include "battle/btl_char_mgr.h"
#include "battle/tag_team_native.h"
#include "battle/tag_team_fusion.h"
#include "battle/tag_team_revive.h"
#include "battle/tag_team_assist.h"
#include "sys/pad.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

enum { F_IDLE,F_INVITE,F_PARTNER,F_NATIVE,F_FUSED,F_SEPARATE,F_LOADING,F_READY,F_LOADED,F_DONE };
enum { F_INVITE_TICKS = 90, F_CPU_INTERVAL = 30, F_CPU_RETRY = 360, F_CPU_AFTER_FUSION = 1350 };
/* Shared native BTLB values; BtlMgrChr intentionally keeps its input opaque. */
enum {F_INPUT_BLAST=4,F_INPUT_RUSH=8,F_INPUT_CIRCLE=0x08000000,
      F_INPUT_CROSS=0x10000000,F_INPUT_TRIANGLE=0x20000000,F_INPUT_SQUARE=0x40000000};
typedef struct TagFusionSlot {
    s32 state,peer,index,ticks,total,press,consume,originalCount,originalActive,result;
    BtlMgrMember original,shadow;
    s32 health,maximum,ki,blast,pad,inject,controller,accepted,startup;
} TagFusionSlot;
static TagFusionSlot sFusion[10];
static u32 sFusionClock, sCpuNextInvite[10];
static s32 sFusionEnabled,sFusionDuration;
static void fusion_test_pads(void),fusion_test_after(void);
static s32 fusion_test_allows_cpu(void);
static void fusion_restore(s32 a);
extern BtlMgrChr *BtlChar_Get(s32);
extern BtlMgrMember *BtlMember_GetActive(BtlMgrChr *);
extern s32 BtlAct_CanFuse(BtlMgrChr *,s32,s32,s32,s32 *),BtlAct_QueueFusion(BtlMgrChr *,s32,s32);
extern s32 BtlAct_GetCurrent(BtlMgrChr *),BtlAct_GetRequested(BtlMgrChr *),BtlAct_GetQueued(BtlMgrChr *);
extern s32 BtlChar_IsFree(BtlMgrChr *),BtlChar_IsFrozen(BtlMgrChr *),BtlChar_TestFlag(BtlMgrChr *,s32);
extern void BtlChar_SetFlag(BtlMgrChr *,s32),BtlChar_ClearFlag(BtlMgrChr *,s32);
extern void BtlChar_SetHeldFlag(BtlMgrChr *,s32),BtlAct_Request(BtlMgrChr *,s32),BtlAct_ClearQueue(BtlMgrChr *);
extern void BtlChar_SavePlacement(BtlMgrChr *),BtlMember_ClearQueuedDamage(BtlMgrChr *),BtlAct_SetFormCurrent(BtlMgrChr *);
extern s32 BtlChange_TryRequestChara(s32,s32,s32,s32,s32,s32,s32),BtlChange_IsLoadedFor(s32);
extern void BtlChange_SetReady(s32),BtlChange_SetDone(s32),BtlAnim_Request(BtlMgrChr *,s32,f32);
extern void BtlMember_LoadParams(BtlMgrChr *,s32,s32,f32,s32);
extern BtlMgrObj *BtlChar_GetObj(BtlMgrChr *);
extern void BtlObj_BindCommonTables(BtlMgrObj *);
extern s32 BtlSeq_IsFighting(void),BattleSide_GetControl(s32),BattleSide_GetPad(s32),BattleSide_GetObjId(s32);
extern void BtlCharApi_GetBasePos(s32,f32 *);
extern s32 Port_Setting(const char *,s32),BtlParam_GetUnkB1(BtlMgrChr *,s32),BtlParam_GetUnkB4(BtlMgrChr *,s32);
extern s32 BtlParam_GetCostAE(BtlMgrChr *,s32);
extern s32 BtlCharApi_GetHp(s32);
extern s32 BtlGame_IsPaused(void);
/* Access the same pause flags as the battle update without another partial
 * declaration of BattleWork. The helper is implemented in battle.c. */
extern s32 TagFusion_Ticking(void);
extern u32 BtlChar_Rand(void);
extern void BtlInput_BuildMaskTable(BtlMgrChr *);
extern s32 BtlInput_TestAction(BtlMgrChr *,s32,s32),BtlInput_IsPressed(BtlMgrChr *,u32);

static s32 fusion_valid(s32 a){return a>=0&&a<TagBattle_Count()&&a<10;}
static s32 fusion_clamp(s32 n,s32 max){return n<0?0:n>max?max:n;}
s32 TagFusion_Local(void){return sFusionEnabled&&TagBattle_Active()&&TagBattle_Local()&&!TagBattle_FreeForAll();}
s32 TagFusion_Busy(s32 a){return TagFusion_Local()&&fusion_valid(a)&&sFusion[a].state!=F_IDLE;}
s32 TagFusion_Absent(s32 a){return TagFusion_Local()&&fusion_valid(a)&&sFusion[a].state==F_PARTNER;}
s32 TagFusion_ViewActor(s32 a){return TagFusion_Absent(a)?sFusion[a].peer:a;}
s32 TagFusion_Pad(s32 a,s32 fallback){return TagFusion_Busy(a)&&(sFusion[a].state>=F_FUSED||TagFusion_Absent(a))?BtlChar_Get(a)->pad:fallback;}
s32 TagFusion_Controller(s32 a){
    a=TagFusion_ViewActor(a);
    return TagFusion_Busy(a) && sFusion[a].state >= F_NATIVE ? sFusion[a].controller : a;
}
s32 TagFusion_InputActor(s32 a){
    if(!TagFusion_Busy(a))return a;
    if(sFusion[a].state==F_FUSED)return sFusion[a].controller;
    if(sFusion[a].state==F_PARTNER){s32 peer=sFusion[a].peer;
        if(fusion_valid(peer)&&sFusion[peer].state==F_FUSED)return sFusion[peer].controller==a?peer:a;}
    return a;
}
s32 TagFusion_CPUWaiting(s32 actor){
    return TagFusion_Busy(actor) && sFusion[actor].state==F_INVITE;
}
static s32 fusion_clash_partner(s32 a){
    if(!TagFusion_Busy(a)||sFusion[a].state!=F_FUSED)return -1;
    s32 b=sFusion[a].peer;
    return fusion_valid(b) && BattleSide_GetControl(a)==BTL_CONTROL_PAD &&
        BattleSide_GetControl(b)==BTL_CONTROL_PAD ? b : -1;
}
static u32 fusion_other_pressed(s32 a,s32 b){
    if(TagBattle_Network()){
        extern u32 TagBattle_SourcePressed(s32);
        return TagBattle_SourcePressed(sFusion[a].controller==a?b:a);
    }
    BtlMgrChr *other=BtlChar_Get(b);Pad *pad=&gPad[other->pad];
    extern u32 BtlInput_GetKeyMask(BtlMgrChr*,s32);
    u32 raw=pad->pressed;
    return (raw&BtlInput_GetKeyMask(other,0)?F_INPUT_RUSH:0)|
           (raw&BtlInput_GetKeyMask(other,3)?F_INPUT_BLAST:0)|
           (raw&PAD_CIRCLE?F_INPUT_CIRCLE:0)|(raw&PAD_CROSS?F_INPUT_CROSS:0)|
           (raw&PAD_TRIANGLE?F_INPUT_TRIANGLE:0)|(raw&PAD_SQUARE?F_INPUT_SQUARE:0);
}
s32 TagFusion_ClashExtra(s32 a,s32 input){
    s32 b=fusion_clash_partner(a),action;
    if(b<0)return 0;action=BtlAct_GetCurrent(BtlChar_Get(a));
    if(!((action>=0x130&&action<=0x132)||action==0xFA))return 0;
    return (fusion_other_pressed(a,b)&(input==0x50?F_INPUT_BLAST:F_INPUT_RUSH))!=0;
}
s32 TagFusion_ClashChoice(s32 a,s32 pick,s32 previous){
    s32 b=fusion_clash_partner(a);u32 expected=0x8000000u<<pick,all=0x78000000;
    if(b<0||BtlAct_GetCurrent(BtlChar_Get(a))!=0xFC)return 0;
    u32 pressed=fusion_other_pressed(a,b);
    if(!(pressed&all))return 0;
    if((pressed&expected)&&!(pressed&(all^expected)))return 1;
    return previous<0||!(pressed&(0x8000000u<<previous))?-1:0;
}
s32 TagFusion_Separating(s32 a){return TagFusion_Busy(a)&&sFusion[a].state>=F_SEPARATE;}
void TagFusion_Reset(void){
    memset(sFusion,0,sizeof(sFusion));
    memset(sCpuNextInvite,0,sizeof(sCpuNextInvite));
    sFusionClock=0;
    sFusionEnabled=Port_TagOption("tag_off_fusion",1)!=0;
    sFusionDuration=Port_TagOption("tag_fusion_seconds",150);
    if(sFusionDuration<10||sFusionDuration>600)sFusionDuration=150;
}
void TagFusion_End(void){memset(sFusion,0,sizeof(sFusion));}
static s32 fusion_idle(s32 a){
    BtlMgrChr *c; s32 action,request;
    if(!fusion_valid(a)||TagBattle_IsActorLoading(a)||TagBattle_Pair(a)>=0||TagRevive_Busy(a)||TagAssist_Busy(a))return 0;
    c=BtlChar_Get(a);if(!c||BtlMember_GetActive(c)->gauge.health<=0||!BtlChar_IsFree(c)||BtlChar_IsFrozen(c))return 0;
    action=BtlAct_GetCurrent(c);request=BtlAct_GetRequested(c);
    if(!((action>=0xB&&action<=0x1A)||action==0x37))return 0;
    return BtlAct_GetQueued(c)<0&&(request<0||request==action||request==0xB);
}
static f32 fusion_distance(s32 a,s32 b){
    f32 p[4],q[4],x,y,z;BtlCharApi_GetBasePos(BattleSide_GetObjId(a),p);BtlCharApi_GetBasePos(BattleSide_GetObjId(b),q);
    x=p[0]-q[0];y=p[1]-q[1];z=p[2]-q[2];return x*x+y*y+z*z;
}
/* The native validator sees a temporary roster alias. No gauge, equipment or
 * present bit survives this read-only check. A separate ally is never made
 * selectable as a reserve or switched into the initiator's body. */
static s32 fusion_compatible(s32 a,s32 b,s32 index){
    BtlMgrChr *c=BtlChar_Get(a),*other=BtlChar_Get(b);BtlMgrMember saved;
    s32 count=c->memberCount,member=-1,result;
    if(index<0||index>2||a==b||!TagBattle_Allied(a,b)||c->curMember!=0||other->curMember!=0||count!=1||other->memberCount!=1)return 0;
    if((BtlParam_GetUnkB1(c,index)!=1&&BtlParam_GetUnkB1(c,index)!=2)||BtlParam_GetUnkB4(c,index)==BtlMember_GetActive(c)->chara)return 0;
    if(BtlMember_GetActive(c)->gauge.unk34||BtlMember_GetActive(other)->gauge.unk34)return 0;
    /* Mixed frozen/mortal components need individual damage allocation, not
     * an implicit invulnerable fusion. Do not silently change either rule. */
    {const TagHealthRule *x=TagBattle_HealthRule(a),*y=TagBattle_HealthRule(b);
     if((x&&x->frozen)!=(y&&y->frozen))return 0;}
    saved=c->members[1];c->members[1]=*BtlMember_GetActive(other);c->memberCount=2;
    result=BtlAct_CanFuse(c,index,1,!BtlChar_TestFlag(c,0x10F),&member)&&member==1;
    c->members[1]=saved;c->memberCount=count;return result;
}
static void fusion_cancel(s32 a){
    s32 b=sFusion[a].peer;
    memset(&sFusion[a],0,sizeof(sFusion[a]));
    if(fusion_valid(b)&&sFusion[b].peer==a&&sFusion[b].state==F_INVITE)memset(&sFusion[b],0,sizeof(sFusion[b]));
}
static void fusion_invite(s32 actor,s32 partner,s32 index){
    sFusion[actor].state=F_INVITE;
    sFusion[actor].peer=partner;
    sFusion[actor].index=index;
    sFusion[actor].ticks=F_INVITE_TICKS;
    sFusion[partner].state=F_INVITE;
    sFusion[partner].peer=actor;
    sFusion[partner].index=-1;
    if(getenv("BT4_RENDER_TEST"))fprintf(stderr,"tag fusion: invited actors=%d,%d index=%d\n",actor,partner,index);
}
/* Called by the native directional R3 gesture. Its caller must not dequeue a
 * native fusion until consent has been obtained on the next simulation tick. */
s32 TagFusion_Request(s32 actor,s32 index){
    s32 i,best=-1;f32 nearest=120.0f*120.0f;
    if(!TagFusion_Local()||!TagFusion_Ticking()||!fusion_idle(actor)||sFusion[actor].state)return 0;
    for(i=0;i<TagBattle_Count();++i){f32 d;
        if(i==actor||sFusion[i].state||!TagBattle_Allied(actor,i))continue;
        d=fusion_distance(actor,i);
        if(d<nearest&&fusion_idle(i)&&fusion_compatible(actor,i,index)){nearest=d;best=i;}
    }
    if(best>=0)fusion_invite(actor,best,index);
    return 0;
}

static s32 fusion_cpu_safe(s32 actor){
    for(s32 enemy=0;enemy<TagBattle_Count();++enemy){
        if(!TagBattle_Allied(actor,enemy) && !TagBattle_IsActorLoading(enemy) && BtlCharApi_GetHp(enemy)>0 &&
           fusion_distance(actor,enemy)<150.f*150.f)return 0;
    }
    return 1;
}

static void fusion_cpu_try(s32 actor){
    s32 best=-1, bestIndex=-1;
    f32 nearest=120.f*120.f;
    if(sFusion[actor].state || sCpuNextInvite[actor]>sFusionClock ||
       !fusion_idle(actor) || !fusion_cpu_safe(actor))return;
    for(s32 ally=0;ally<TagBattle_Count();++ally){
        if(ally==actor || sFusion[ally].state || sCpuNextInvite[ally]>sFusionClock ||
           !TagBattle_Allied(actor,ally))continue;
        f32 distance=fusion_distance(actor,ally);
        if(distance>=nearest || !fusion_idle(ally) || !fusion_cpu_safe(ally))continue;
        /* Prefer a temporary fusion when the same pair can perform both. */
        s32 index=-1;
        for(s32 i=0;i<3;++i){
            s32 kind=BtlParam_GetUnkB1(BtlChar_Get(actor),i);
            if((index<0 || kind==1) && fusion_compatible(actor,ally,i)){
                index=i;
                if(kind==1)break;
            }
        }
        if(index>=0){best=ally;bestIndex=index;nearest=distance;}
    }
    if(best>=0){
        sCpuNextInvite[actor]=sCpuNextInvite[best]=sFusionClock+F_CPU_RETRY;
        fusion_invite(actor,best,bestIndex);
    }
}

void TagFusion_ReadPads(void){
    s32 a;fusion_test_pads();if(!TagFusion_Local()||!TagFusion_Ticking())return;
    for(a=0;a<TagBattle_Count();++a){
        TagFusionSlot *s=&sFusion[a];s->press=0;
        if(s->state!=F_INVITE && s->state!=F_FUSED && !s->consume)continue;
        s32 n=BattleSide_GetPad(a), owner=s->state==F_FUSED?s->controller:a;
        if(BattleSide_GetControl(owner)!=BTL_CONTROL_PAD || (!TagBattle_Network()&&(n<0||n>=PAD_COUNT)))continue;
        Pad *p=TagBattle_SourceCommandPad(owner);if(!(p->held&PAD_R3))s->consume=0;
        if((p->pressed&PAD_R3)&&s->state==F_INVITE&&s->index<0)s->press=s->consume=1;
        if((p->pressed&PAD_R3)&&(p->held&PAD_L3)&&s->state==F_FUSED&&s->total&&fusion_idle(a))s->press=s->consume=2;
        if(s->consume){TagBattle_ConsumeCommand(a,s->consume==2?PAD_R3|PAD_L3:PAD_R3);u32 mask=s->consume==2?PAD_R3|PAD_L3:PAD_R3,gameMask=s->consume==2?PADG_R3|PADG_L3:PADG_R3;
            p->held&=~mask;p->pressed&=~mask;p->repeat&=~mask;
            p->gameHeld&=~gameMask;p->gamePressed&=~gameMask;p->gameRepeat&=~gameMask;}
    }
}
static void fusion_hold_partner(s32 a){
    BtlMgrChr *c=BtlChar_Get(a);s32 bit;
    c->freeze=2;c->freezeNext=c->freezeDelay=0;
    BtlMember_GetActive(c)->gauge.health=0;
    BtlChar_SetFlag(c,0xB);BtlChar_SetFlag(c,0x136);BtlChar_ClearFlag(c,0x125);
    for(bit=0x42;bit<=0x46;++bit)BtlChar_SetFlag(c,bit);
}
static s32 fusion_streaming(void){s32 a;for(a=0;a<10;++a)if(sFusion[a].state==F_NATIVE||sFusion[a].state>=F_SEPARATE)return 1;return 0;}
static s32 fusion_begin(s32 a,s32 b){
    BtlMgrChr *c=BtlChar_Get(a),*other=BtlChar_Get(b);TagFusionSlot *s=&sFusion[a],*p=&sFusion[b];
    s32 index=s->index,seq=BtlParam_GetUnkB1(c,index);
    if(fusion_streaming()||!fusion_idle(a)||!fusion_idle(b)||!fusion_compatible(a,b,index))return 0;
    s->original=*BtlMember_GetActive(c);p->original=*BtlMember_GetActive(other);
    s->originalCount=c->memberCount;s->originalActive=c->curMember;s->shadow=c->members[1];
    s->result=BtlParam_GetUnkB4(c,index);
    s->health=s->original.gauge.health+p->original.gauge.health;
    s->maximum=s->original.gauge.healthMax+p->original.gauge.healthMax;
    s->ki=s->original.gauge.ki;s->blast=s->original.gauge.blast-BtlParam_GetCostAE(c,index);
    s->pad=c->pad;p->pad=other->pad;s->inject=c->injectOn;p->inject=other->injectOn;
    s->controller=a;
    if(getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_COOP_TEST")&&atoi(getenv("BT4_TAG_COOP_TEST"))==4){
        for(s32 test=0;test<100;++test){u64 next=((u64)(u32)gBtlChars->unk18*714025+4096)%150889;if(next&1)break;BtlChar_Rand();}
    }
    if(BattleSide_GetControl(b)==BTL_CONTROL_PAD &&
       (BattleSide_GetControl(a)!=BTL_CONTROL_PAD || (BtlChar_Rand()&1)))s->controller=b;
    /* Fusion dance is temporary. Native Potara remains permanent; it is not
     * forcibly separated by a menu timer. */
    s->total=s->ticks=seq==1?sFusionDuration*30:0;
    c->members[1]=p->original;c->memberCount=2;
    if(!BtlAct_QueueFusion(c,index,1)){c->members[1]=s->shadow;c->memberCount=s->originalCount;return 0;}
    s->state=F_NATIVE;p->state=F_PARTNER;s->startup=0;
    BtlAct_ClearQueue(other);BtlMember_ClearQueuedDamage(other);fusion_hold_partner(b);
    BtlAct_Request(c,BtlAct_GetQueued(c));
    if(getenv("BT4_RENDER_TEST"))fprintf(stderr,"tag fusion: native start actors=%d,%d result=%d hp=%d max=%d\n",a,b,s->result,s->health,s->maximum);
    return 1;
}
static void fusion_distribute(s32 total,s32 initialA,s32 initialB,s32 maxA,s32 maxB,s32 *outA,s32 *outB){
    s32 maximum=maxA+maxB,left;
    total=fusion_clamp(total,maximum);
    left=maximum?(s32)(initialA+(s64)(total-initialA-initialB)*maxA/maximum):0;
    left=fusion_clamp(left,maxA);*outB=fusion_clamp(total-left,maxB);*outA=total-*outB;
}
static void fusion_separate(s32 a){
    BtlMgrChr *c=BtlChar_Get(a);BtlMgrGauge *g=&BtlMember_GetActive(c)->gauge;
    TagFusionSlot *s=&sFusion[a],*p=&sFusion[s->peer];s32 hp=fusion_clamp(g->health,s->maximum),remaining;
    /* Allocate the current pooled health by each original maximum. Integer
     * remainder goes to the partner so one HP is never duplicated or lost. */
    fusion_distribute(hp,s->original.gauge.health,p->original.gauge.health,s->original.gauge.healthMax,p->original.gauge.healthMax,&s->health,&p->health);
    /* Spend/gain resources proportionally to original capacities. The
     * partner's stored gauge is part of the pool, never a free refill. */
    remaining=fusion_clamp(g->ki,g->kiMax)+p->original.gauge.ki;
    fusion_distribute(remaining,s->original.gauge.ki,p->original.gauge.ki,s->original.gauge.kiMax,p->original.gauge.kiMax,&s->ki,&p->ki);
    remaining=fusion_clamp(g->blast,g->blastMax)+p->original.gauge.blast;
    fusion_distribute(remaining,s->original.gauge.blast,p->original.gauge.blast,s->original.gauge.blastMax,p->original.gauge.blastMax,&s->blast,&p->blast);
    s->state=F_SEPARATE;BtlAct_ClearQueue(c);BtlAct_Request(c,0x103);
}
void TagFusion_Update(void){
    s32 a;if(!TagFusion_Local()||!TagFusion_Ticking())return;
    ++sFusionClock;
    for(a=0;a<TagBattle_Count();++a){
        TagFusionSlot *s=&sFusion[a];s32 b=s->peer;
        if(s->state==F_PARTNER){fusion_hold_partner(a);continue;}
        if(s->state==F_INVITE&&s->index>=0){
            if(!fusion_valid(b)||!fusion_idle(a)||!fusion_idle(b)||fusion_distance(a,b)>120*120||!fusion_compatible(a,b,s->index)||(!s->accepted&&--s->ticks<=0)){fusion_cancel(a);continue;}
            if((BattleSide_GetControl(b)==BTL_CONTROL_CPU && s->ticks<=F_INVITE_TICKS-9)||sFusion[b].press)
                s->accepted=sFusion[b].accepted=1;
            if(s->accepted && BattleSide_GetControl(a)==BTL_CONTROL_CPU &&
               (!fusion_cpu_safe(a)||!fusion_cpu_safe(b))){fusion_cancel(a);continue;}
            if(s->accepted)fusion_begin(a,b);
        }else if(s->state==F_NATIVE){
            BtlMgrChr *c=BtlChar_Get(a);
            if(BtlMember_GetActive(c)->chara==s->result&&BtlAct_GetCurrent(c)!=0xF1&&BtlAct_GetCurrent(c)!=0xF2){
                c->members[1]=s->shadow;c->memberCount=s->originalCount;s->state=F_FUSED;
                if(s->controller==b){
                    c->pad=sFusion[b].pad;BtlChar_Get(b)->pad=s->pad;
                    c->injectOn=sFusion[b].inject;
                    s->consume=sFusion[b].consume;
                    sFusion[b].consume=0;
                    BtlInput_BuildMaskTable(c);BtlInput_BuildMaskTable(BtlChar_Get(b));
                }
                if(getenv("BT4_RENDER_TEST"))fprintf(stderr,"tag fusion: playable actor=%d partner=%d controller=%d pad=%d ticks=%d\n",a,b,s->controller,c->pad,s->ticks);
            }else{
                /* A hit can win native action arbitration on the same tick
                 * as consent. Release the reserved ally if the fusion never
                 * enters its native action or loader, preserving actual damage. */
                s32 action=BtlAct_GetCurrent(c);
                if(action==0xF1||action==0xF2||TagBattle_IsActorLoading(a))s->startup=0;
                else if(++s->startup>=3){
                    TagFusionSlot *partner=&sFusion[b];
                    c->members[1]=s->shadow;c->memberCount=s->originalCount;
                    if(BtlAct_GetQueued(c)==0xF1||BtlAct_GetQueued(c)==0xF2)BtlAct_ClearQueue(c);
                    partner->health=partner->original.gauge.health;
                    partner->ki=partner->original.gauge.ki;partner->blast=partner->original.gauge.blast;
                    fusion_restore(b);BtlAct_Request(BtlChar_Get(b),0xB);
                    memset(partner,0,sizeof(*partner));memset(s,0,sizeof(*s));
                    if(getenv("BT4_RENDER_TEST"))fprintf(stderr,"tag fusion: interrupted start released actors=%d,%d\n",a,b);
                }
            }
        }else if(s->state==F_FUSED){
            BtlMgrChr *c=BtlChar_Get(a);
            if(s->ticks>0)--s->ticks;
            /* Let a lethal rush finish its native victim transaction and fall
             * before replacing the fused model. Never strand its opponent. */
            if((s->total&&(s->ticks==0||s->press==2)&&fusion_idle(a))||
               (BtlMember_GetActive(c)->gauge.health<=0&&BtlAct_GetCurrent(c)==0xD8&&TagBattle_Pair(a)<0))fusion_separate(a);
        }
    }
    if(sFusionClock>=120 && sFusionClock%3==0 && !fusion_streaming() && fusion_test_allows_cpu()){
        a=sFusionClock%F_CPU_INTERVAL/3;
        if(fusion_valid(a)&&BattleSide_GetControl(a)==BTL_CONTROL_CPU)fusion_cpu_try(a);
    }
    fusion_test_after();
}
/* After the native merge: preserve its equipment combination but not the
 * game's optional fusion heal, Ki refill or second HP multiplier. */
void TagFusion_AfterModelLoaded(void *fighter){
    BtlMgrChr *c=fighter;TagFusionSlot *s;BtlMgrGauge *g;
    if(!fusion_valid(c->side))return;s=&sFusion[c->side];
    if(s->state!=F_NATIVE&&s->state!=F_FUSED)return;
    if(s->state==F_NATIVE&&BtlMember_GetActive(c)->chara!=s->result)return;
    g=&BtlMember_GetActive(c)->gauge;g->healthMax=s->maximum;g->health=s->health;
    g->ki=fusion_clamp(s->ki,g->kiMax);g->blast=fusion_clamp(s->blast,g->blastMax);g->unk1C=0;
    BtlChar_ClearFlag(c,6);
}
void TagFusion_BeforeModelLoaded(void *fighter){
    BtlMgrChr *c=fighter;TagFusionSlot *s;BtlMgrGauge *g;
    if(!fusion_valid(c->side)||(s=&sFusion[c->side])->state!=F_FUSED)return;
    g=&BtlMember_GetActive(c)->gauge;s->health=g->health;s->maximum=g->healthMax;s->ki=g->ki;s->blast=g->blast;
}
static void fusion_restore(s32 a){
    BtlMgrChr *c=BtlChar_Get(a);TagFusionSlot *s=&sFusion[a];BtlMgrMember *m=BtlMember_GetActive(c);s32 bit;
    *m=s->original;m->gauge.health=fusion_clamp(s->health,m->gauge.healthMax);
    m->gauge.ki=fusion_clamp(s->ki,m->gauge.kiMax);m->gauge.blast=fusion_clamp(s->blast,m->gauge.blastMax);
    m->gauge.unk1C=0;m->gauge.lowHealth=m->gauge.health<10000;
    c->pad=s->pad;c->injectOn=s->inject;BtlInput_BuildMaskTable(c);
    c->freeze=c->freezeNext=c->freezeDelay=0;BtlChar_ClearFlag(c,6);BtlChar_ClearFlag(c,0xB);
    BtlChar_ClearFlag(c,0x136);BtlChar_ClearFlag(c,0x125);BtlMember_ClearQueuedDamage(c);
    for(bit=0x42;bit<=0x46;++bit)BtlChar_ClearFlag(c,bit);
}
static void fusion_place_partner(s32 actor,s32 partner){
    extern f32 BtlCharApi_GetRadius(s32),BtlStage_GetRadius(void),Mathf_Sin(f32),Mathf_Cos(f32);
    extern s32 BtlStage_FindZoneNear(s32,f32 *),StgCol_TraceSegment(void *);
    extern void BtlChar_Place(BtlMgrChr *,f32 *,f32 *,s32);
    BtlMgrChr *c=BtlChar_Get(actor);f32 pos[4],rot[4],radius,limit; s32 i;
    struct {f32 from[4],to[4];} seg;
    memcpy(pos,&c->pose,16);memcpy(rot,(u8*)&c->pose+16,16);
    radius=BtlCharApi_GetRadius(BattleSide_GetObjId(actor))+BtlCharApi_GetRadius(BattleSide_GetObjId(partner))+8;
    if(radius<24)radius=24;
    limit=BtlStage_GetRadius()-BtlCharApi_GetRadius(BattleSide_GetObjId(partner))-4;
    for(i=0;i<8;++i){f32 angle=rot[1]+1.5707963f+i*.78539816f;
        memcpy(seg.from,pos,16);memcpy(seg.to,pos,16);
        seg.to[0]+=Mathf_Sin(angle)*radius;seg.to[2]+=Mathf_Cos(angle)*radius;
        if(seg.to[0]*seg.to[0]+seg.to[2]*seg.to[2]>limit*limit||StgCol_TraceSegment(&seg))continue;
        BtlChar_Place(BtlChar_Get(partner),seg.to,rot,BtlStage_FindZoneNear(-1,seg.to));return;
    }
    /* If no safe nearby position is found, retain the partner's previous
     * valid placement instead of teleporting through stage geometry. */
}
void TagFusion_Action(void *fighter,s32 phase){
    BtlMgrChr *c=fighter;TagFusionSlot *s=&sFusion[c->side];s32 bit;
    if(phase==0){BtlChar_SavePlacement(c);BtlMember_ClearQueuedDamage(c);BtlChar_ClearFlag(c,6);BtlChar_ClearFlag(c,0x125);}
    if(phase==1){
        if(s->state==F_SEPARATE){
            BtlAct_SetFormCurrent(c);c->newChara=s->original.chara;c->newCostume=s->original.costume;c->new20=s->original.gauge.unk20;
            if(BtlChange_TryRequestChara(c->side,c->newChara,c->newCostume,c->new20,-1,-1,-1))s->state=F_LOADING;
        }else if(s->state==F_LOADING&&BtlChange_IsLoadedFor(c->side)){
            BtlChange_SetReady(c->side);BtlAnim_Request(c,0,0);s->state=F_READY;
        }else if(s->state==F_LOADED){BtlChange_SetDone(c->side);s->state=F_DONE;
        }else if(s->state==F_DONE){BtlChar_SetFlag(c,0xF5);BtlChar_SetFlag(c,0x55);BtlAct_Request(c,0xB);}
        for(bit=0x42;bit<=0x46;++bit)BtlChar_SetFlag(c,bit);
        BtlChar_SetFlag(c,0x136);BtlChar_SetFlag(c,0xBB);BtlChar_SetFlag(c,0x128);
    }
    if(phase==3&&s->state==F_DONE){
        s32 b=s->peer;BtlMgrChr *other=BtlChar_Get(b);
        fusion_restore(b);BtlAct_ClearQueue(other);BtlAct_Request(other,0xB);
        fusion_place_partner(c->side,b);
        sCpuNextInvite[c->side]=sCpuNextInvite[b]=sFusionClock+F_CPU_AFTER_FUSION;
        memset(&sFusion[b],0,sizeof(sFusion[b]));memset(s,0,sizeof(*s));
        if(getenv("BT4_RENDER_TEST"))fprintf(stderr,"tag fusion: separated actors=%d,%d\n",c->side,b);
    }
}
s32 TagFusion_OnModelLoaded(void *fighter){
    BtlMgrChr *c=fighter;TagFusionSlot *s;
    if(!fusion_valid(c->side)||(s=&sFusion[c->side])->state!=F_READY||BtlAct_GetCurrent(c)!=0x103)return 0;
    fusion_restore(c->side);BtlObj_BindCommonTables(BtlChar_GetObj(c));
    BtlMember_LoadParams(c,c->curMember,0,0,0);
    /* The original equipment-derived limits and per-slot HP are authoritative. */
    fusion_restore(c->side);s->state=F_LOADED;return 1;
}
s32 TagFusion_Hud(s32 a,s32 *other,s32 *ticks,s32 *total){
    TagFusionSlot *s;if(!TagFusion_Busy(a))return 0;s=&sFusion[a];
    *other=s->peer;*ticks=s->ticks;*total=s->total;
    if(s->state==F_INVITE){
        if(s->index<0)*ticks=sFusion[s->peer].ticks;
        return s->accepted?5:s->index<0?2:1;
    }
    if(s->state==F_PARTNER){s=&sFusion[s->peer];*ticks=s->ticks;*total=s->total;return 4;}
    return s->state==F_FUSED?3:0;
}
#include "tag_team_fusion_audit.inc"
#endif
