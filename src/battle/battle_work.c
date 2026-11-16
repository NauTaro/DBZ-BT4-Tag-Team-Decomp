#include "common.h"
#ifdef PORT
#include "plat_songs.h"
#endif
#include "battle/battle_work.h"
#include "battle/battle_setup.h"
#include "sys/math3d.h"
#include "battle/tag_team_native.h"
#include <stdlib.h>
#include <stdio.h>
#include "sys/adx.h"
#include "sys/loading.h"

/* Battle work: the static BattleWork block and its accessors, reset, per-frame update, load / unload entry points.
 * 0x126EC8..0x127120. The loader itself is src/battle/battle_load.c (0x127120 onward).
 *
 * The two files are separate translation units: when Battle_GetWork (0x126EC8) or Battle_GetEventWork (0x127028)
 * is defined in the same file as the loader job step functions, ee-gcc 2.96 fills their branch delay slots
 * differently (e.g. `beq; nop; b end; li v0,1` instead of the original `beq; li v0,1; b end+4; ld s0`, and
 * `beqzl` instead of `beqz` in front of the Battle_GetEventWork call) and four of them cannot match. Where
 * exactly the boundary lies between 0x127048 and 0x1272B0 is not provable from the code (every function in
 * between matches on either side); 0x127120, the first function that is not a Battle_* entry point, was chosen. */

extern void *memset(void *dst, s32 c, u32 n);

extern BattleWork gBattleWork;
extern s32 gBtlScriptCommands[]; /* table handed to Gsc_InitDefault */

extern s32 BtlObj_CreateChara(s32 id);
extern void BtlObj_Init(s32 arg);
extern void BtlObj_Term(void);
extern void Gsc_InitDefault(s32 *tbl);
extern void Gsc_Exit(void);
extern s32 Gsc_StartMain(s32 script);
extern void BtlScript_Init(void);
extern void BtlScript_Restart(void);
extern void BtlScript_Free(void);
static s32 sTagActors = 2;
static s32 sTagFreeForAll,sTagWinningActor=-1;
s32 TagBattle_FreeForAll(void){return sTagFreeForAll && TagBattle_Active();}
s32 TagBattle_Allied(s32 a,s32 b){return sTagFreeForAll?a==b:TagBattle_Team(a)==TagBattle_Team(b);}
s32 TagBattle_WinningActor(void){return sTagWinningActor;}
void TagBattle_SetWinningActor(s32 actor){sTagWinningActor=actor;}
static BattleSide sTagSides[8];
/* Retain each seat's actual set, rather than the reused movie slots. Online
   sets come only from the authoritative draft, never another machine's saves. */
static BattleItemSet sTagDraftItems[10];
static s32 sTagDraftItemChara[10];
void TagBattle_ClearDraftItems(void) {
    s32 i;
    memset(sTagDraftItems, 0, sizeof(sTagDraftItems));
    for (i = 0; i < 10; ++i) sTagDraftItemChara[i] = -1;
}
void TagBattle_SetDraftItems(s32 actor, s32 chara, const u16 *ids) {
    s32 i;
    if (actor < 0 || actor >= 10) return;
    sTagDraftItemChara[actor] = chara;
    for (i = 0; i < 8; ++i)
        sTagDraftItems[actor].id[i] = ids && ids[i] <= 350 ? ids[i] : 0;
}
BattleItemSet *TagBattle_GetDraftItems(s32 actor, s32 chara) {
    extern int Port_TagNetwork(void);
    if(Port_TagNetwork()){
        extern const u16 *Port_TagPlayerItems(int);const u16 *items=Port_TagPlayerItems(actor);
        if(!items||actor<0||actor>=10)return NULL;memcpy(sTagDraftItems[actor].id,items,16);return &sTagDraftItems[actor];
    }
    if (actor < 0 || actor >= 10 ||
        sTagDraftItemChara[actor] != chara) return NULL;
    return &sTagDraftItems[actor];
}
#ifdef BT4_TAG_TEAM_EXPANDED
static s32 sTagFocus[10], sTagCameraMode[10], sTagCounterTarget[10];
static u32 sTagCombat[10];
s32 TagBattle_DevCombatActive(void){
    extern int Port_TagRulesV2(void),Port_TagNetwork(void);
    return TagBattle_Active()&&(!Port_TagNetwork()||Port_TagRulesV2());
}
s32 TagBattle_Boss(s32 actor){
    return TagBattle_DevCombatActive()&&actor>=0&&actor<10&&(sTagCombat[actor]&(1u<<18));
}
s32 TagBattle_CombatDamage(s32 attacker,s32 victim,s32 damage){
    s64 scaled;
    u32 outgoing,incoming;
    if(!TagBattle_DevCombatActive()||attacker<0||attacker>=10||victim<0||
       victim>=10||attacker==victim||damage<=0)return damage;
    outgoing=sTagCombat[attacker]&511;
    incoming=(sTagCombat[victim]>>9)&511;
    if(!outgoing)outgoing=100;
    if(!incoming)incoming=100;
    scaled=((s64)damage*outgoing*incoming+5000)/10000;
    return scaled>2147483647?2147483647:scaled<1?1:(s32)scaled;
}
#endif
static s32 sTagLocal, sTagTargets[10],sTagTeams[10]={0,1,0,1,0,1,0,1,0,1};
s32 TagBattle_Active(void){return sTagLocal || sTagActors>2;}
s32 TagBattle_Team(s32 a){return a>=0&&a<sTagActors?sTagTeams[a]:a&1;}
s32 TagBattle_TeamIndex(s32 a){s32 i,n=0;for(i=0;i<a;++i)if(TagBattle_Team(i)==TagBattle_Team(a))++n;return n;}
s32 TagBattle_TeamCount(s32 t){s32 i,n=0;for(i=0;i<sTagActors;++i)if(TagBattle_Team(i)==t)++n;return n;}
static s32 TagBattle_FirstEnemy(s32 a){
    s32 i,index=0,n=sTagFreeForAll?sTagActors-1:TagBattle_TeamCount(1-TagBattle_Team(a));
    s32 wanted=n?TagBattle_TeamIndex(a)%n:0;
    for(i=0;i<sTagActors;++i)if(!TagBattle_Allied(i,a)&&index++==wanted)return i;
    return a^1;
}

