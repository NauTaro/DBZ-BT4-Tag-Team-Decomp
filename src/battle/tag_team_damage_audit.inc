/* Native attack/recovery integration fixture. Only opt-in hidden test runs. */
static s32 sDamageTick,sDamageCase,sDamageFailed,sDamageTechniqueHp,sDamageMeleeHp,sDamageMeleeHit;
static s32 TagDamage_Shooter(void){return getenv("BT4_DAMAGE_SHOOTER")?atoi(getenv("BT4_DAMAGE_SHOOTER")):0;}
static s32 TagDamage_MeleeTick(void){return getenv("BT4_DAMAGE_MELEE_TICK")?atoi(getenv("BT4_DAMAGE_MELEE_TICK")):240;}
static s32 TagDamage_On(void){return getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_DAMAGE_TEST")&&BtlSeq_IsFighting();}
static void TagDamage_Check(const char *name,s32 ok){
    fprintf(stderr,"tag damage check: %s=%s\n",name,ok?"PASS":"FAIL");if(!ok)sDamageFailed=1;
}
static void TagDamage_Place(s32 actor,const f32 *origin,f32 x,f32 z,f32 yaw){
    f32 p[4],r[4]={0,0,0,0};memcpy(p,origin,sizeof(p));p[0]+=x;p[2]+=z;r[1]=yaw;
    BtlChar_Place((BtlCtlChr *)BtlChar_Get(actor),(Vec4 *)p,(Vec4 *)r,BtlStage_FindZoneNear(-1,(Vec4 *)p));
}
static void TagDamage_TestPads(void){
    if(!TagDamage_On())return;
    s32 i,shooter=TagDamage_Shooter(),striker=2-shooter;sDamageCase=atoi(getenv("BT4_TAG_DAMAGE_TEST"));++sDamageTick;
    for(i=0;i<PAD_COUNT;++i){gPad[i].held=gPad[i].pressed=gPad[i].repeat=0;gPad[i].gameHeld=gPad[i].gamePressed=gPad[i].gameRepeat=0;}
    if(sDamageTick==10){
        f32 p[4];
        for(i=0;i<TagBattle_Count();++i){s32 *g=BtlMember_GetActiveGauge(BtlChar_Get(i));g[0]=g[1]=200000;g[3]=g[4];g[5]=500000;BtlAct_Request(BtlChar_Get(i),0xB);}
        BtlCharApi_GetBasePos(BattleSide_GetObjId(0),p);
        TagDamage_Place(shooter,p,0,0,0);TagDamage_Place(1,p,0,35,3.14159265f);TagDamage_Place(striker,p,120,0,0);
        TagBattle_SetTarget(0,1);TagBattle_SetTarget(2,1);TagBattle_SetTarget(1,0);
    }
    if(sDamageTick==30){
        extern s32 BtlSuper_GetHitDirKind(void *,s32),BtlSkill_GetId(void *,s32),BtlAct_GetSkillAction(void *,s32),BtlAct_CanUseSkill(void *,s32);
        for(i=0;i<TagBattle_Count();++i)BtlChar_SetFlag(BtlChar_Get(i),5);
        for(i=2;i<5;++i)fprintf(stderr,"tag damage technique: actor=%d slot=%d id=%d type=%d kind=%d cost=%d\n",shooter,i,BtlSuper_GetId(BtlChar_Get(shooter),i),BtlSuper_GetType(BtlChar_Get(shooter),i),BtlSuper_GetHitDirKind(BtlChar_Get(shooter),i),BtlSuper_GetKiCost(BtlChar_Get(shooter),i));
        for(i=0;i<2;++i)fprintf(stderr,"tag damage skill: actor=%d slot=%d id=%d action=%X can=%d\n",shooter,i,BtlSkill_GetId(BtlChar_Get(shooter),i),BtlAct_GetSkillAction(BtlChar_Get(shooter),i),BtlAct_CanUseSkill(BtlChar_Get(shooter),i));
        if(sDamageCase==2||sDamageCase==5){
            s32 slot=sDamageCase==2?0:1;TagDamage_Check("skill_admitted",BtlAct_CanUseSkill(BtlChar_Get(shooter),slot));
            BtlAct_Request(BtlChar_Get(shooter),BtlAct_GetSkillAction(BtlChar_Get(shooter),slot));
        }else if(sDamageCase!=3)TagDamage_Check("technique_queued",BtlAct_QueueTechnique(BtlChar_Get(shooter),sDamageCase==4?2:3));
    }
    if(sDamageTick==TagDamage_MeleeTick()){
        f32 p[4];sDamageTechniqueHp=BtlCharApi_GetHp(1);sDamageMeleeHp=sDamageTechniqueHp;
        BtlCharApi_GetBasePos(BattleSide_GetObjId(1),p);
        TagDamage_Place(striker,p,0,-14,0);TagBattle_SetTarget(striker,1);BtlChar_SetFlag(BtlChar_Get(striker),5);
        BtlAct_Request(BtlChar_Get(striker),0x52);
    }
}
static void TagDamage_TestInput(void){
    if(!TagDamage_On())return;
    extern void BtlCharApi_SetInjectedInput(s32,u32,f32,f32);
    for(s32 i=0;i<TagBattle_Count();++i)if(BattleSide_GetControl(i)!=BTL_CONTROL_PAD)BtlCharApi_SetInjectedInput(BattleSide_GetObjId(i),0,0,0);
}
static void TagDamage_TestAfter(void){
    if(!TagDamage_On())return;
    if(sDamageTick%15==0){
        extern s32 BtlChar_TestFlag(void *,s32),BtlAnim_GetId(void *),BtlAtk_GetId(void *),BtlCharApi_IsCamShown(s32);
        for(s32 i=0;i<3;++i){void *c=BtlChar_Get(i);
            fprintf(stderr,"tag damage trace: tick=%d actor=%d hp=%d act=%X anim=%X atk=%X pair=%d freeze=%d void=%d hidden=%d immune=%d,%d,%d,%d prepare=%d interact=%d\n",
                sDamageTick,i,BtlCharApi_GetHp(i),BtlAct_GetCurrent(c),BtlAnim_GetId(c),BtlAtk_GetId(c),TagBattle_Pair(i),BtlChar_IsFrozen(c),BtlChar_TestFlag(c,0xBA),BtlChar_TestFlag(c,0xB),BtlChar_TestFlag(c,0x42),BtlChar_TestFlag(c,0x44),BtlChar_TestFlag(c,0x46),BtlChar_TestFlag(c,0xAA),TagBattle_Preparing(i),TagBattle_CanInteract(i,1));
        }
    }
    if(sDamageTick==TagDamage_MeleeTick()+160){
        if(sDamageCase!=3)TagDamage_Check("technique_hit_victim",sDamageTechniqueHp<200000);
        TagDamage_Check("other_ally_melee_deals_damage",sDamageMeleeHit&&BtlCharApi_GetHp(1)<sDamageMeleeHp);
        TagDamage_Check("victim_recovers_collision",!BtlChar_TestFlag(BtlChar_Get(1),0x42)&&TagBattle_CanInteract(2,1));
        TagDamage_Check("victim_remains_visible",!BtlChar_TestFlag(BtlChar_Get(1),0xB));
        fprintf(stderr,"tag damage result: case=%d %s\n",sDamageCase,sDamageFailed?"FAIL":"PASS");fflush(NULL);exit(sDamageFailed?3:0);
    }
}
void TagDamage_TestHit(s32 attacker,s32 victim){
    if(TagDamage_On()&&sDamageTick>=TagDamage_MeleeTick()&&attacker==2-TagDamage_Shooter()&&victim==1){
        sDamageMeleeHit=1;fprintf(stderr,"tag damage confirmed melee: tick=%d attacker=%d victim=%d hp=%d\n",sDamageTick,attacker,victim,BtlCharApi_GetHp(victim));
    }
}
