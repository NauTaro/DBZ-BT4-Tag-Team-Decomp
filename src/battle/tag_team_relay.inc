/* Dev-local cooperative pursuit. No network simulation change, teleport,
 * invulnerability, extra damage or fabricated hit is introduced. The native
 * melee resolver owns the counter; pressing the prompt never earns a link. */
typedef struct TagRelayChain {
    s32 last,partner,links,ticks,age,state,hitTick;
} TagRelayChain;
static TagRelayChain sTagRelay[10]; /* indexed by the victim, not its character */
static s32 sTagRelayAction[10],sTagRelayPress[10],sTagRelayTick;
static s32 sTagRelayTravel[10],sTagRelayTarget[10];
static s32 sTagRelaySeen[10];
static s32 sTagRelayStrike[10];
enum { TAG_RELAY_WINDOW=90,TAG_RELAY_FINISH=180 };

static void TagRelay_Clear(s32 victim){
    memset(&sTagRelay[victim],0,sizeof(sTagRelay[victim]));
    sTagRelay[victim].last=sTagRelay[victim].partner=-1;
    sTagRelay[victim].hitTick=-1;
}
static void TagRelay_Reset(void){
    sTagRelayTick=0;
    for(s32 i=0;i<10;++i){TagRelay_Clear(i);sTagRelayAction[i]=-1;sTagRelayPress[i]=0;sTagRelayTravel[i]=0;sTagRelayTarget[i]=-1;sTagRelaySeen[i]=-1;sTagRelayStrike[i]=-1;}
}
static s32 TagRelay_Airborne(s32 action){
    return action==0xCE||action==0xD4||action==0xD5||action==0xD6||action==0xDF||
           action==0xB8||action==0xBA||action==0xBC||action==0xBF;
}
s32 TagRelay_Engaged(s32 actor){
    if(!TagAssist_Local()||actor<0||actor>=TagBattle_Count())return 0;
    for(s32 t=0;t<TagBattle_Count();++t)if(sTagRelay[t].state&&
        (sTagRelay[t].last==actor||sTagRelay[t].partner==actor))return 1;
    return 0;
}
static s32 TagRelay_Free(s32 actor){
    if(TagAssist_Idle(actor))return 1;
    if(actor<0||actor>=TagBattle_Count()||TagBattle_IsActorLoading(actor)||TagBattle_Pair(actor)>=0||
       BtlCharApi_GetHp(actor)<=0||TagRevive_Busy(actor)||TagFusion_Busy(actor))return 0;
    void *chr=BtlChar_Get(actor);
    return chr&&BtlChar_IsFree(chr)&&!BtlChar_IsFrozen(chr)&&BtlAct_GetCurrent(chr)==0x19&&BtlAct_GetQueued(chr)<0;
}
static s32 TagRelay_Eligible(s32 actor,s32 victim){
    TagRelayChain *s;
    if(victim<0||victim>=TagBattle_Count()||!TagRelay_Free(actor)||TagAssist_Busy(actor))return 0;
    s=&sTagRelay[victim];
    if(s->state!=1||s->last==actor||s->last<0||s->links>=3||
       !TagBattle_Allied(actor,s->last)||sTagAssist[actor].cooldown||
       !TagAssist_TargetValid(actor,victim)||TagBattle_Target(actor)!=victim||
       !TagRelay_Airborne(BtlAct_GetCurrent(BtlChar_Get(victim)))||
       BtlSide_GetKi(actor)<5000||TagAssist_Distance(actor,victim)>600.f*600.f)return 0;
    TagAssistSegment segment;
    BtlCharApi_GetBasePos(BattleSide_GetObjId(actor),segment.from);
    BtlCharApi_GetBasePos(BattleSide_GetObjId(victim),segment.to);
    segment.from[1]-=BtlCharApi_GetHeight(BattleSide_GetObjId(actor))*.5f;
    segment.to[1]-=BtlCharApi_GetHeight(BattleSide_GetObjId(victim))*.5f;
    return !StgCol_TraceSegment(&segment);
}
static void TagRelay_ReadPads(void){
    s32 i;
    for(i=0;i<10;++i)sTagRelayPress[i]=0;
    if(!TagAssist_Running())return;
    for(i=0;i<TagBattle_Count();++i)if(!sTagRelayTravel[i]&&BattleSide_GetControl(i)==BTL_CONTROL_PAD&&
        TagRelay_Eligible(i,TagBattle_Target(i))){
        s32 pad=BattleSide_GetPad(i);
        Pad *command=TagBattle_CommandPad(i);
        if((TagBattle_Network()||(pad>=0&&pad<PAD_COUNT))&&(command->gamePressed&PADG_CROSS)){
            sTagRelayPress[i]=1;
            TagBattle_ConsumeCommand(i,PAD_CROSS);
            command->gameHeld&=~PADG_CROSS;command->gamePressed&=~PADG_CROSS;command->gameRepeat&=~PADG_CROSS;
            command->held&=~PAD_CROSS;command->pressed&=~PAD_CROSS;command->repeat&=~PAD_CROSS;
        }
    }
}
void TagRelay_Hit(s32 attacker,s32 defender,s32 reaction,s32 previousAction){
    if(!TagAssist_Running()||attacker<0||defender<0||attacker>=TagBattle_Count()||defender>=TagBattle_Count()||
       TagBattle_Allied(attacker,defender))return;
    sTagRelayStrike[attacker]=-1;
    if(getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_RELAY_TEST"))fprintf(stderr,"tag relay contact: actor=%d target=%d reaction=%X previous=%X\n",attacker,defender,reaction,previousAction);
    /* Being interrupted breaks that participant's chain, not an unrelated pair. */
    for(s32 t=0;t<TagBattle_Count();++t)if(t!=defender&&sTagRelay[t].state&&
        (sTagRelay[t].last==defender||sTagRelay[t].partner==defender))TagRelay_Clear(t);
    if(BtlCharApi_GetHp(defender)<=0||TagBattle_Pair(attacker)>=0||TagBattle_Pair(defender)>=0||
       TagAssist_Busy(attacker)||sTagAssist[attacker].cooldown)return;
    s32 hasAlly=0;
    for(s32 i=0;i<TagBattle_Count();++i)if(i!=attacker&&TagBattle_Allied(attacker,i)&&
        !TagBattle_IsActorLoading(i)&&BtlCharApi_GetHp(i)>0&&!TagFusion_Absent(i)){hasAlly=1;break;}
    if(!hasAlly)return;
    /* Only actual launch reactions, after guard/dodge/armor resolution. */
    if(reaction!=10&&reaction!=17&&reaction!=18&&reaction!=19&&reaction!=20&&
       reaction!=21&&reaction!=22&&reaction!=37&&reaction!=49)return;
    TagRelayChain *s=&sTagRelay[defender];
    if(s->hitTick==sTagRelayTick||s->state>=2)return;
    s32 previous=s->last;
    if(s->state==1&&s->ticks>0&&previous>=0&&previous!=attacker&&
       TagBattle_Allied(previous,attacker)&&TagRelay_Airborne(previousAction)){
        ++s->links;s->partner=previous;
    }else{s->links=0;s->partner=-1;}
    s->last=attacker;s->ticks=TAG_RELAY_WINDOW;s->age=0;s->state=1;s->hitTick=sTagRelayTick;
    if(s->links>=3){s->links=3;s->ticks=TAG_RELAY_FINISH;s->state=2;}
    if(getenv("BT4_RENDER_TEST"))fprintf(stderr,"tag relay hit: actor=%d target=%d links=%d reaction=%X previous=%X\n",
        attacker,defender,s->links,reaction,previousAction);
}
s32 TagRelay_Reaction(s32 attacker,s32 defender,s32 reaction,s32 previousAction){
    if(!TagAssist_Running()||attacker<0||defender<0||attacker>=TagBattle_Count()||defender>=TagBattle_Count()||
       sTagRelayStrike[attacker]!=defender||reaction<=2||!TagRelay_Airborne(previousAction))return reaction;
    TagRelayChain *s=&sTagRelay[defender];
    if(s->state!=1||s->last<0||s->last==attacker||!TagBattle_Allied(s->last,attacker)||
       BtlCharApi_GetHp(defender)<=0||TagBattle_Pair(attacker)>=0||TagBattle_Pair(defender)>=0)return reaction;
    /* This is a confirmed, unguarded hit by the cooperative homing strike.
     * Launch with native reaction 17 and its original force/damage pipeline;
     * missed, escaped, armored and guarded attacks never get this treatment. */
    return 17;
}
f32 TagRelay_LaunchYaw(s32 attacker,s32 defender,f32 nativeYaw){
    extern f32 atan2f(f32,f32);
    if(!TagAssist_Running()||attacker<0||defender<0||attacker>=TagBattle_Count()||defender>=TagBattle_Count()||
       sTagRelayStrike[attacker]!=defender)return nativeYaw;
    TagRelayChain *s=&sTagRelay[defender];
    if(s->state!=1||s->last<0||s->last==attacker||!TagBattle_Allied(attacker,s->last))return nativeYaw;
    f32 p[4],q[4],dx,dz;
    BtlCharApi_GetBasePos(BattleSide_GetObjId(defender),p);BtlCharApi_GetBasePos(BattleSide_GetObjId(s->last),q);
    dx=q[0]-p[0];dz=q[2]-p[2];
    f32 distance=sqrtf(dx*dx+dz*dz);
    if(distance>20){
        /* Aim past the receiver's shoulder, not into their collision capsule.
         * This leaves room for the receiving player's own pursuit input. */
        f32 side=60.f;
        dx+=dz/distance*side;dz-=(q[0]-p[0])/distance*side;
    }
    /* Pass the launched opponent back toward the previous teammate. Keep the
     * native speed, height, hurt response and wall/ground collision. */
    return dx*dx+dz*dz>20.f*20.f?atan2f(dx,dz):nativeYaw;
}
static s32 TagRelay_FirstBeam(s32 actor){
    extern s32 BtlSuper_GetHitDirKind(void *,s32);
    /* The execution type alone is insufficient: type 2 also contains charged
     * Kamehamehas. Native recoil kind 0 denotes their sustained forward force.
     * Warp moves, rushes and ultimates are not substituted for slot 2. */
    s32 type=BtlSuper_GetType(BtlChar_Get(actor),2),flags=BtlSuper_GetFlags(BtlChar_Get(actor),2);
    return (type==0||type==1||type==2||type==7)&&BtlSuper_GetHitDirKind(BtlChar_Get(actor),2)==0&&
        !(flags&(0x2000|0x4000));
}
static void TagRelay_Update(void){
    s32 i;
    for(i=0;i<10;++i)sTagRelayAction[i]=-1;
    if(!TagAssist_Running())return;
    ++sTagRelayTick;
    for(i=0;i<TagBattle_Count();++i)if(BtlAct_GetCurrent(BtlChar_Get(i))!=0x6E)sTagRelayStrike[i]=-1;
    for(i=0;i<TagBattle_Count();++i)if(sTagRelayTravel[i]){
        s32 t=sTagRelayTarget[i],action=BtlAct_GetCurrent(BtlChar_Get(i));
        if(--sTagRelayTravel[i]<=0||!TagAssist_TargetValid(i,t)||!TagRelay_Airborne(BtlAct_GetCurrent(BtlChar_Get(t)))||
           (action!=0x19&&sTagRelayTravel[i]<43)||TagFusion_Busy(i)||TagRevive_Busy(i)){
            sTagRelayTravel[i]=0;continue;
        }
        f32 reach=BtlCharApi_GetRadius(BattleSide_GetObjId(i))+BtlCharApi_GetRadius(BattleSide_GetObjId(t))+10;
        if(action==0x19&&TagAssist_Distance(i,t)<reach*reach)sTagRelayAction[i]=0x6E;
    }
    for(s32 t=0;t<TagBattle_Count();++t){
        TagRelayChain *s=&sTagRelay[t];
        if(!s->state)continue;
        if(--s->ticks<=0||!TagAssist_TargetValid(s->last,t)||BtlCharApi_GetHp(s->last)<=0||
           TagFusion_Busy(s->last)||(s->partner>=0&&(BtlCharApi_GetHp(s->partner)<=0||TagFusion_Busy(s->partner)))){
            TagRelay_Clear(t);continue;
        }
        ++s->age;
        if(s->state==1&&s->age>3&&!TagRelay_Airborne(BtlAct_GetCurrent(BtlChar_Get(t)))){
            TagRelay_Clear(t);continue;
        }
        if(s->state==2){
            s32 a=s->last,b=s->partner;
            if(b<0||!TagRelay_FirstBeam(a)||!TagRelay_FirstBeam(b))continue;
            if(BattleSide_GetControl(a)!=BTL_CONTROL_PAD&&BattleSide_GetControl(b)==BTL_CONTROL_PAD){s32 swap=a;a=b;b=swap;}
            /* The three accepted relays consent to the finish. Stop ordinary
             * movement, but never cancel an attack, dodge or hurt reaction. */
            if(TagRelay_Free(a)&&BtlAct_GetCurrent(BtlChar_Get(a))==0x19){BtlAct_Request(BtlChar_Get(a),0xB);continue;}
            if(TagRelay_Free(b)&&BtlAct_GetCurrent(BtlChar_Get(b))==0x19){BtlAct_Request(BtlChar_Get(b),0xB);continue;}
            if(!TagAssist_Idle(a)||!TagAssist_Idle(b)||TagAssist_Busy(a)||TagAssist_Busy(b)||
               sTagAssist[a].cooldown||sTagAssist[b].cooldown||
               !BtlAct_CanQueueTechnique(BtlChar_Get(a),2)||!BtlAct_CanQueueTechnique(BtlChar_Get(b),2)||
               TagAssist_Distance(a,b)>600.f*600.f)continue;
            if(TagBattle_Target(a)!=t)continue;
            if(TagAssist_Start(a,b)){
                sTagAssist[a].slot=sTagAssist[a].otherSlot=2;sTagAssist[a].relay=1;
                s->last=a;s->partner=b;s->state=3;s->ticks=TAG_RELAY_FINISH;
            }
        }else if(s->state==3){
            if(sTagAssist[s->last].active){s->state=4;s->ticks=60;}
            else if(!sTagAssist[s->last].wait)TagRelay_Clear(t);
        }
    }
    for(i=0;i<TagBattle_Count();++i){
        s32 t=TagBattle_Target(i);
        if(sTagRelayTravel[i]||!TagRelay_Eligible(i,t))continue;
        TagRelayChain *s=&sTagRelay[t];
        s32 claimed=0;
        for(s32 j=0;j<TagBattle_Count();++j)if(j!=i){
            if((sTagRelayTravel[j]&&sTagRelayTarget[j]==t)||sTagRelayStrike[j]==t){claimed=1;break;}
            if(BattleSide_GetControl(i)!=BTL_CONTROL_PAD&&TagRelay_Eligible(j,t)&&
               (sTagRelayPress[j]||(BattleSide_GetControl(j)!=BTL_CONTROL_PAD&&sTagRelaySeen[j]!=s->hitTick&&
                (TagAssist_Distance(j,t)<TagAssist_Distance(i,t)||(TagAssist_Distance(j,t)==TagAssist_Distance(i,t)&&j<i))))){claimed=1;break;}
            if(sTagRelayPress[i]&&sTagRelayPress[j]&&j<i&&TagBattle_Target(j)==t){claimed=1;break;}
        }
        if(claimed)continue;
        if(sTagRelayPress[i]||(BattleSide_GetControl(i)!=BTL_CONTROL_PAD&&s->age>=3&&sTagRelaySeen[i]!=s->hitTick)){
            /* Cooperative burst, based on the native dash and homing strike. */
            sTagRelayAction[i]=0x19;
            sTagRelaySeen[i]=s->hitTick;
            if(getenv("BT4_RENDER_TEST"))fprintf(stderr,"tag relay pursuit: actor=%d target=%d\n",i,t);
        }
    }
}
s32 TagRelay_TakeAction(s32 actor){
    if(!TagAssist_Running()||actor<0||actor>=TagBattle_Count())return -1;
    s32 action=sTagRelayAction[actor];sTagRelayAction[actor]=-1;
    if(action==0x6E&&sTagRelayTravel[actor]&&BtlAct_GetCurrent(BtlChar_Get(actor))==0x19&&
       TagAssist_TargetValid(actor,sTagRelayTarget[actor])&&BtlChar_IsFree(BtlChar_Get(actor))){
        sTagRelayTravel[actor]=0;sTagRelayStrike[actor]=sTagRelayTarget[actor];return action;
    }
    if(action==0x19&&TagRelay_Eligible(actor,TagBattle_Target(actor))){
        extern s32 BtlMember_SpendKi(void *,s32,s32);
        BtlMember_SpendKi(BtlChar_Get(actor),5000,0);
        sTagRelayTravel[actor]=45;sTagRelayTarget[actor]=TagBattle_Target(actor);
        return BtlAct_GetCurrent(BtlChar_Get(actor))==0x19?-1:action;
    }
    return -1;
}
f32 TagRelay_RushScale(s32 actor){
    return TagAssist_Running()&&actor>=0&&actor<TagBattle_Count()&&sTagRelayTravel[actor]>0?2.f:1.f;
}
s32 TagRelay_IgnoreBodyStop(s32 actor){
    return TagAssist_Running()&&actor>=0&&actor<TagBattle_Count()&&sTagRelay[actor].state&&
        sTagRelay[actor].links>0&&sTagRelay[actor].age<=5;
}
s32 TagRelay_Hud(s32 actor,s32 *partner,s32 *links,s32 *ticks){
    if(!TagAssist_Local()||actor<0||actor>=TagBattle_Count()||BtlCharApi_GetHp(actor)<=0)return 0;
    s32 t=TagBattle_Target(actor);
    if(t<0||t>=TagBattle_Count())return 0;
    TagRelayChain *s=&sTagRelay[t];
    if(!s->state||s->last<0||!TagBattle_Allied(actor,s->last))return 0;
    *partner=actor==s->last?s->partner:s->last;*links=s->links;*ticks=s->ticks;
    if(s->state>=2&&s->partner>=0){
        extern s32 BtlSuper_GetKiCost(void *,s32);
        if(!TagRelay_FirstBeam(s->last)||!TagRelay_FirstBeam(s->partner))return 4;
        if(BtlSide_GetKi(s->last)<BtlSuper_GetKiCost(BtlChar_Get(s->last),2)||
           BtlSide_GetKi(s->partner)<BtlSuper_GetKiCost(BtlChar_Get(s->partner),2))return 5;
    }
    return s->state>=2?3:TagRelay_Eligible(actor,t)?1:2;
}
