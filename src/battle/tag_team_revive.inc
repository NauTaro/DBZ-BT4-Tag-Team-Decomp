/* GPL-3.0-or-later. Shared-rule proximity revival, advanced once per simulation
 * tick. Fixed ten-slot storage; no model copies, actor replacement or heap cache. */
#include "battle/tag_team_revive.h"
#include "battle/tag_team_fusion.h"
extern void BtlAct_ClearQueue(void *),BtlAct_Request(void *,s32);
extern void BtlChar_ClearFlag(void *,s32),BtlChar_SetHeldFlag(void *,s32);
extern void BtlAi_TagRescueInput(s32,f32 *,s32,s32);

typedef struct TagReviveSlot {
    s32 progress,helpers,completed,ready,settled,hp,seen,target,busy;
    f32 last[4];
} TagReviveSlot;
static TagReviveSlot sTagRevive[10];
static s32 sTagReviveTeam[2],sTagReviveTicks,sTagReviveCharge,sTagRevivePercent;
static s32 sTagReviveAlpha,sTagReviveCpu,sTagReviveClock;
static f32 sTagReviveRange;

static s32 TagRevive_Setting(const char *key,s32 fallback,s32 low,s32 high){
    s32 v=Port_TagOption(key,fallback);return v<low||v>high?fallback:v;
}
static s32 TagRevive_Local(void){
    return TagBattle_Active()&&TagBattle_Local()&&!TagBattle_FreeForAll();
}
static s32 TagRevive_Running(void){
    return TagRevive_Local()&&BtlSeq_IsFighting()&&
        !(Battle_GetWork()->flags&(BATTLE_FLAG_PAUSE|BATTLE_FLAG_LOADING));
}
void TagRevive_Reset(void){
    s32 i;
    memset(sTagRevive,0,sizeof(sTagRevive));
    for(i=0;i<10;++i){sTagRevive[i].target=-1;BtlAi_TagRescueInput(i,NULL,0,0);}
    sTagReviveTeam[0]=TagRevive_Setting("tag_revive_team1",0,0,1);
    sTagReviveTeam[1]=TagRevive_Setting("tag_revive_team2",0,0,1);
    sTagReviveTicks=TagRevive_Setting("tag_revive_seconds",10,5,20)*30*4;
    sTagReviveCharge=TagRevive_Setting("tag_revive_charge",0,0,1);
    sTagRevivePercent=TagRevive_Setting("tag_revive_health",25,1,100);
    sTagReviveRange=(f32)TagRevive_Setting("tag_revive_radius",40,20,100);
    sTagReviveAlpha=TagRevive_Setting("tag_revive_opacity",30,10,80);
    sTagReviveCpu=TagRevive_Setting("tag_revive_cpu",1,0,1);
    sTagReviveClock=0;
}
/* A corpse first completes its fall. Queued reactions, model loads and paired
 * cinematics cannot be interrupted by recovery. */