static s32 sTagPairs[10],sTagPairGrace[10],sTagAttackers[10],sTagDamageTargets[10],sTagPresentation,sTagReturn;
static s32 sTagStage,sTagBgm=2;
static s32 sTagViewActor,sTagLocalActor,sTagTestView=-1;
#ifdef BT4_TAG_TEAM_EXPANDED
static s32 sTagCpuFormsDisabled;
static struct {
    s32 actor;
    f32 yaw, pitch;
} sTagSplitView[2];
#endif
s32 TagBattle_LocalActor(void){return sTagLocalActor;}
extern s32 BtlCharApi_GetHp(s32 actor);
s32 TagBattle_ViewActor(void){
    s32 view=TagBattle_Active()?(sTagTestView>=0?sTagTestView:sTagViewActor):0;
#ifdef BT4_TAG_TEAM_EXPANDED
    extern s32 TagFusion_ViewActor(s32);view=TagFusion_ViewActor(view);
#endif
    return view;
}
extern int Port_TagOfflineMode(void),Port_TagOfflineBeans(void),Port_TagOfflineCustom(void);
#ifdef BT4_TAG_TEAM_EXPANDED
s32 TagBattle_AllowsTransformation(s32 actor) {
    extern s32 TagFusion_Controller(s32 actor);
    if (!sTagCpuFormsDisabled || actor < 0 || actor >= sTagActors ||
        !(sTagCpuFormsDisabled & (1 << TagBattle_Team(actor)))) return 1;
    return BattleSide_GetControl(TagFusion_Controller(actor)) != BTL_CONTROL_CPU;
}

static s32 TagBattle_SplitLocal(void) {
    return TagBattle_Active() && Port_TagOfflineMode() == 2;
}

s32 TagBattle_SplitSpectating(s32 player) {
    extern s32 TagFusion_Absent(s32);
    return TagBattle_SplitLocal() && !TagFusion_Absent(player) && BtlCharApi_GetHp(player) <= 0;
}

static s32 TagBattle_SplitLiveAlly(s32 player, s32 actor) {
    extern s32 TagFusion_Absent(s32);
    return actor != player && TagBattle_Allied(actor, player) &&
           !TagFusion_Absent(actor) && BtlCharApi_GetHp(actor) > 0;
}

s32 TagBattle_SplitViewActor(s32 player) {
#ifdef PORT
    extern s32 Port_PresentationSplitDrawing(s32),Port_PresentationSplitActor(s32);
    if(Port_PresentationSplitDrawing(-1))return Port_PresentationSplitActor(player);
#endif
    extern s32 TagFusion_ViewActor(s32);
    s32 actor = sTagSplitView[player].actor;
    if (TagBattle_SplitSpectating(player) && TagBattle_SplitLiveAlly(player, actor))
        return actor;
    return TagFusion_ViewActor(player);
}

void TagBattle_UpdateSplitSpectator(s32 player, s32 direction, f32 yaw, f32 pitch) {
    s32 actor = TagBattle_SplitViewActor(player);
    if (!TagBattle_SplitSpectating(player)) {
        sTagSplitView[player].actor = player;
        sTagSplitView[player].yaw = sTagSplitView[player].pitch = 0;
        return;
    }
    if (direction) {
        for (s32 i = 1; i < sTagActors; ++i) {
            s32 next = (actor + direction * i + sTagActors) % sTagActors;
            if (next == player || TagBattle_SplitLiveAlly(player, next)) {
                extern void Port_SeTagCue(s32);
                actor = next;
                Port_SeTagCue(direction);
                break;
            }
        }
    }
    sTagSplitView[player].actor = actor;
    if (actor == player) {
        sTagSplitView[player].yaw += yaw * .055f;
        if (sTagSplitView[player].yaw > 3.14159265f) sTagSplitView[player].yaw -= 6.28318531f;
        if (sTagSplitView[player].yaw < -3.14159265f) sTagSplitView[player].yaw += 6.28318531f;
        sTagSplitView[player].pitch += pitch * .035f;
        if (sTagSplitView[player].pitch > .65f) sTagSplitView[player].pitch = .65f;
        if (sTagSplitView[player].pitch < -.65f) sTagSplitView[player].pitch = -.65f;
    }
}

s32 TagBattle_SplitOrbit(s32 player, f32 *yaw, f32 *pitch) {
    *yaw = sTagSplitView[player].yaw;
    *pitch = sTagSplitView[player].pitch;
    return TagBattle_SplitSpectating(player) && TagBattle_SplitViewActor(player) == player &&
           (*yaw != 0 || *pitch != 0);
}
#endif
static TagHealthRule sTagHealthByTeam[2][5], sTagHealthByActor[10];

