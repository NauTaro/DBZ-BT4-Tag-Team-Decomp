/* Dev Tag Team: native projectile volumes against every hostile body.
 * Bounded state, separate per-victim multi-hit cadence, no lock-on changes. */
#include <stdio.h>
#include <stdlib.h>
extern s32 TagBattle_Allied(s32,s32),TagBattle_CanInteract(s32,s32),TagBattle_IsActorLoading(s32);
extern s32 BtlCharApi_GetHp(s32),EftHit_IsStoppedByHit(EftDetRec *);
typedef struct TagImpactTask {
    u32 bits,flags;s16 result;s8 frames,hits;
} TagImpactTask;
typedef struct TagImpactState {
    void *task;s32 owner;u32 seen;
    u16 frames[10],hits[10];u32 touched[10];
} TagImpactState;
typedef struct TagImpactContact {
    s32 actor;f32 distance;EftDetVec hitPos,contact;
} TagImpactContact;
static TagImpactState sTagImpactState[128];
static u32 sTagImpactFrame=1;

void EftDet_TagImpactReset(void){
    memset(sTagImpactState,0,sizeof(sTagImpactState));sTagImpactFrame=1;
}
void EftDet_TagImpactForget(void *task){
    s32 i;
    for(i=0;i<128;++i)if(sTagImpactState[i].task==task){
        memset(&sTagImpactState[i],0,sizeof(sTagImpactState[i]));return;
    }
}
static TagImpactState *TagImpact_Get(EftDetRec *rec){
    s32 i,free=-1,oldest=0;
    TagImpactState *s;
    for(i=0;i<128;++i){
        s=&sTagImpactState[i];
        if(s->task==rec->task&&s->owner==rec->objId){s->seen=sTagImpactFrame;return s;}
        if(!s->task&&free<0)free=i;
        if(s->seen<sTagImpactState[oldest].seen)oldest=i;
    }
    s=&sTagImpactState[free>=0?free:oldest];memset(s,0,sizeof(*s));
    s->task=rec->task;s->owner=rec->objId;s->seen=sTagImpactFrame;return s;
}
static s32 TagImpact_Actor(s32 obj){
    s32 i;for(i=0;i<TagBattle_Count();++i)if(BattleSide_GetObjId(i)==obj)return i;
    return -1;
}
static void EftDet_TagFighterPass(void){
    EftDetList *list=EftHit_GetList();s32 i;
    if(++sTagImpactFrame==0)EftDet_TagImpactReset();
    for(i=0;i<list->count;++i){
        EftDetRec *rec=&list->rec[i];EftDetShape base;
        TagImpactContact contacts[10],contact;
        TagImpactTask *task=rec->task;TagImpactState *state=NULL;
        s32 owner=TagImpact_Actor(rec->objId),n=0,k,j,multi,area,flags,lastFlags;
        s32 primary,pierce,primaryContact=0,terminal=0;
        s32 savedFrames,savedHits,maxFrames,maxHits;
        u32 events;
        if(owner<0||!task||!EftHit_CanHit(rec,1)||rec->shape.type<0||rec->shape.type>1)continue;
        base=rec->shape;
        for(k=0;k<TagBattle_Count();++k){
            EftDetObj *obj;s32 hit;
            if(k==owner||TagBattle_Allied(owner,k)||!TagBattle_CanInteract(owner,k)||
               TagBattle_IsActorLoading(k)||BtlCharApi_GetHp(k)<=0)continue;
            obj=BtlObj_Get(BattleSide_GetObjId(k));
            if(!obj||!obj->work||!obj->parts)continue;
            rec->shape=base;
            hit=base.type==0?EftDet_SpheresVsFighter(obj,&rec->shape):EftDet_CapsuleVsFighter(obj,&rec->shape);
            if(!hit||((base.hitFlags&EFT_DET_HIT_STAGE)&&rec->shape.dist>=base.stageDist))continue;
            contact.actor=k;contact.distance=rec->shape.dist;
            contact.hitPos=rec->shape.hitPos;contact.contact=rec->shape.contact;
            /* Resolve actual trajectory order, independent of roster ordering. */
            for(j=n;j>0&&contacts[j-1].distance>contact.distance;--j)contacts[j]=contacts[j-1];
            contacts[j]=contact;++n;
        }
        rec->shape=base;
        if(!n)continue;
        multi=EftHit_IsMultiHit(rec);
        if(getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_IMPACT_TRACE")&&owner==0)
            fprintf(stderr,"tag impact: frame=%u record=%d type=%d shape=%d class=%d kind=%d multi=%d contacts=%d flags=%X\n",
                sTagImpactFrame,i,rec->type,rec->shape.type,rec->src?rec->src->def->cls:-1,
                rec->src?rec->src->def->kind:-1,multi,n,task->flags);
        /* Strike class 0, kind 4 is the native expanding radial wave. */
        area=multi||(rec->src&&rec->src->def->cls==0&&rec->src->def->kind==4)||(rec->src&&rec->src->def->cls!=0&&
            (rec->shape.type==1||!EftHit_IsStoppedByHit(rec)));
        primary=TagBattle_Target(owner);
        pierce=multi&&rec->src&&rec->src->def->cls!=0&&rec->shape.type==1;
        if(multi)state=TagImpact_Get(rec);
        savedFrames=maxFrames=task->frames;savedHits=maxHits=task->hits;
        flags=lastFlags=rec->flags;events=task->flags;
        for(k=0;k<n;++k){
            s32 actor=contacts[k].actor,objId=BattleSide_GetObjId(actor);
            if(getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_IMPACT_TRACE")&&owner==0&&actor==3){
                EftDetSphere *sphere=base.type==0?base.b:base.a;
                EftDetVec point;BtlCharApi_GetBasePos(objId,&point);
                fprintf(stderr,"tag impact secondary: shape=%d radius=%.2f center=%.2f,%.2f,%.2f actor=%.2f,%.2f,%.2f hp=%d\n",base.type,sphere->radius,sphere->pos.x,sphere->pos.y,sphere->pos.z,point.x,point.y,point.z,BtlCharApi_GetHp(actor));
            }
            if(!TagBattle_CanInteract(owner,actor))continue;
            if(state&&state->touched[actor]==sTagImpactFrame)continue;
            rec->shape=base;rec->shape.hitFlags|=EFT_DET_HIT_FIGHTER;
            rec->shape.dist=contacts[k].distance;rec->shape.hitPos=contacts[k].hitPos;
            rec->shape.contact=contacts[k].contact;rec->flags=flags;
            if(state){task->frames=(s8)state->frames[actor];task->hits=(s8)state->hits[actor];}
            EftHit_MarkLastHit(rec);
            if(BtlColl_TryDodge(objId,rec))continue;
            if(state)state->touched[actor]=sTagImpactFrame;
            if(BtlColl_TryDeflect(objId,rec)){
                EftHit_SetTaskFlag(i,0x40,rec->shape.contact);events|=task->flags;terminal=1;break;
            }else if(BtlColl_TryReflect(objId,rec)){
                EftHit_SetTaskFlag(i,0x80,rec->shape.contact);events|=task->flags;terminal=1;break;
            }else if(BtlColl_TryAbsorb(objId,rec)){
                EftHit_SetTaskFlag(i,0x20,rec->shape.contact);events|=task->flags;terminal=1;break;
            }else if(multi)EftDet_HitFighterMulti(rec,objId,i);
            else EftDet_HitFighter(rec,objId,i);
            if(state){
                state->frames[actor]=(u8)task->frames;state->hits[actor]=(u8)task->hits;
                if(task->frames>maxFrames)maxFrames=task->frames;
                if(task->hits>maxHits)maxHits=task->hits;
            }
            lastFlags|=rec->flags;events|=task->flags;
            if(actor==primary&&(task->flags&0x8003))primaryContact=1;
            if(!area&&(task->flags&3))break;
        }
        rec->flags=lastFlags;task->flags|=events;
        if(pierce&&!terminal){
            /* A secondary body takes its own native hits, but must not pin the
             * moving beam head in front of the selected opponent. The native
             * contact event clamps that head on the next post-update. Its hit
             * budget therefore belongs to the selected victim, not the maximum
             * over every victim encountered along the path. Stage/clash/defense
             * events and each victim's damage cadence remain untouched. */
            if(!primaryContact)task->flags&=~0x8000;
            task->frames=primary>=0&&primary<10?(s8)state->frames[primary]:(s8)savedFrames;
            task->hits=primary>=0&&primary<10?(s8)state->hits[primary]:(s8)savedHits;
        }
        else if(state){task->frames=(s8)maxFrames;task->hits=(s8)maxHits;}
        else {task->frames=(s8)savedFrames;task->hits=(s8)savedHits;}
    }
}