static s32 TagRevive_Downed(s32 actor){
    void *c=BtlChar_Get(actor);s32 team=TagBattle_Team(actor);
    if(TagFusion_Busy(actor)||team<0||team>1||!sTagReviveTeam[team]||!c||TagBattle_IsActorLoading(actor)||
       TagBattle_Pair(actor)>=0||BtlCharApi_GetHp(actor)>0)return 0;
    return BtlAct_GetCurrent(c)==0xD8&&!BtlChar_IsFrozen(c)&&
        BtlAct_GetQueued(c)<0&&BtlAct_GetRequested(c)<0;
}
static s32 TagRevive_Idle(s32 actor){
    void *c=BtlChar_Get(actor);s32 action,request;
    if(!c||BtlCharApi_GetHp(actor)<=0||TagBattle_IsActorLoading(actor)||TagBattle_Pair(actor)>=0||
       !BtlChar_IsFree(c)||BtlChar_IsFrozen(c))return 0;
    action=BtlAct_GetCurrent(c);request=BtlAct_GetRequested(c);
    if(!((action>=0xB&&action<=0x1A)||action==0x37||action==0x43))return 0;
    return BtlAct_GetQueued(c)<0&&(request<0||request==action||request==0xB||request==0x37);
}
static s32 TagRevive_InRange(s32 helper,s32 corpse){
    f32 p[4],q[4],dx,dy,dz;TagAssistSegment seg;
    BtlCharApi_GetBasePos(BattleSide_GetObjId(helper),p);
    BtlCharApi_GetBasePos(BattleSide_GetObjId(corpse),q);
    dx=p[0]-q[0];dy=p[1]-q[1];dz=p[2]-q[2];
    if(dx*dx+dy*dy+dz*dz>sTagReviveRange*sTagReviveRange)return 0;
    memcpy(seg.from,p,sizeof(p));memcpy(seg.to,q,sizeof(q));
    seg.from[1]-=4;seg.to[1]-=4;
    return !StgCol_TraceSegment(&seg);
}
/* Scan every live enemy. A rescue under an immediate threat waits for a safe
 * opening; keep the enemy lock-on instead of locking onto an allied corpse. */