void TagBattle_SetLocalHealth(const TagHealthRule rules[2][5]) {
    memcpy(sTagHealthByTeam, rules, sizeof(sTagHealthByTeam));
}

const TagHealthRule *TagBattle_HealthRule(s32 actor) {
    if (!sTagLocal || (!Port_TagOfflineMode()&&!Port_TagNetwork()) || actor < 0 || actor >= sTagActors) return NULL;
    const TagHealthRule *rule = &sTagHealthByActor[actor];
    return rule->multiplier > 1 || rule->maximum || rule->starting ||
           (rule->percent > 0 && rule->percent < 100) || rule->frozen ? rule : NULL;
}
s32 TagBattle_Spectating(void){
    const s32 offline = Port_TagOfflineMode();
#ifdef BT4_TAG_TEAM_EXPANDED
    extern s32 TagFusion_Absent(s32),TagFusion_Controller(s32);
    if((offline==1||Port_TagNetwork()) && TagFusion_Absent(sTagLocalActor) && TagFusion_Controller(sTagLocalActor)==sTagLocalActor)return 0;
#endif
    return TagBattle_Active() && (offline == 3 || (offline != 2 && BtlCharApi_GetHp(sTagLocalActor) <= 0 &&
           (offline == 1 || sTagViewActor != sTagLocalActor)));
}
void TagBattle_UpdateSpectator(s32 dir){
    s32 old=sTagViewActor,i,start;
    const s32 offline = Port_TagOfflineMode();
    if(offline==2)return;
#ifdef BT4_TAG_TEAM_EXPANDED
    {extern s32 TagFusion_Busy(s32),TagFusion_ViewActor(s32);
     if(TagFusion_Busy(sTagLocalActor)&&TagFusion_ViewActor(sTagLocalActor)!=sTagLocalActor){sTagViewActor=TagFusion_ViewActor(sTagLocalActor);return;}}
#endif
    if(offline!=3&&BtlCharApi_GetHp(sTagLocalActor)>0){sTagViewActor=sTagLocalActor;return;}
    start=sTagViewActor;
    if ((offline == 1||Port_TagNetwork()) && !dir) {
        if (BtlCharApi_GetHp(start) <= 0) sTagViewActor = sTagLocalActor;
        return;
    }
    if(dir || BtlCharApi_GetHp(start)<=0){
        if(!dir)dir=1;
        for(i=1;i<=sTagActors;++i){s32 a=(start+dir*i+sTagActors*2)%sTagActors;
            if ((offline == 3 || sTagFreeForAll || TagBattle_Allied(a, sTagLocalActor)) &&
                (BtlCharApi_GetHp(a) > 0 || ((offline == 1||Port_TagNetwork()) && a == sTagLocalActor))) {
                sTagViewActor = a;
                break;
            }
        }
    }
    if(old!=sTagViewActor){
        extern void Port_SeTagCue(s32);
        if(dir)Port_SeTagCue(dir);
        if(getenv("BT4_RENDER_TEST"))fprintf(stderr,"tag spectator: from=%d viewing=%d alive=%d\n",old,sTagViewActor,BtlCharApi_GetHp(sTagViewActor)>0);
    }
}
void TagBattle_SetArena(s32 stage,s32 bgm){sTagStage=stage;sTagBgm=bgm;}
s32 TagBattle_Presentation(void){return 1;}
/* Rush cameras are private to their actual participants in both styles.
 * A selected target is not a victim until the collision reserves a pair. */
