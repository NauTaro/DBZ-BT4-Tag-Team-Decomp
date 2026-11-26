/* Native local Dev integration fixtures. Private pad playback, no OS input. */
static s32 sReviveTestTick,sReviveTestCase,sReviveTestFailed,sReviveSaved,sReviveKi,sReviveObserved;
static s32 sReviveFullKiProgress;
static f32 sReviveThreatPosition[4],sReviveThreatRotation[4];
static s32 TagRevive_TestOn(void){return getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_REVIVE_TEST")&&BtlSeq_IsFighting();}
static void TagRevive_Check(const char *name,s32 pass){
    fprintf(stderr,"tag revive check: %s=%s\n",name,pass?"PASS":"FAIL");if(!pass)sReviveTestFailed=1;
}
static void TagRevive_TestPads(void){
    s32 actor;
    if(!TagRevive_TestOn())return;
    sReviveTestCase=atoi(getenv("BT4_TAG_REVIVE_TEST"));++sReviveTestTick;
    for(actor=0;actor<PAD_COUNT;++actor){gPad[actor].held=gPad[actor].pressed=0;gPad[actor].gameHeld=gPad[actor].gamePressed=gPad[actor].gameRepeat=0;}
    if(sReviveTestTick==10){
        for(actor=0;actor<TagBattle_Count();++actor){
            s32 *g=BtlMember_GetActiveGauge(BtlChar_Get(actor));g[0]=g[1]=40000;g[3]=12345;
            BtlAct_Request(BtlChar_Get(actor),0xB);TagJoint_Place(actor,actor>=2?20.f:0.f,0);
        }
        if(sReviveTestCase==7||sReviveTestCase==9||sReviveTestCase==10||sReviveTestCase==14){ /* CPU helper rescues fallen P1 */
            BtlMember_GetActiveGauge(BtlChar_Get(0))[0]=0;BtlAct_Request(BtlChar_Get(0),0xD8);
            TagBattle_UpdateSpectator(1);
            if(sReviveTestCase==9)TagJoint_PlaceInBeam(2,-1.0f,0);
            if(sReviveTestCase==10){TagJoint_PlaceInBeam(1,0,10);TagJoint_PlaceInBeam(3,.7f,80);}
        }else{
            BtlMember_GetActiveGauge(BtlChar_Get(2))[0]=0;BtlAct_Request(BtlChar_Get(2),0xD8);
        }
        if(sReviveTestCase==5)for(actor=1;actor<TagBattle_Count();++actor)
            if(actor!=2&&TagBattle_Allied(0,actor))TagJoint_Place(actor,15.f,0);
        if(sReviveTestCase==8){
            BtlMember_GetActiveGauge(BtlChar_Get(3))[0]=0;BtlAct_Request(BtlChar_Get(3),0xD8);
        }
        sReviveKi=BtlSide_GetKi(sReviveTestCase==7||sReviveTestCase==9||sReviveTestCase==10||sReviveTestCase==14?0:2);
    }
    if(sReviveTestCase==14&&sReviveTestTick==100){
        f32 p[4],rot[4]={0,0,0,0};
        sReviveSaved=sTagRevive[0].progress;
        TagRevive_Check("CPU_had_started_rescue",sReviveSaved>0&&sTagRevive[2].target==0);
        BtlCharApi_GetBasePos(BattleSide_GetObjId(1),sReviveThreatPosition);
        memcpy(sReviveThreatRotation,(u8 *)BtlChar_GetPos(BtlChar_Get(1))+16,16);
        BtlCharApi_GetBasePos(BattleSide_GetObjId(0),p);p[0]+=12;
        BtlChar_Place((BtlCtlChr *)BtlChar_Get(1),(Vec4 *)p,(Vec4 *)rot,BtlStage_FindZoneNear(-1,(Vec4 *)p));
    }
    if(sReviveTestCase==14&&sReviveTestTick==140){
        TagRevive_Check("CPU_retains_interrupted_rescue",sTagRevive[2].target==0&&!sTagRevive[2].busy);
        TagRevive_Check("unsafe_rescue_keeps_progress",sTagRevive[0].progress==sReviveSaved);
        BtlChar_Place((BtlCtlChr *)BtlChar_Get(1),(Vec4 *)sReviveThreatPosition,
            (Vec4 *)sReviveThreatRotation,BtlStage_FindZoneNear(-1,(Vec4 *)sReviveThreatPosition));
    }
    if(sReviveTestCase==3){
        if(sReviveTestTick==90)TagRevive_Check("charge_rule_rejects_idle",sTagRevive[2].progress==0);
        if(sReviveTestTick>=100&&sReviveTestTick<300)gPad[0].held=PAD_L2;
    }
    if(sReviveTestCase==13&&sReviveTestTick==95){
        extern s32 TagBattle_Boss(s32),TagBattle_CombatDamage(s32,s32,s32);
        extern s32 TagBattle_SetTarget(s32,s32);
        extern void TagBattle_CycleCameraTarget(s32),TagBattle_FocusEnemy(s32);
        extern s32 TagBattle_Focus(s32);
        extern void TagBattle_CounterTarget(s32,s32),TagBattle_ClearCounterTarget(s32);
        TagRevive_Check("boss_resists_catch_and_scales_damage",
            TagBattle_Boss(0)&&!TagBattle_ReservePair(1,0)&&
            TagBattle_CombatDamage(0,1,200)==300&&TagBattle_CombatDamage(1,0,200)==100);
        TagBattle_CounterTarget(0,3);
        TagRevive_Check("counter_preserves_actual_attacker",TagBattle_Target(0)==3&&!TagBattle_SetTarget(0,1));
        TagBattle_ClearCounterTarget(0);
        TagBattle_SetTarget(0,1);
        for(s32 j=0;j<TagBattle_Count()&&TagBattle_Focus(0)<0;++j)TagBattle_CycleCameraTarget(0);
        TagRevive_Check("ally_focus_keeps_enemy_for_combat",TagBattle_Focus(0)==2&&TagBattle_Target(0)==3);
        TagBattle_CycleCameraTarget(0);
        TagRevive_Check("enemy_view_restores_enemy_for_combat",TagBattle_Focus(0)==-1&&TagBattle_Target(0)==1);
        TagBattle_FocusEnemy(0);
        {
            extern void BtlMembers_UpdateQueuedDamage(void);
            s32 *q=(s32 *)((u8 *)BtlChar_Get(0)+0xD94);
            s32 *victim=BtlMember_GetActiveGauge(BtlChar_Get(1)),before=victim[0];
            q[1]=200;q[4]=0x1FB;
            BtlChar_SetHeldFlag(BtlChar_Get(0),0x8C);BtlMembers_UpdateQueuedDamage();
            TagRevive_Check("cinematic_damage_uses_outgoing_modifier",before-victim[0]==300);
            BtlChar_ClearFlag(BtlChar_Get(0),0x8C);victim[0]=before;
            q=(s32 *)((u8 *)BtlChar_Get(1)+0xD94);
            victim=BtlMember_GetActiveGauge(BtlChar_Get(0));before=victim[0];
            q[1]=200;q[4]=0x1FB;
            BtlChar_SetHeldFlag(BtlChar_Get(1),0x8C);BtlMembers_UpdateQueuedDamage();
            TagRevive_Check("cinematic_damage_uses_resistance",before-victim[0]==100);
            BtlChar_ClearFlag(BtlChar_Get(1),0x8C);victim[0]=before;
        }
    }
    if(sReviveTestCase==13&&sReviveTestTick>=100&&sReviveTestTick<340){
        extern void BtlMember_SetMaxPower(void *,s32);
        s32 *g=BtlMember_GetActiveGauge(BtlChar_Get(0));
        g[3]=g[4];
        if(sReviveTestTick==145)BtlMember_SetMaxPower(BtlChar_Get(0),30000);
        gPad[0].held=PAD_L2;
    }
    if(sReviveTestCase==4){
        if(sReviveTestTick==70){
            sReviveSaved=sTagRevive[2].progress;TagJoint_Place(0,150.f,0);
            TagRevive_Check("partial_progress",sReviveSaved>0);
        }
        if(sReviveTestTick==100){
            TagRevive_Check("leaving_keeps_progress",sTagRevive[2].progress==sReviveSaved);
            TagJoint_Place(0,0,0);
            Battle_GetWork()->flags|=BATTLE_FLAG_PAUSE;
        }
    }
    if(sReviveTestCase==6){
        if(sReviveTestTick>=10&&sReviveTestTick<90)BtlAct_Request(BtlChar_Get(2),0xD9);
        if(sReviveTestTick==90){
            TagRevive_Check("cannot_revive_falling_actor",sTagRevive[2].progress==0);
            BtlAct_Request(BtlChar_Get(2),0xD8);
        }
    }
    if(sReviveTestCase==11&&sReviveTestTick==70){
        sReviveSaved=sTagRevive[2].progress;
        BtlMember_GetActiveGauge(BtlChar_Get(0))[0]-=5000;
    }
    if(sReviveTestCase==12&&sReviveTestTick==70){
        TagRevive_Check("restart_had_progress",sTagRevive[2].progress>0);
        TagRevive_Reset();
        TagRevive_Check("restart_clears_revival",!sTagRevive[2].progress&&!sTagRevive[2].ready&&!sTagRevive[2].completed);
    }
}
static void TagRevive_TestInput(void){
    s32 actor,corpse=sReviveTestCase==7||sReviveTestCase==9||sReviveTestCase==10||sReviveTestCase==14?0:2;
    if(!TagRevive_TestOn())return;
    for(actor=0;actor<TagBattle_Count();++actor)if(BattleSide_GetControl(actor)!=BTL_CONTROL_PAD)
        BtlCharApi_SetInjectedInput(BattleSide_GetObjId(actor),0,0,0);
    if(!sTagRevive[corpse].completed)sReviveKi=BtlMember_GetActiveGauge(BtlChar_Get(corpse))[3];
}
static void TagRevive_TestAfter(void){
    s32 corpse=sReviveTestCase==7||sReviveTestCase==9||sReviveTestCase==10||sReviveTestCase==14?0:2;
    if(!TagRevive_TestOn())return;
    if(sReviveTestTick==90||sReviveTestTick==280){extern void Port_TagUiCapture(s32);Port_TagUiCapture(sReviveTestTick==90?110:111);}
    if(sTagRevive[corpse].completed&&!sReviveObserved){
        s32 ki=BtlMember_GetActiveGauge(BtlChar_Get(corpse))[3];sReviveObserved=1;
        fprintf(stderr,"tag revive ki: before=%d after=%d\n",sReviveKi,ki);
        TagRevive_Check("no_ki_refill_on_revival",ki<=sReviveKi+1000);
    }
    if(sReviveTestCase==4&&sReviveTestTick==125){
        TagRevive_Check("pause_preserves_progress",sTagRevive[2].progress==sReviveSaved);
        Battle_GetWork()->flags&=~BATTLE_FLAG_PAUSE;
    }
    if(sReviveTestCase==11&&sReviveTestTick==70)TagRevive_Check("damage_interrupts_helper",sTagRevive[2].progress==sReviveSaved);
    if(sReviveTestTick%60==0)fprintf(stderr,"tag revive trace: tick=%d action=%X hp=%d progress=%d helpers=%d ready=%d\n",
        sReviveTestTick,BtlAct_GetCurrent(BtlChar_Get(corpse)),BtlCharApi_GetHp(corpse),sTagRevive[corpse].progress,sTagRevive[corpse].helpers,sTagRevive[corpse].ready);
    if(sReviveTestTick==90&&sReviveTestCase==5)sReviveSaved=sTagRevive[2].progress;
    if(sReviveTestTick==100&&sReviveTestCase==5)
        TagRevive_Check("cooperation_speed_1_25",sTagRevive[2].helpers==2&&sTagRevive[2].progress-sReviveSaved==50);
    if(sReviveTestCase==13&&sReviveTestTick>=100&&sReviveTestTick<340){
        if(BtlAct_GetCurrent(BtlChar_Get(0))!=0x37&&sTagRevive[2].progress>sReviveSaved)
            sReviveFullKiProgress=1;
        sReviveSaved=sTagRevive[2].progress;
    }
    if(sReviveTestTick==(sReviveTestCase==9?900:360)){
        s32 *g=BtlMember_GetActiveGauge(BtlChar_Get(corpse));
        if(sReviveTestCase==2||sReviveTestCase==10)TagRevive_Check(sReviveTestCase==10?"CPU_rejects_unsafe_rescue":"disabled_team_stays_down",g[0]==0&&sTagRevive[corpse].completed==0);
        else{
            TagRevive_Check("revives_once",sTagRevive[corpse].completed==1);
            TagRevive_Check("restores_correct_health",g[0]==10000&&g[1]==40000);
            TagRevive_Check("native_getup_finished",BtlAct_GetCurrent(BtlChar_Get(corpse))!=0xD8&&BtlChar_IsFree(BtlChar_Get(corpse)));
        }
        if(sReviveTestCase==7){TagBattle_UpdateSpectator(0);TagRevive_Check("revive_restores_local_view",TagBattle_ViewActor()==0&&!TagBattle_Spectating());}
        if(sReviveTestCase==8)TagRevive_Check("team2_disabled_independently",BtlCharApi_GetHp(3)==0&&sTagRevive[3].completed==0);
        if(sReviveTestCase==13)TagRevive_Check("full_ki_holding_charge_advances_revival",sReviveFullKiProgress);
        fprintf(stderr,"tag revive result: case=%d %s\n",sReviveTestCase,sReviveTestFailed?"FAIL":"PASS");fflush(NULL);exit(sReviveTestFailed);
    }
}