static s32 TagRevive_Safe(s32 actor,s32 corpse){
    s32 i;
    for(i=0;i<TagBattle_Count();++i){
        if(TagBattle_Allied(actor,i)||BtlCharApi_GetHp(i)<=0||TagBattle_IsActorLoading(i))continue;
        if(TagAssist_Distance(actor,i)<50*50||TagAssist_Distance(corpse,i)<65*65)return 0;
    }
    return 1;
}
void TagRevive_PrepareInput(void){
    s32 actor,i;
    if(!TagRevive_Running())return;
    for(actor=0;actor<TagBattle_Count();++actor){
        TagReviveSlot *s=&sTagRevive[actor];s32 best=-1;f32 distance=1.0e20f,goal[4];
        s->busy=0;
        if(!sTagReviveCpu||BattleSide_GetControl(actor)==BTL_CONTROL_PAD||!TagRevive_Idle(actor))continue;
        for(i=0;i<TagBattle_Count();++i){
            f32 d;
            if(i==actor||!TagBattle_Allied(actor,i)||!TagRevive_Downed(i)||!sTagRevive[i].ready||
               !TagRevive_Safe(actor,i))continue;
            d=TagAssist_Distance(actor,i);
            /* Retain an interrupted rescue when it remains safe. */
            if(i==s->target)d*=.8f;
            if(d<distance){distance=d;best=i;}
        }
        if(best<0){
            /* Combat can interrupt a rescue without forgetting its downed
             * ally. Retain the intent while unsafe, but release control to AI. */
            if(s->target<0||s->target>=TagBattle_Count()||
               !TagBattle_Allied(actor,s->target)||!TagRevive_Downed(s->target))s->target=-1;
            continue;
        }
        s->target=best;s->busy=1;
        BtlCharApi_GetBasePos(BattleSide_GetObjId(best),goal);
        BtlAi_TagRescueInput(actor,goal,TagRevive_InRange(actor,best),sTagReviveCharge);
    }
}
void TagRevive_Update(void){
    s32 actor,helper;
    if(!TagRevive_Running())return;
    ++sTagReviveClock;
    for(actor=0;actor<TagBattle_Count();++actor){
        TagReviveSlot *s=&sTagRevive[actor];f32 p[4],dx,dy,dz;s32 hp=BtlCharApi_GetHp(actor);
        BtlCharApi_GetBasePos(BattleSide_GetObjId(actor),p);
        dx=p[0]-s->last[0];dy=p[1]-s->last[1];dz=p[2]-s->last[2];
        s->settled=s->seen&&dx*dx+dy*dy+dz*dz<.25f&&hp>=s->hp;
        memcpy(s->last,p,sizeof(p));s->seen=1;s->hp=hp;s->helpers=0;
        if(hp>0){s->progress=s->ready=0;continue;}
        if(TagRevive_Downed(actor)&&s->settled){if(s->ready<3)++s->ready;}
        else s->ready=0;
    }
    /* A helper contributes to only its nearest corpse. Multiple helpers share
     * one progress counter, with x1/x1.25/x1.5/x1.75/x2 cooperative speed. */
    for(helper=0;helper<TagBattle_Count();++helper){
        s32 best=-1;f32 distance=1.0e20f;
        if(!TagRevive_Idle(helper)||!sTagRevive[helper].settled)continue;
        if(BattleSide_GetControl(helper)!=BTL_CONTROL_PAD&&(!sTagReviveCpu||!sTagRevive[helper].busy))continue;
        if(sTagReviveCharge){
            void *fighter=BtlChar_Get(helper);
            /* A full gauge can end the charge animation while the player
             * keeps the charge control held. Revival uses the same input. */
            extern s32 TagBattle_DevCombatActive(void);
            if(BtlAct_GetCurrent(fighter)!=0x37&&
               (!TagBattle_DevCombatActive()||(BattleSide_GetControl(helper)==BTL_CONTROL_PAD&&
               !BtlInput_IsHeld(fighter,BTLB_CHARGE))))continue;
        }
        for(actor=0;actor<TagBattle_Count();++actor){
            f32 d;
            if(!sTagRevive[actor].ready||!TagRevive_Downed(actor)||
               !TagBattle_Allied(actor,helper)||!TagRevive_InRange(helper,actor))continue;
            if(BattleSide_GetControl(helper)!=BTL_CONTROL_PAD&&sTagRevive[helper].target!=actor)continue;
            d=TagAssist_Distance(helper,actor);
            if(d<distance){distance=d;best=actor;}
        }
        if(best>=0)++sTagRevive[best].helpers;
    }
    for(actor=0;actor<TagBattle_Count();++actor){
        TagReviveSlot *s=&sTagRevive[actor];s32 *g,hp;void *c;
        if(!s->helpers||!s->ready)continue;
        s->progress+=3+(s->helpers>5?5:s->helpers);
        if(s->progress<sTagReviveTicks)continue;
        if(!TagRevive_Downed(actor))continue;
        c=BtlChar_Get(actor);g=BtlMember_GetActiveGauge(c);
        hp=(g[1]/100)*sTagRevivePercent+(g[1]%100)*sTagRevivePercent/100;
        if(hp<1)hp=1;
        /* Retain the actor, model, equipment, ki and position. Native action
         * dispatch owns all get-up animation/queue cleanup. No free ki refill. */
        BtlAct_ClearQueue(c);BtlChar_ClearFlag(c,0x137);BtlChar_SetHeldFlag(c,5);
        g[0]=hp;BtlAct_Request(c,BtlChar_TestFlag(c,0x11)?0xE6:0xE1);
        s->hp=hp;s->progress=s->ready=0;++s->completed;
        if(getenv("BT4_RENDER_TEST"))fprintf(stderr,"tag revive: actor=%d hp=%d max=%d helpers=%d count=%d\n",actor,hp,g[1],s->helpers,s->completed);
    }
}
s32 TagRevive_Busy(s32 actor){return TagRevive_Running()&&actor>=0&&actor<10&&sTagRevive[actor].busy;}
s32 TagRevive_Hud(s32 actor,s32 *progress,s32 *total,s32 *helpers){
    if(!TagRevive_Local()||actor<0||actor>=TagBattle_Count()||!sTagRevive[actor].ready||!TagRevive_Downed(actor))return 0;
    *progress=sTagRevive[actor].progress;*total=sTagReviveTicks;*helpers=sTagRevive[actor].helpers;return 1;
}
s32 TagRevive_Opacity(void){return sTagReviveAlpha;}
f32 TagRevive_Radius(void){return sTagReviveRange;}