s32 TagBattle_RushAction(s32 actor){
    extern void *BtlChar_Get(s32);
    extern s32 BtlAct_GetCurrent(void *),BtlAct_GetQueued(void *);
    s32 action=BtlAct_GetCurrent(BtlChar_Get(actor));
    if(action==0x105)action=BtlAct_GetQueued(BtlChar_Get(actor));
    return (action>=0x11B && action<=0x12F)||(action>=0x133 && action<=0x13B);
}
s32 TagBattle_Preparing(s32 actor){
    extern void *BtlChar_Get(s32);extern s32 BtlAct_GetCurrent(void *),BtlChar_TestFlag(void *,s32);
    extern s32 BtlAnim_GetId(void *);
    void *chr; s32 a,m;
    if(!TagBattle_Active()||actor<0||actor>=sTagActors||TagBattle_Pair(actor)>=0||TagBattle_RushAction(actor))return 0;
    chr=BtlChar_Get(actor);a=BtlAct_GetCurrent(chr);m=BtlAnim_GetId(chr);
    /* Transformation, fusion and member-change animations belong to their
       actor. Their repeated native level-2 stop cannot stall the target. */
    if(a>=0xEC&&a<=0xF8)return 1;
#ifdef BT4_TAG_TEAM_EXPANDED
    /* A cooperative fusion protects its two components, not their enemy's
     * input. Its native transformation hit-stop must stay private. */
    {extern s32 TagFusion_Busy(s32);if((a==0xF1||a==0xF2)&&TagFusion_Busy(actor))return 1;}
#endif
    if(a<0xFD||a>0x129)return 0;
    return a==0x105||BtlChar_TestFlag(chr,0x125)||m==0x105||m==0x106||m==0x127||m==0x128||m==0x149||m==0x14A;
}
s32 TagBattle_PairAttacker(s32 actor){return TagBattle_Pair(actor)>=0?sTagAttackers[actor]:-1;}
s32 TagBattle_DamageTarget(s32 actor){return TagBattle_Active()&&actor>=0&&actor<sTagActors?sTagDamageTargets[actor]:-1;}
void TagBattle_ClearDamageTarget(s32 actor){if(actor>=0&&actor<sTagActors)sTagDamageTargets[actor]=-1;}
s32 TagBattle_CameraActor(s32 viewer){
    extern void *BtlChar_Get(s32);extern s32 BtlAct_GetCurrent(void *);
    extern s32 TagBattle_SharedCutOwner(void);
    const s32 shared=TagBattle_SharedCutOwner();
    if(shared>=0&&shared<sTagActors)return shared;
    s32 pair=TagBattle_Pair(viewer),owner=TagBattle_DefaultCutOwner();
    if(pair>=0){
        s32 a=BtlAct_GetCurrent(BtlChar_Get(viewer)),b=BtlAct_GetCurrent(BtlChar_Get(pair));
        if((a>=0x12A&&a<=0x13B)||(b>=0x12A&&b<=0x13B))return sTagAttackers[viewer];
    }
    /* An unrelated fighter's preparation never takes this actor's view. */
    if(owner==viewer)return owner;
    /* Only a reserved rush victim shares the attacker's impact camera.
     * Projectile damage alone must not transfer ownership of the view. */
    return viewer;
}
s32 TagBattle_CutVisible(s32 actor,s32 viewer){
    return actor>=0&&actor<sTagActors&&actor==TagBattle_CameraActor(viewer);
}
void TagBattle_SetReturn(s32 choice){sTagReturn=choice;}
s32 TagBattle_TakeReturn(void){s32 r=sTagReturn;sTagReturn=0;return r;}
void TagBattle_ResetPairs(void){
    s32 i;
    sTagViewActor=sTagLocalActor;
#ifdef BT4_TAG_TEAM_EXPANDED
    memset(sTagSplitView, 0, sizeof(sTagSplitView));
    sTagSplitView[1].actor = 1;
    for(i=0;i<10;++i){sTagFocus[i]=-1;sTagCameraMode[i]=0;sTagCounterTarget[i]=-1;}
#endif
    for(i=0;i<10;++i){sTagPairs[i]=sTagAttackers[i]=sTagDamageTargets[i]=-1;sTagPairGrace[i]=0;}
}
s32 TagBattle_Pair(s32 a){return TagBattle_Active() && a>=0 && a<sTagActors?sTagPairs[a]:-1;}
s32 TagBattle_CanInteract(s32 a,s32 b){
#ifdef BT4_TAG_TEAM_EXPANDED
    extern s32 TagFusion_Absent(s32);if(TagFusion_Absent(a)||TagFusion_Absent(b))return 0;
#endif
    return !TagBattle_Active() || ((TagBattle_Pair(a)<0 || TagBattle_Pair(a)==b) && (TagBattle_Pair(b)<0 || TagBattle_Pair(b)==a));
}
s32 TagBattle_ReservePair(s32 a,s32 b){
    if(!TagBattle_Active())return 1;
    if(a<0||b<0||a>=sTagActors||b>=sTagActors||TagBattle_Allied(a,b)||!TagBattle_CanInteract(a,b))return 0;
#ifdef BT4_TAG_TEAM_EXPANDED
    if(TagBattle_Boss(b))return 0;
#endif
    sTagPairs[a]=b;sTagPairs[b]=a;sTagPairGrace[a]=sTagPairGrace[b]=6;
    sTagAttackers[a]=sTagAttackers[b]=a;sTagDamageTargets[a]=b;sTagDamageTargets[b]=-1;
    {extern void *BtlChar_Get(s32);extern void BtlMember_ClearQueuedDamage(void *);
     BtlMember_ClearQueuedDamage(BtlChar_Get(b));}
    return 1;
}
static s32 TagBattle_PairedAction(s32 action){return (action>=0xB4&&action<=0xBF)||(action>=0x12D&&action<=0x132)||(action>=0x133&&action<=0x13B);}
void TagBattle_UpdatePairs(void){
    extern void *BtlChar_Get(s32 actor);
    extern s32 BtlAct_GetCurrent(void *chr);
    s32 a,b;
    for(a=0;a<sTagActors;++a){b=sTagPairs[a];if(b<=a||b>=sTagActors)continue;
        s32 attacker=sTagAttackers[a];
        {extern s32 Bt4BodyChange_Active(s32);
        if(attacker>=0&&(TagBattle_PairedAction(BtlAct_GetCurrent(BtlChar_Get(attacker)))||Bt4BodyChange_Active(attacker)))sTagPairGrace[a]=6;
        else if(--sTagPairGrace[a]<=0){
            extern void ChrCam_EndCut(void *);
            ChrCam_EndCut(BtlChar_Get(a));ChrCam_EndCut(BtlChar_Get(b));
            sTagPairs[a]=sTagPairs[b]=sTagAttackers[a]=sTagAttackers[b]=-1;sTagPairGrace[a]=sTagPairGrace[b]=0;
        }
        }
    }
}
extern s32 BtlCharApi_GetHp(s32 actor);
s32 TagBattle_Local(void) { return sTagLocal; }
s32 TagBattle_Target(s32 actor) {
    s32 target;
    if(!TagBattle_Active() || actor<0 || actor>=sTagActors)return actor^1;
    if(sTagPairs[actor]>=0)return sTagPairs[actor];
#ifdef BT4_TAG_TEAM_EXPANDED
    if(sTagCounterTarget[actor]>=0)return sTagCounterTarget[actor];
#endif
    target=sTagTargets[actor];
    if(target<0 || target>=sTagActors || TagBattle_Allied(target,actor))target=TagBattle_FirstEnemy(actor);
    return target;
}
#ifdef BT4_TAG_TEAM_EXPANDED
/* Focus is separate from the enemy slot used by damage, AI and cinematics. */
static s32 TagBattle_AllyFocusAvailable(s32 actor,s32 ally){
    extern s32 TagFusion_Absent(s32);
    extern s32 TagRevive_Hud(s32,s32 *,s32 *,s32 *);
    s32 progress,total,helpers;
    return ally>=0&&ally<sTagActors&&ally!=actor&&TagBattle_Allied(actor,ally)&&
        !TagFusion_Absent(ally)&&!TagBattle_IsActorLoading(ally)&&
        (BtlCharApi_GetHp(ally)>0||TagRevive_Hud(ally,&progress,&total,&helpers));
}

