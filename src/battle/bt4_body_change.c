/* Ginyu's two-body transaction. Native simulation state, included in rollback. */
#ifdef PORT
#include "common.h"
#include "battle/btl_char_mgr.h"
#include "battle/bt4_body_change.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

enum { BODY_IDLE, BODY_WAIT, BODY_REQUEST, BODY_LOADING, BODY_READY, BODY_LOADED, BODY_DONE };
typedef struct Bt4BodyChange {
    s32 state,peer,member;
    s32 chara,costume,variant,health,healthMax;
    s32 ki,blast;
} Bt4BodyChange;
static Bt4BodyChange sBody[10];

extern s32 BtlChar_GetCount(void),BtlAct_GetCurrent(BtlMgrChr *);
extern BtlMgrChr *BtlChar_Get(s32);
extern BtlMgrMember *BtlMember_GetActive(BtlMgrChr *);
extern void BtlAct_Request(BtlMgrChr *,s32),BtlAct_SetFormCurrent(BtlMgrChr *),BtlAct_ClearQueue(BtlMgrChr *);
extern void BtlChar_SavePlacement(BtlMgrChr *),BtlMember_ClearQueuedDamage(BtlMgrChr *);
extern void BtlChar_SetFlag(BtlMgrChr *,s32),BtlChar_ClearFlag(BtlMgrChr *,s32);
extern s32 BtlChange_RequestBody(s32,s32,s32,s32);
extern s32 BtlChange_IsLoadedFor(s32),BtlChange_IsBodyFor(s32);
extern s32 BtlChange_PendingFor(s32);
extern void BtlChange_SetReady(s32),BtlChange_SetDone(s32);
extern void BtlAnim_Request(BtlMgrChr *,s32,f32);
extern void BtlObj_BindCommonTables(BtlMgrObj *);
extern BtlMgrObj *BtlChar_GetObj(BtlMgrChr *);
extern void BtlMember_LoadParams(BtlMgrChr *,s32,s32,f32,s32);
extern s32 BtlParam_GetAuraKind(BtlMgrChr *);
extern void BtlObj_SetColorPreset(BtlMgrObj *,s32,s32);

static s32 body_rush(s32 action){return (action>=0x12D&&action<=0x12F)||(action>=0x139&&action<=0x13B);}
static s32 body_valid(s32 actor){return actor>=0&&actor<BtlChar_GetCount()&&actor<10;}
static s32 body_clamp(s32 n,s32 max){return n<0?0:n>max?max:n;}
void Bt4BodyChange_Reset(void){memset(sBody,0,sizeof(sBody));}
s32 Bt4BodyChange_Active(s32 actor){return body_valid(actor)&&sBody[actor].state!=BODY_IDLE;}

s32 Bt4BodyChange_HideModel(s32 object) {
    for (s32 actor=0;actor<(s32)(sizeof(sBody)/sizeof(sBody[0]));++actor) {
        const Bt4BodyChange *p=&sBody[actor];
        if (p->state>=BODY_LOADED &&
            (BtlChar_Get(actor)->objId==object || BtlChar_Get(p->peer)->objId==object)) return 1;
    }
    return 0;
}
void Bt4BodyChange_Queue(s32 attacker,s32 victim){
    if(!body_valid(attacker)||!body_valid(victim)||attacker==victim||sBody[attacker].state||sBody[victim].state)return;
    sBody[attacker].state=sBody[victim].state=BODY_WAIT;
    sBody[attacker].peer=victim;sBody[victim].peer=attacker;
    sBody[attacker].member=BtlChar_Get(attacker)->curMember;
    sBody[victim].member=BtlChar_Get(victim)->curMember;
}
static void body_capture(s32 actor,s32 target){
    BtlMgrChr *chr=BtlChar_Get(actor),*other=BtlChar_Get(target);
    BtlMgrMember *m=BtlMember_GetActive(chr),*body=BtlMember_GetActive(other);
    Bt4BodyChange *p=&sBody[actor];
    p->chara=body->chara;p->costume=body->costume;p->variant=body->gauge.unk20;
    p->health=body->gauge.health;p->healthMax=body->gauge.healthMax;
    p->ki=m->gauge.ki;p->blast=m->gauge.blast;
}

static void body_protect(BtlMgrChr *chr) {
    for (s32 bit=0x42;bit<=0x46;++bit) BtlChar_SetFlag(chr,bit);
    BtlChar_SetFlag(chr,0x136);
    BtlChar_SetFlag(chr,0xBB);
    BtlChar_SetFlag(chr,0x128);
}

static void body_begin(BtlMgrChr *chr) {
    BtlChar_SavePlacement(chr);
    BtlMember_ClearQueuedDamage(chr);
    BtlAct_ClearQueue(chr);
    BtlChar_ClearFlag(chr,6);
    BtlChar_ClearFlag(chr,0x125);
    chr->freeze=chr->freezeNext=chr->freezeDelay=0;
    BtlAct_Request(chr,0x103);
    body_protect(chr);
}

static void body_advance(s32 actor) {
    Bt4BodyChange *p=&sBody[actor];
    BtlMgrChr *chr=BtlChar_Get(actor);
    body_protect(chr);
    if(p->state==BODY_REQUEST){
        chr->newChara=p->chara;
        chr->newCostume=p->costume;
        chr->new20=p->variant;
        if(BtlChange_RequestBody(actor,p->chara,p->costume,p->variant))p->state=BODY_LOADING;
    }else if(p->state==BODY_LOADING&&BtlChange_IsBodyFor(actor)&&BtlChange_IsLoadedFor(actor)){
        BtlChange_SetReady(actor);
        BtlAnim_Request(chr,0,0.0f);
        p->state=BODY_READY;
    }else if(p->state==BODY_LOADED){
        BtlChange_SetDone(actor);
        p->state=BODY_DONE;
    }
}

