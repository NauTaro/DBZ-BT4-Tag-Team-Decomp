/* Dev-only local coordinated blasts. Fighters retain their own native move,
 * ki cost, effects, damage, control and normal vulnerability. No teleport,
 * damage multiplier, fabricated unlock or global cinematic lock is added. */
#include "battle/tag_team_assist.h"
#include "battle/tag_team_revive.h"
#include "battle/tag_team_fusion.h"
extern s32 BtlAct_GetCurrent(void *),BtlAct_GetRequested(void *),BtlAct_GetQueued(void *);
extern s32 BtlAct_CanUseTechnique(void *,s32),BtlAct_CanQueueTechnique(void *,s32),BtlAct_QueueTechnique(void *,s32);
extern s32 BtlSuper_GetType(void *,s32),BtlSuper_GetFlags(void *,s32);
extern s32 BtlChar_IsFree(void *),BtlChar_IsFrozen(void *),BtlChar_TestFlag(void *,s32);
extern int Port_Setting(const char *,int);
extern f32 BtlCharApi_GetHeight(s32),BtlCharApi_GetRadius(s32),BtlStage_GetRadius(void);
extern void BtlAi_TagAssistInput(s32,f32 *);
extern f32 sqrtf(f32);
typedef struct TagAssistSegment {f32 from[4],to[4];} TagAssistSegment;
extern s32 StgCol_TraceSegment(TagAssistSegment *);

enum { TAG_ASSIST_WINDOW=90,TAG_ASSIST_COOLDOWN=300,TAG_ASSIST_NOTICE=60 };
typedef struct TagAssistSlot {
    s32 partner,target,slot,otherSlot,wait,active,cooldown,launch,press,held,consume,side,ready,approach,relay;
} TagAssistSlot;
static TagAssistSlot sTagAssist[10];
static s32 sTagAssistEnabled,sTagAssistTick;

static s32 TagAssist_Local(void){
    return sTagAssistEnabled&&TagBattle_Active()&&TagBattle_Local()&&!TagBattle_FreeForAll();
}
s32 TagAssist_Busy(s32 actor){
    return TagAssist_Local() && actor >= 0 && actor < TagBattle_Count() &&
        (sTagAssist[actor].wait || sTagAssist[actor].active || sTagAssist[actor].launch >= 0);
}
static s32 TagAssist_Running(void){
    return TagAssist_Local()&&BtlSeq_IsFighting()&&
        !(Battle_GetWork()->flags&(BATTLE_FLAG_PAUSE|BATTLE_FLAG_LOADING));
}
static s32 TagAssist_Idle(s32 actor){
    void *chr;
    s32 action,request;
    if(actor<0||actor>=TagBattle_Count()||TagBattle_IsActorLoading(actor)||TagBattle_Pair(actor)>=0||
       BtlCharApi_GetHp(actor)<=0||TagRevive_Busy(actor)||TagFusion_Busy(actor))return 0;
    chr=BtlChar_Get(actor);
    if(!chr||!BtlChar_IsFree(chr)||BtlChar_IsFrozen(chr))return 0;
    action=BtlAct_GetCurrent(chr);request=BtlAct_GetRequested(chr);
    /* Neutral, walk, ascend/descend and ki charge only. Attacks, dodges,
     * transformations, rushes, grabs and hit reactions cannot be cancelled. */
    if(!((action>=0xB&&action<=0xE)||(action>=0x10&&action<=0x16)||action==0x37))return 0;
    if(request>=0&&request!=action&&request!=0xB)return 0;
    return BtlAct_GetQueued(chr)<0;
}
static s32 TagAssist_Technique(s32 actor){
    void *chr=BtlChar_Get(actor);
    s32 slot;
    if(!TagAssist_Idle(actor))return -1;
    for(slot=2;slot<=3;++slot){
        s32 type=BtlSuper_GetType(chr,slot),flags=BtlSuper_GetFlags(chr,slot);
        if((type==0||type==1||type==2)&&!(flags&(0x2000|0x4000))&&BtlAct_CanQueueTechnique(chr,slot))return slot;
    }
    return -1;
}
static f32 TagAssist_Distance(s32 a,s32 b){
    f32 p[4],q[4],dx,dy,dz;
    BtlCharApi_GetBasePos(BattleSide_GetObjId(a),p);
    BtlCharApi_GetBasePos(BattleSide_GetObjId(b),q);
    dx=p[0]-q[0];dy=p[1]-q[1];dz=p[2]-q[2];
    return dx*dx+dy*dy+dz*dz;
}
/* Keep the helper behind the leader's firing plane and outside the beam.
 * Only virtual movement input is used; stage collision remains authoritative. */