s32 TagBattle_CameraMode(s32 actor){
    return TagBattle_DevCombatActive()&&actor>=0&&actor<sTagActors?sTagCameraMode[actor]:0;
}

s32 TagBattle_Focus(s32 actor){
    if(!TagBattle_DevCombatActive()||actor<0||actor>=sTagActors||
       sTagPairs[actor]>=0||sTagCounterTarget[actor]>=0)return -1;
    if(sTagCameraMode[actor]==0)return -1;
    if(sTagCameraMode[actor]==1&&TagBattle_AllyFocusAvailable(actor,sTagFocus[actor]))return sTagFocus[actor];
    return -2;
}
static void TagBattle_SelectCamera(s32 actor,s32 mode){
    s32 i,best=-1;
    f32 distance=1.0e20f;
    Vec4 origin;
    extern void BtlCharApi_GetBasePos(s32,Vec4 *);
    extern s32 TagRevive_Hud(s32,s32 *,s32 *,s32 *);
    if(!TagBattle_DevCombatActive()||actor<0||actor>=sTagActors||sTagPairs[actor]>=0||sTagCounterTarget[actor]>=0)return;
    sTagCameraMode[actor]=mode;
    sTagFocus[actor]=-1;
    if(mode!=1)return;
    BtlCharApi_GetBasePos(BattleSide_GetObjId(actor),&origin);
    for(i=0;i<sTagActors;++i){
        Vec4 p;
        f32 x,y,z,d;
        s32 progress,total,helpers,downed;
        if(!TagBattle_AllyFocusAvailable(actor,i))continue;
        downed=TagRevive_Hud(i,&progress,&total,&helpers);
        BtlCharApi_GetBasePos(BattleSide_GetObjId(i),&p);
        x=p.x-origin.x;y=p.y-origin.y;z=p.z-origin.z;
        d=x*x+y*y+z*z+(downed?0.0f:100000000.0f);
        if(d<distance){distance=d;best=i;}
    }
    sTagFocus[actor]=best;
}
void TagBattle_StepCamera(s32 actor){
    TagBattle_SelectCamera(actor,(TagBattle_CameraMode(actor)+1)%3);
}
void TagBattle_ChangeFocus(s32 actor,s32 ally){
    const s32 mode=ally?1:2;
    TagBattle_SelectCamera(actor,TagBattle_CameraMode(actor)==mode?0:mode);
}
void TagBattle_FocusEnemy(s32 actor){
    TagBattle_SelectCamera(actor,0);
}
void TagBattle_CounterTarget(s32 actor,s32 attacker){
    if(!TagBattle_DevCombatActive()||actor<0||actor>=sTagActors||attacker<0||
       attacker>=sTagActors||TagBattle_Allied(actor,attacker))return;
    sTagTargets[actor]=sTagCounterTarget[actor]=attacker;
}
void TagBattle_ClearCounterTarget(s32 actor){
    if(actor>=0&&actor<sTagActors)sTagCounterTarget[actor]=-1;
}
#endif
s32 TagBattle_SetTarget(s32 actor,s32 target){
    extern void *BtlChar_Get(s32);extern s32 BtlAct_GetCurrent(void *);
    if(!TagBattle_Active()||actor<0||actor>=sTagActors||target<0||target>=sTagActors||
       TagBattle_Pair(actor)>=0||TagBattle_Allied(actor,target)||
       TagBattle_IsActorLoading(target)||BtlCharApi_GetHp(target)<=0)return 0;
    {s32 action=BtlAct_GetCurrent(BtlChar_Get(actor));if(action>=0xB4&&action<=0xBF)return 0;}
#ifdef BT4_TAG_TEAM_EXPANDED
    if(sTagCounterTarget[actor]>=0)return 0;
#endif
    sTagTargets[actor]=target;return 1;
}
void TagBattle_CycleTarget(s32 actor) {
    s32 i,old=TagBattle_Target(actor);
    extern void *BtlChar_Get(s32);extern s32 BtlAct_GetCurrent(void *);
    if(TagBattle_Pair(actor)>=0)return;
    if(actor<0||actor>=sTagActors)return;
    {s32 action=BtlAct_GetCurrent(BtlChar_Get(actor));if(action>=0xB4&&action<=0xBF)return;}
    for(i=1;i<=sTagActors;++i){s32 target=(old+i)%sTagActors;
        if(!TagBattle_Allied(target,actor) && !TagBattle_IsActorLoading(target) && BtlCharApi_GetHp(target)>0){
            if(!TagBattle_SetTarget(actor,target))return;
            if(actor==0 && target!=old && BtlCharApi_GetHp(old)<=0 && getenv("BT4_RENDER_TEST") && getenv("BT4_TAG_KO_TEST"))
                fprintf(stderr,"tag auto target: actor=0 old=%d target=%d\n",old,target);
            return;}}
}
static s32 TagBattle_DirectionalTarget(s32 actor,s32 direction,s32 ally,s32 target) {
    extern void BtlCharApi_GetPos(s32 objId,Vec4 *out);
    extern void BtlCharApi_GetCamPose(s32 objId,Vec4 *pos,Vec4 *rot);
    extern f32 sinf(f32),cosf(f32);
    Vec4 eye,rot,origin,p;f32 sy,cy,sp,cp,best=-1.0f;
    s32 i,chosen=-1;
    if(!TagBattle_Active()||actor<0||actor>=sTagActors||direction<1||direction>4||TagBattle_Pair(actor)>=0)return -1;
    BtlCharApi_GetCamPose(BattleSide_GetObjId(actor),&eye,&rot);
    BtlCharApi_GetPos(BattleSide_GetObjId(target),&origin);
    sy=sinf(rot.y);cy=cosf(rot.y);sp=sinf(rot.x);cp=cosf(rot.x);
    for(i=0;i<sTagActors;++i){
        f32 x,y,z,dx,dy,along,cross,dist,score;
        if(i==target)continue;
#ifdef BT4_TAG_TEAM_EXPANDED
        if(ally){if(!TagBattle_AllyFocusAvailable(actor,i))continue;}
        else
#endif
        if(TagBattle_Allied(actor,i)||TagBattle_IsActorLoading(i)||BtlCharApi_GetHp(i)<=0)continue;
        BtlCharApi_GetPos(BattleSide_GetObjId(i),&p);
        x=p.x-origin.x;y=p.y-origin.y;z=p.z-origin.z;
        dx=x*cy-z*sy;dy=-y*cp+(x*sy+z*cy)*sp;
        along=direction==1?dy:direction==2?-dy:direction==3?-dx:dx;
        cross=direction<=2?dx:dy;
        dist=along*along+cross*cross;
        if(along<=0.01f||dist<0.0001f)continue;
        score=along*along/dist;
        if(score<0.25f)continue;
        score-=dist*0.00000001f;
        if(score>best){best=score;chosen=i;}
    }
    return chosen;
}
s32 TagBattle_TargetDirection(s32 actor,s32 direction) {
    const s32 chosen=TagBattle_DirectionalTarget(actor,direction,0,TagBattle_Target(actor));
    return chosen>=0?TagBattle_SetTarget(actor,chosen):0;
}
#ifdef BT4_TAG_TEAM_EXPANDED
void TagBattle_NavigateCamera(s32 actor,s32 direction){
    if(!TagBattle_DevCombatActive()||actor<0||actor>=sTagActors||sTagPairs[actor]>=0||
       sTagCounterTarget[actor]>=0)return;
    const s32 mode=sTagCameraMode[actor];
    if(mode==0){
        if(direction)TagBattle_TargetDirection(actor,direction);
        else TagBattle_CycleTarget(actor);
    }else if(mode==1){
        const s32 focus=TagBattle_AllyFocusAvailable(actor,sTagFocus[actor])?sTagFocus[actor]:actor;
        if(direction){
            const s32 next=TagBattle_DirectionalTarget(actor,direction,1,focus);
            if(next>=0)sTagFocus[actor]=next;
        }else{
            for(s32 i=1;i<=sTagActors;++i){
                const s32 next=(focus+i)%sTagActors;
                if(TagBattle_AllyFocusAvailable(actor,next)){sTagFocus[actor]=next;break;}
            }
        }
    }
}
#endif
#include "tag_team_target_audit.inc"
s32 TagBattle_ConfigureLocal(void) {
    extern int Port_TagHumans(void),Port_TagCapacity(void),Port_TagConfirmed(int seat),Port_TagCPU(int seat);
    extern int Port_TagDifficulty(int seat),Port_TagPlayerChara(int seat),Port_TagPlayerCostume(int seat),Port_TagTime(void);
    static const s32 levels[5]={0,6,13,21,29};
    s32 i,count=Port_TagCapacity();
    extern int Port_TagTeam(int seat),Port_TagTeamCount(int team);
    extern int Port_TagNetwork(void),Port_TagSeat(void),Port_TagStage(void),Port_TagBgm(void),Port_TagAttacks(void);
    s32 network=Port_TagNetwork(),offline=Port_TagOfflineMode(),time=Port_TagTime();
    s32 timeLimit=offline?(time==4?0:time+1):time;
    if((!network&&Port_TagHumans()!=1) || count<2 || count>10 || !Port_TagTeamCount(0) || !Port_TagTeamCount(1))return 0;
    for(i=0;i<count;++i)if(!Port_TagConfirmed(i) || (!network && i!=Port_TagSeat() && !Port_TagCPU(i)))return 0;
    Battle_ClearWork();sTagActors=count;sTagLocal=1;sTagLocalActor=Port_TagSeat();
    if(network){sTagStage=Port_TagStage();sTagBgm=Port_TagBgm();}
    {extern int Port_TagFreeForAll(void);sTagFreeForAll=Port_TagFreeForAll();sTagWinningActor=-1;}
    for(i=0;i<count;++i)sTagTeams[i]=Port_TagTeam(i);TagBattle_ResetPairs();memset(sTagSides,0,sizeof(sTagSides));
#ifdef BT4_TAG_TEAM_EXPANDED
    for(i=0;i<count;++i){
        extern unsigned Port_TagActorCombat(int);
        extern int Port_Setting(const char *,int);
        u32 value;
        if(network)value=Port_TagActorCombat(i);
        else{
            char key[24];
            snprintf(key,sizeof(key),"tag_combat%d%d",sTagTeams[i]+1,TagBattle_TeamIndex(i)+1);
            value=Port_Setting(key,100u|(100u<<9));
        }
        if(value>=(1u<<19)||(value&511)<25||(value&511)>400||
           ((value>>9)&511)<25||((value>>9)&511)>400)value=100u|(100u<<9);
        sTagCombat[i]=value;
    }
#endif
    memset(sTagHealthByActor,0,sizeof(sTagHealthByActor));
    if(network){extern int Port_TagActorHealth(int,s32*);for(i=0;i<count;++i)Port_TagActorHealth(i,(s32*)&sTagHealthByActor[i]);}
    if (offline) {
        for (i = 0; i < count; ++i)
            sTagHealthByActor[i] = sTagHealthByTeam[sTagTeams[i]][TagBattle_TeamIndex(i)];
    }
    sTagPresentation=1;
    /* The dedicated offline rules use the native Duel timer indices. */
    if(!offline){
        if((getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_LONG_VIDEO"))||getenv("BT4_TAG_SPECTATOR_TEST"))timeLimit=0;
        else if(getenv("BT4_TAG_LOCAL_TEST"))timeLimit=5;
    }
    s32 referee = 4, destruction = 0;
#ifdef BT4_TAG_TEAM_EXPANDED
    sTagCpuFormsDisabled = 0;
    if (offline && !network) {
        extern int Port_Setting(const char *name, int fallback);
        referee = Port_Setting("tag_off_referee", 4);
        if (referee < 0 || referee > 6) referee = 4;
        destruction = Port_Setting("tag_off_destroy_stage", 0) == 1;
        if (Port_Setting("tag_off_cpu_forms_1", 1) == 0) sTagCpuFormsDisabled |= 1;
        if (Port_Setting("tag_off_cpu_forms_2", 1) == 0) sTagCpuFormsDisabled |= 2;
    }
    if (network) {
        extern unsigned Port_TagBattleRules(void);
        const unsigned rules = Port_TagBattleRules();
        if (rules) {
            referee = (rules & 7) ? (rules & 7) - 1 : 4;
            destruction = (rules & 8) != 0;
            sTagCpuFormsDisabled = (rules >> 4) & 3;
        }
    }
#endif
    BattleSetup_SetRule(offline==2,0,sTagBgm,timeLimit,referee,sTagStage,destruction);
    for(i=0;i<count;++i){
        s32 d=Port_TagDifficulty(i);if(d<0||d>4)d=2;
        f32 health=100.0f;
        if(network&&getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_ONLINE_RESULT_TEST"))health=2.0f;
        if(getenv("BT4_RENDER_TEST") && getenv("BT4_TAG_SPECTATOR_TEST") && i==0)health=1.0f;
        /* Hidden renderer regression: two low-health fighters let real CPU
         * attacks exercise KO portraits, alive counts and automatic retargeting. */
        if(getenv("BT4_RENDER_TEST") && getenv("BT4_TAG_KO_TEST")){
            const char *test=getenv("BT4_TAG_KO_TEST");
            if(i==3 || (i==2 && test[0]!='e'))health=1.0f;
        }
        s32 cpu=offline==3||(Port_TagCPU(i)&&!(offline==2&&i==1));
        if(getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_VIDEO_TEST"))cpu=1;
        BattleSetup_SetSide(i,cpu?BTL_CONTROL_CPU:BTL_CONTROL_PAD,offline==2&&i==1,1,1,1,0,NULL);
        BattleSetup_SetMember(i,0,Port_TagPlayerChara(i),Port_TagPlayerCostume(i),0,levels[d],health,
                             TagBattle_GetDraftItems(i,Port_TagPlayerChara(i)));
        sTagTargets[i]=TagBattle_FirstEnemy(i);
    }
    if(getenv("BT4_RENDER_TEST") && getenv("BT4_TAG_KO_TEST") && getenv("BT4_TAG_KO_TEST")[0]=='e')sTagTargets[0]=3;
    /* Camera/shake defaults participate in the native simulation. Personal
     * presentation settings must not select different native camera paths. */
    if(network){extern void BattleSetup_SetOptions(s32,s32,s32,s32);BattleSetup_SetOptions(0,0,1,1);}
    BattleSetup_Finish();
    if(offline)fprintf(stderr,"tag offline: mode=%d pads=%d presets=%d beans=%d\n",offline,offline==3?0:offline,Port_TagOfflineCustom(),Port_TagOfflineBeans());
    fprintf(stderr,"tag local: configured %d actors, teams=%d:%d humans=%d local-actor=%d independent CPU difficulty\n",count,TagBattle_TeamCount(0),TagBattle_TeamCount(1),Port_TagHumans(),sTagLocalActor);
    return 1;
}
s32 TagBattle_Count(void) { return sTagActors; }
BattleSide *TagBattle_ExtraSide(s32 actor) { return actor >= 2 && actor < sTagActors ? &sTagSides[actor - 2] : NULL; }
void TagBattle_Prepare(void) {
    const char *test = getenv("BT4_TAG_ACTOR_TEST");
    s32 i,n=test?atoi(test):2;
    if(sTagLocal)return;
    sTagFreeForAll=0;sTagWinningActor=-1;
    TagBattle_ResetPairs();sTagPresentation=1;
    sTagActors = n>=4 && n<=10 && !(n&1) ? n : 2;
    for(i=0;i<sTagActors;++i){sTagTeams[i]=i&1;sTagTargets[i]=i^1;}
    memset(sTagSides,0,sizeof(sTagSides));
    for(i=2;i<sTagActors;++i){
        BattleSide *dst=&sTagSides[i-2];
        *dst=Battle_GetSetup()->sides[i&1];
        dst->memberCount=1;dst->lead=0;dst->pad=i&1;dst->control=BTL_CONTROL_CPU;
        dst->members[0].cpuLevel=13;dst->objId=dst->modelSlot=-1;
        if(getenv("BT4_RENDER_TEST")&&getenv("BT4_TEST_TECH_ACTOR")&&atoi(getenv("BT4_TEST_TECH_ACTOR"))==i){
            if(getenv("BT4_TEST_ATTACK_CHARA")){dst->members[0].chara=atoi(getenv("BT4_TEST_ATTACK_CHARA"));dst->startForm.chara=dst->members[0].chara;}
        }
        if(i==2&&getenv("BT4_RENDER_TEST")&&getenv("BT4_TEST_OBSERVER_CHARA")){dst->members[0].chara=atoi(getenv("BT4_TEST_OBSERVER_CHARA"));dst->startForm.chara=dst->members[0].chara;}
    }
    if(getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_CAMERA_VIEW")){s32 a=atoi(getenv("BT4_TAG_CAMERA_VIEW"));sTagTestView=a>=0&&a<sTagActors?a:-1;}
    if(sTagActors>2)fprintf(stderr,"tag actors: loading %d independent actors\n",sTagActors);
}
void TagBattle_End(void) {
#ifdef BT4_TAG_TEAM_EXPANDED
    extern void TagFusion_End(void);TagFusion_End();
    sTagCpuFormsDisabled = 0;
#endif
    extern void Port_ActorAdxStopAll(void);Port_ActorAdxStopAll();extern void TagClash_Reset(void);TagClash_Reset();sTagFreeForAll=0;sTagWinningActor=-1;sTagActors=2;sTagLocal=0;sTagTestView=-1;memset(sTagSides,0,sizeof(sTagSides)); }

/* Returns the battle's shared state. */
BattleWork *Battle_GetWork(void) {
    return &gBattleWork;
}

/* Zeroes the whole work, then puts the setup, result and event blocks in their empty state. */
void Battle_ClearWork(void) {
    memset(Battle_GetWork(), 0, sizeof(BattleWork));
    BattleSetup_Clear();
    BattleResult_Clear();
    BtlEvent_ClearAll();
}

/* Start-of-match reset: stops audio, clears flags/result/events, reloads what changed and starts the BGM. */
void Battle_ResetWork(void) {
    BattleEvents *ev;

    Adx_StopAll();
    Battle_GetWork()->flags = 0;
    BtlLoad_Reload();
    BattleReplay_ClearDataFlag();
    BattleResult_Clear();
    BtlEvent_Reset();
#ifdef PORT
    Bgm_Play(PORT_BGM_FILE(Battle_GetBgm())); /* (a song added from outside the disc has a file id of its own) */
#else
    Bgm_Play(Battle_GetBgm() + 0x816);
#endif
    if (Battle_GetMode() == 1) {
        ev = Battle_GetEventWork();
        BtlScript_Restart();
        ev->unk4 = Gsc_StartMain(ev->script);
    }
}

/* Per-frame work update (play time, event bit sets); does nothing while paused. */
void Battle_UpdateWork(void) {
    if (Battle_GetWork()->flags & BATTLE_FLAG_PAUSE) {
        return;
    }
    BattleResult_CountFrame();
    BtlEvent_Update();
}

/* Returns the same block as Battle_GetWork (the setup lives at its start). */
BattleSetup *Battle_GetSetup(void) {
    return &Battle_GetWork()->setup;
}

/* Returns the result block. */
BattleResult *Battle_GetResult(void) {
    return &Battle_GetWork()->result;
}

/* Returns the script handle + event bit sets block. */
BattleEvents *Battle_GetEventWork(void) {
    return &Battle_GetWork()->events;
}

/* Returns the opponent pool of mode 3 (work + 0x5A8). */
BattleMemberPool *Battle_GetWork5A8(void) {
    return &Battle_GetWork()->pool;
}

/* Loads everything a battle needs (blocking, behind the loading screen) and creates the two sides' objects. */
s32 Battle_Load(void) {
    s32 actor;
    TagBattle_Prepare();
    BtlObj_Init(1);
    Gsc_InitDefault(gBtlScriptCommands);
    BtlScript_Init();
    BtlLoad_PushInitialJob();
    Load_RunBlocking();
    for(actor=0;actor<TagBattle_Count();++actor)
        BattleSide_SetObjId(actor,BtlObj_CreateChara(BattleSide_GetModelSlot(actor)));
    return 1;
}

/* Frees everything Battle_Load made. */
s32 Battle_Unload(void) {
    BtlObj_Term();
    BtlLoad_FreeAll();
    BtlScript_Free();
    Gsc_Exit();
    TagBattle_End();
    return 1;
}