static void body_apply(BtlMgrChr *chr, const Bt4BodyChange *p) {
    BtlMgrMember *m=BtlMember_GetActive(chr);
    BtlMgrGauge *g=&m->gauge;
    BtlMgrObj *obj=BtlChar_GetObj(chr);
    m->chara=p->chara;
    m->costume=p->costume;
    g->unk20=p->variant;
    BtlObj_BindCommonTables(obj);
    BtlMember_LoadParams(chr,chr->curMember,0,0.0f,0);
    g->healthMax=p->healthMax;
    g->health=body_clamp(p->health,p->healthMax);
    g->ki=body_clamp(p->ki,g->kiMax);
    g->blast=body_clamp(p->blast,g->blastMax);
    g->unk1C=0;
    g->lowHealth=g->health<10000;
    g->unk30=1;
    BtlAct_SetFormCurrent(chr);
    BtlObj_SetColorPreset(obj,BtlParam_GetAuraKind(chr),-1);
    obj->flags=(obj->flags & ~0x40000000u) | (p->variant ? 0x40000000u : 0);
    if(getenv("BT4_RENDER_TEST")&&getenv("BT4_BODY_CHANGE_TRACE"))
        fprintf(stderr,"body exchange: applied actor=%d char=%d costume=%d hp=%d max=%d\n",
                chr->side,m->chara,m->costume,g->health,g->healthMax);
}

static void body_finish(BtlMgrChr *chr) {
    BtlAct_ClearQueue(chr);
    chr->freeze=chr->freezeNext=chr->freezeDelay=0;
    for (s32 bit=0x42;bit<=0x46;++bit) BtlChar_ClearFlag(chr,bit);
    BtlChar_ClearFlag(chr,0x136);
    BtlChar_ClearFlag(chr,0xBB);
    BtlChar_ClearFlag(chr,0x128);
    BtlChar_SetFlag(chr,0xF5);
    BtlChar_SetFlag(chr,0x55);
    BtlAct_Request(chr,0xB);
}

void Bt4BodyChange_Update(void){
    s32 a,b;
    for(a=0;a<BtlChar_GetCount()&&a<10;++a){
        Bt4BodyChange *p=&sBody[a];b=p->peer;
        if(!p->state||!body_valid(b)||b<=a)continue;
        if(p->state==BODY_WAIT&&sBody[b].state==BODY_WAIT){
            BtlMgrChr *left=BtlChar_Get(a),*right=BtlChar_Get(b);
            if(left->curMember!=p->member||right->curMember!=sBody[b].member||
               BtlMember_GetActive(left)->gauge.health<=0||BtlMember_GetActive(right)->gauge.health<=0){
                memset(p,0,sizeof(*p));memset(&sBody[b],0,sizeof(sBody[b]));continue;
            }
            if(body_rush(BtlAct_GetCurrent(left))||body_rush(BtlAct_GetCurrent(right))||
               BtlChange_PendingFor(a)||BtlChange_PendingFor(b))continue;
            /* Capture both before either record or model is changed. */
            body_capture(a,b);body_capture(b,a);
            if(getenv("BT4_RENDER_TEST")&&getenv("BT4_BODY_CHANGE_TRACE"))
                fprintf(stderr,"body exchange: begin actors=%d,%d targets=%d,%d hp=%d,%d max=%d,%d\n",a,b,p->chara,sBody[b].chara,p->health,sBody[b].health,p->healthMax,sBody[b].healthMax);
            p->state=sBody[b].state=BODY_REQUEST;
            body_begin(left);body_begin(right);
        }else if(p->state>=BODY_REQUEST&&sBody[b].state>=BODY_REQUEST){
            /* The loader has one owner at a time. Advancing it here still
               finishes both halves if an action was interrupted or changed. */
            body_advance(a);
            body_advance(b);
            if(p->state!=BODY_DONE||sBody[b].state!=BODY_DONE)continue;
            /* Both model and move banks are loaded before either player sees
               a new identity or has control restored. */
            body_apply(BtlChar_Get(a),p);
            body_apply(BtlChar_Get(b),&sBody[b]);
            body_finish(BtlChar_Get(a));
            body_finish(BtlChar_Get(b));
            if(getenv("BT4_RENDER_TEST")&&getenv("BT4_BODY_CHANGE_TRACE"))fprintf(stderr,"body exchange: completed actors=%d,%d\n",a,b);
            memset(p,0,sizeof(*p));memset(&sBody[b],0,sizeof(sBody[b]));
        }
    }
}
void Bt4BodyChange_Action(void *fighter,s32 phase){
    if (phase == 1) body_protect(fighter);
}
s32 Bt4BodyChange_OnModelLoaded(void *fighter){
    BtlMgrChr *chr=fighter;Bt4BodyChange *p;
    if(!body_valid(chr->side))return 0;
    p=&sBody[chr->side];
    if(p->state!=BODY_READY||!BtlChange_IsBodyFor(chr->side))return 0;
    if(getenv("BT4_RENDER_TEST")&&getenv("BT4_BODY_CHANGE_TRACE"))
        fprintf(stderr,"body exchange: loaded actor=%d char=%d costume=%d hp=%d max=%d\n",
                chr->side,p->chara,p->costume,p->health,p->healthMax);
    p->state=BODY_LOADED;return 1;
}
#endif