static s32 TagAssist_Formation(s32 actor,s32 partner,s32 side,f32 *goal){
    f32 p[4],q[4],t[4],dx,dz,n,height,rear,lateral,radius,limit;
    TagAssistSegment seg;
    s32 i;
    BtlCharApi_GetBasePos(BattleSide_GetObjId(actor),p);
    BtlCharApi_GetBasePos(BattleSide_GetObjId(partner),q);
    BtlCharApi_GetBasePos(BattleSide_GetObjId(TagBattle_Target(actor)),t);
    dx=t[0]-p[0];dz=t[2]-p[2];n=sqrtf(dx*dx+dz*dz);
    if(n<1.0f)return 0;
    dx/=n;dz/=n;
    height=BtlCharApi_GetHeight(BattleSide_GetObjId(actor));
    if(BtlCharApi_GetHeight(BattleSide_GetObjId(partner))>height)
        height=BtlCharApi_GetHeight(BattleSide_GetObjId(partner));
    radius=BtlCharApi_GetRadius(BattleSide_GetObjId(actor))+BtlCharApi_GetRadius(BattleSide_GetObjId(partner));
    lateral=height*1.0f;if(lateral<20.0f)lateral=20.0f;
    if(lateral<radius+6.0f)lateral=radius+6.0f;
    rear=height*.4f;if(rear<8.0f)rear=8.0f;
    /* Decline formations too large for the request radius instead of clipping
     * giant fighters into the leader or an arena boundary. */
    if(lateral*lateral+rear*rear>100.0f*100.0f)return 0;
    goal[0]=p[0]-dx*rear+dz*lateral*side;
    goal[1]=p[1];goal[2]=p[2]-dz*rear-dx*lateral*side;goal[3]=1;
    limit=BtlStage_GetRadius()-BtlCharApi_GetRadius(BattleSide_GetObjId(partner))-4.0f;
    if(limit<=0||goal[0]*goal[0]+goal[2]*goal[2]>limit*limit)return 0;
    for(i=0;i<4;++i){seg.from[i]=q[i];seg.to[i]=goal[i];}
    seg.from[1]-=height*.5f;seg.to[1]-=height*.5f;
    return !StgCol_TraceSegment(&seg);
}
static s32 TagAssist_Formed(s32 partner,f32 *goal){
    f32 q[4],dx,dy,dz;
    BtlCharApi_GetBasePos(BattleSide_GetObjId(partner),q);
    dx=goal[0]-q[0];dy=goal[1]-q[1];dz=goal[2]-q[2];
    return dx*dx+dz*dz<=6.0f*6.0f&&dy*dy<=6.0f*6.0f;
}
static s32 TagAssist_ChooseSide(s32 actor,s32 partner){
    f32 goal[4];
    /* Prefer the reference's screen-right shoulder. Use the other shoulder
     * only when stage geometry blocks that placement. */
    if(TagAssist_Formation(actor,partner,1,goal))return 1;
    return TagAssist_Formation(actor,partner,-1,goal)?-1:0;
}
static void TagAssist_Approach(s32 actor,s32 partner,f32 *goal){
    TagAssistSlot *s=&sTagAssist[actor];
    f32 p[4],q[4],t[4],dx,dz,n,lateral,forward,safeRear;
    BtlCharApi_GetBasePos(BattleSide_GetObjId(actor),p);
    BtlCharApi_GetBasePos(BattleSide_GetObjId(partner),q);
    BtlCharApi_GetBasePos(BattleSide_GetObjId(TagBattle_Target(actor)),t);
    dx=t[0]-p[0];dz=t[2]-p[2];n=sqrtf(dx*dx+dz*dz);if(n<1)return;dx/=n;dz/=n;
    lateral=(q[0]-p[0])*dz-(q[2]-p[2])*dx;
    forward=(q[0]-p[0])*dx+(q[2]-p[2])*dz;
    safeRear=BtlCharApi_GetRadius(BattleSide_GetObjId(actor))+BtlCharApi_GetRadius(BattleSide_GetObjId(partner))+10;
    if(s->approach==0){
        if(forward<=-safeRear+4)s->approach=1;
        else{
            goal[0]=p[0]-dx*safeRear+dz*lateral;
            goal[2]=p[2]-dz*safeRear-dx*lateral;return;
        }
    }
    if(s->approach==1){
        f32 wanted=(goal[0]-p[0])*dz-(goal[2]-p[2])*dx;
        if((wanted-lateral)*(wanted-lateral)<=4*4)s->approach=2;
        else{
            goal[0]=p[0]-dx*safeRear+dz*wanted;
            goal[2]=p[2]-dz*safeRear-dx*wanted;
        }
    }
}
static s32 TagAssist_TargetValid(s32 actor,s32 target){
    return target>=0&&target<TagBattle_Count()&&!TagBattle_Allied(actor,target)&&
        !TagBattle_IsActorLoading(target)&&BtlCharApi_GetHp(target)>0&&TagBattle_Pair(target)<0;
}
static s32 TagAssist_CandidateFor(s32 actor,s32 cpuOnly){
    s32 i,best=-1,target=TagBattle_Target(actor);
    f32 distance=120.0f*120.0f;
    if(sTagAssist[actor].wait||sTagAssist[actor].active||sTagAssist[actor].cooldown||
       TagRelay_Engaged(actor)||!TagAssist_TargetValid(actor,target)||TagAssist_Technique(actor)<0)return -1;
    for(i=0;i<TagBattle_Count();++i){
        f32 d;
        if(i==actor||(cpuOnly&&BattleSide_GetControl(i)==BTL_CONTROL_PAD)||!TagBattle_Allied(actor,i)||sTagAssist[i].wait||sTagAssist[i].active||
           sTagAssist[i].cooldown||TagRelay_Engaged(i)||TagAssist_Technique(i)<0)continue;
        d=TagAssist_Distance(actor,i);
        if(d<distance){best=i;distance=d;}
    }
    return best;
}
static s32 TagAssist_Candidate(s32 actor){return TagAssist_CandidateFor(actor,0);}
static s32 TagAssist_Start(s32 actor,s32 partner){
    TagAssistSlot *s=&sTagAssist[actor];
    s->relay=0;
    s->side=TagAssist_ChooseSide(actor,partner);s->ready=0;s->approach=-1;
    if(!s->side)return 0;
    s->partner=partner;s->target=TagBattle_Target(actor);
    s->slot=TagAssist_Technique(actor);s->otherSlot=TagAssist_Technique(partner);
    s->wait=TAG_ASSIST_WINDOW;
    sTagAssist[partner].partner=actor;sTagAssist[partner].wait=-1;
    if(getenv("BT4_RENDER_TEST"))fprintf(stderr,"tag joint: request actor=%d partner=%d target=%d\n",actor,partner,s->target);
    return 1;
}
static void TagAssist_Cancel(s32 actor){
    s32 partner=sTagAssist[actor].partner;
    if(getenv("BT4_RENDER_TEST")&&partner>=0&&partner<TagBattle_Count())
        fprintf(stderr,"tag joint: cancel actor=%d partner=%d wait=%d idle=%d,%d actions=%X,%X target=%d valid=%d distance2=%d\n",
            actor,partner,sTagAssist[actor].wait,TagAssist_Idle(actor),TagAssist_Idle(partner),
            BtlAct_GetCurrent(BtlChar_Get(actor)),BtlAct_GetCurrent(BtlChar_Get(partner)),
            TagBattle_Target(actor),TagAssist_TargetValid(actor,sTagAssist[actor].target),(s32)TagAssist_Distance(actor,partner));
    sTagAssist[actor].partner=-1;sTagAssist[actor].wait=0;
    if(partner>=0&&partner<TagBattle_Count()&&sTagAssist[partner].partner==actor){
        sTagAssist[partner].partner=-1;sTagAssist[partner].wait=0;
    }
}
void TagAssist_Reset(void){
    s32 i;
    memset(sTagAssist,0,sizeof(sTagAssist));
    sTagAssistTick=0;
    for(i=0;i<10;++i)sTagAssist[i].partner=sTagAssist[i].target=sTagAssist[i].launch=-1;
    sTagAssistEnabled=Port_TagOption("tag_off_joint",1)!=0;
}
void TagAssist_ReadPads(void){
    s32 i;
    if(!TagAssist_Running())return;
    for(i=0;i<TagBattle_Count();++i){
        TagAssistSlot *s=&sTagAssist[i];
        s->press=s->held=0;
        if(BattleSide_GetControl(i)==BTL_CONTROL_PAD){
            s32 n=BattleSide_GetPad(i);
            if(TagBattle_Network()||(n>=0&&n<PAD_COUNT)){
                Pad *pad=TagBattle_CommandPad(i);
                if(!(pad->held&PAD_R3))s->consume=0;
                s->held=(pad->held&PAD_L2)!=0;
                if((pad->pressed&PAD_R3)&&((s->held&&(s->wait>0||TagAssist_Candidate(i)>=0))||s->wait<0)){
                    s->press=s->consume=1;
                }
                if(s->consume){
                    TagBattle_ConsumeCommand(i,PAD_R3);
                    pad->held&=~PAD_R3;pad->pressed&=~PAD_R3;pad->repeat&=~PAD_R3;
                    pad->gameHeld&=~PADG_R3;pad->gamePressed&=~PADG_R3;pad->gameRepeat&=~PADG_R3;
                }
            }
        }
    }
}
void TagAssist_PrepareInput(void){
    s32 actor;
    if(!TagAssist_Running())return;
    for(actor=0;actor<TagBattle_Count();++actor)if(sTagAssist[actor].wait>0){
        TagAssistSlot *s=&sTagAssist[actor];
        s32 p=s->partner;
        f32 goal[4];
        if(p<0||p>=TagBattle_Count()||BattleSide_GetControl(p)==BTL_CONTROL_PAD)continue;
        if(!TagAssist_Idle(actor)||!TagAssist_Idle(p)||!TagAssist_Formation(actor,p,s->side,goal)){
            TagAssist_Cancel(actor);continue;
        }
        if(s->approach<0){
            /* Already on the selected shoulder: no crossing detour needed. */
            s->approach=TagAssist_Formed(p,goal)?2:0;
        }
        if(s->approach<2)TagAssist_Approach(actor,p,goal);
        if(s->approach==2&&TagAssist_Formed(p,goal)){
            ++s->ready;BtlAi_TagAssistInput(p,NULL);
        }else{
            s->ready=0;BtlAi_TagAssistInput(p,goal);
        }
    }
}
static s32 TagAssist_Commit(s32 actor,s32 partner){
    TagAssistSlot *a=&sTagAssist[actor],*b=&sTagAssist[partner];
    s32 target=a->target;
    if(!TagAssist_Idle(actor)||!TagAssist_Idle(partner)||!TagAssist_TargetValid(actor,target)||
       TagAssist_Distance(actor,partner)>(a->relay?600.0f*600.0f:120.0f*120.0f)||
       !BtlAct_CanQueueTechnique(BtlChar_Get(actor),a->slot)||
       !BtlAct_CanQueueTechnique(BtlChar_Get(partner),a->otherSlot))return 0;
    /* Both retarget in the same simulation step before their native dispatch.
     * Do not reserve a rush pair: the enemy remains available to projectiles. */
    if(!TagBattle_SetTarget(partner,target))return 0;
    a->launch=a->slot;b->launch=a->otherSlot;
    a->active=b->active=TAG_ASSIST_NOTICE;
    a->cooldown=b->cooldown=TAG_ASSIST_COOLDOWN;
    a->wait=b->wait=0;
    b->target=target;
    if(getenv("BT4_RENDER_TEST"))fprintf(stderr,"tag joint: launch actors=%d,%d slots=%d,%d target=%d\n",actor,partner,a->slot,a->otherSlot,target);
    return 1;
}
void TagAssist_Update(void){
    s32 i;
    if(!TagAssist_Running())return;
    ++sTagAssistTick;
    for(i=0;i<TagBattle_Count();++i){
        sTagAssist[i].launch=-1;
        if(sTagAssist[i].cooldown>0)--sTagAssist[i].cooldown;
        if(sTagAssist[i].active>0)--sTagAssist[i].active;
    }
    for(i=0;i<TagBattle_Count();++i){
        TagAssistSlot *s=&sTagAssist[i];
        if(s->wait>0){
            s32 p=s->partner;
            if(p<0||p>=TagBattle_Count()||!TagAssist_Idle(i)||!TagAssist_Idle(p)||
               !TagAssist_TargetValid(i,s->target)||TagBattle_Target(i)!=s->target||
               TagAssist_Distance(i,p)>(s->relay?600.0f*600.0f:120.0f*120.0f)||
               !BtlAct_CanQueueTechnique(BtlChar_Get(i),s->slot)||
               !BtlAct_CanQueueTechnique(BtlChar_Get(p),s->otherSlot)){
                TagAssist_Cancel(i);continue;
            }
            if(--s->wait==0){TagAssist_Cancel(i);continue;}
            if((BattleSide_GetControl(p)==BTL_CONTROL_PAD&&(sTagAssist[p].press||s->relay))||
               (BattleSide_GetControl(p)!=BTL_CONTROL_PAD&&s->wait<=TAG_ASSIST_WINDOW-9&&s->ready>=2)){
                if(!TagAssist_Commit(i,p))TagAssist_Cancel(i);
            }
        }
    }
    for(i=0;i<TagBattle_Count();++i){
        TagAssistSlot *s=&sTagAssist[i];
        s32 partner;
        if(!s->press||!s->held||s->wait||s->active||s->cooldown)continue;
        partner=TagAssist_Candidate(i);
        if(partner<0)continue;
        TagAssist_Start(i,partner);
    }
    /* CPU pairs use the same invitation, formation and native costs. Never
     * recruit a human automatically, consume RNG, or override an active move.
     * Staggered assessments avoid all ten fighters requesting on one tick. */
    if(getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_ASSIST_TEST")){
        s32 test=atoi(getenv("BT4_TAG_ASSIST_TEST"));
        if(test!=13&&test!=14&&test!=15)return;
    }
    for(i=0;i<TagBattle_Count();++i){
        s32 partner,target;
        f32 distance;
        if(BattleSide_GetControl(i)==BTL_CONTROL_PAD||(sTagAssistTick+i*7)%45)continue;
        target=TagBattle_Target(i);
        if(!TagAssist_TargetValid(i,target))continue;
        distance=TagAssist_Distance(i,target);
        if(distance<50.0f*50.0f||distance>450.0f*450.0f)continue;
        partner=TagAssist_CandidateFor(i,1);
        if(partner>=0&&TagAssist_Start(i,partner)&&getenv("BT4_RENDER_TEST"))
            fprintf(stderr,"tag joint CPU: actor=%d partner=%d target=%d\n",i,partner,target);
    }
}
s32 TagAssist_TakeTechnique(s32 actor){
    s32 slot;
    if(!TagAssist_Running()||actor<0||actor>=TagBattle_Count())return -1;
    slot=sTagAssist[actor].launch;sTagAssist[actor].launch=-1;
    if(slot<0||!TagAssist_Idle(actor)||!TagAssist_TargetValid(actor,sTagAssist[actor].target))return -1;
    return slot;
}
s32 TagAssist_Hud(s32 actor,s32 *partner,s32 *ticks){
    TagAssistSlot *s;
    if(!TagAssist_Local()||actor<0||actor>=TagBattle_Count()||BtlCharApi_GetHp(actor)<=0)return 0;
    s=&sTagAssist[actor];*partner=s->partner;
    if(s->wait>0){*ticks=s->wait;return 2;}
    if(s->wait<0){*ticks=sTagAssist[s->partner].wait;return 3;}
    if(s->active>0){*ticks=s->active;return 4;}
    if(s->held&&(*partner=TagAssist_Candidate(actor))>=0)return 1;
    return 0;
}
s32 TagAssist_CameraPartner(s32 actor){
    TagAssistSlot *s;
    s32 p,a,b;
    if(!TagAssist_Local()||actor<0||actor>=TagBattle_Count())return -1;
    s=&sTagAssist[actor];p=s->partner;
    if(s->wait||s->cooldown<=0||p<0||p>=TagBattle_Count()||
        BtlCharApi_GetHp(actor)<=0||BtlCharApi_GetHp(p)<=0||
        TagBattle_Pair(actor)>=0||TagBattle_Pair(p)>=0)return -1;
    a=BtlAct_GetCurrent(BtlChar_Get(actor));b=BtlAct_GetCurrent(BtlChar_Get(p));
    return s->active||((a>=0x106&&a<=0x111)||(b>=0x106&&b<=0x111))?p:-1;
}

