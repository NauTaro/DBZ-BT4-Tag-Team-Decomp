/* Opt-in integration fixtures. The full relay uses native homing strikes,
 * collisions, launched reactions and controller input, not synthetic credits. */
static s32 sRelayTestTick,sRelayTestCase,sRelayTestFailed,sRelayTestMax,sRelayTestBeam,sRelayTestDamage;
static s32 sRelayTestCost[2],sRelayTestKi[2],sRelayTestPause;
static s32 TagRelay_TestOn(void){return getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_RELAY_TEST")&&BtlSeq_IsFighting();}
static void TagRelay_Check(const char *key,s32 ok){
    fprintf(stderr,"tag relay check: %s=%s\n",key,ok?"PASS":"FAIL");if(!ok)sRelayTestFailed=1;
}
static void TagRelay_TestPads(void){
    if(!TagRelay_TestOn())return;
    s32 i;sRelayTestCase=atoi(getenv("BT4_TAG_RELAY_TEST"));++sRelayTestTick;
    for(i=0;i<PAD_COUNT;++i){gPad[i].held=gPad[i].pressed=gPad[i].repeat=0;gPad[i].gameHeld=gPad[i].gamePressed=gPad[i].gameRepeat=0;}
    if(sRelayTestTick==10){
        f32 p[4],q[4],r[4]={0,0,0,0},dx,dz,n;
        for(i=0;i<TagBattle_Count();++i){
            s32 *g=BtlMember_GetActiveGauge(BtlChar_Get(i));g[0]=g[1]=200000;g[3]=g[4];
            BtlAct_Request(BtlChar_Get(i),0xB);TagJoint_Place(i,0,0);
        }
        BtlCharApi_GetBasePos(BattleSide_GetObjId(0),p);BtlCharApi_GetBasePos(BattleSide_GetObjId(1),q);
        dx=q[0]-p[0];dz=q[2]-p[2];n=sqrtf(dx*dx+dz*dz);dx/=n;dz/=n;
        for(i=1;i<=2;++i){
            memcpy(q,p,sizeof(q));q[0]+=dx*(i==0?0:i==1?35:85);q[2]+=dz*(i==0?0:i==1?35:85);
            BtlChar_Place((BtlCtlChr *)BtlChar_Get(i),(Vec4 *)q,(Vec4 *)r,BtlStage_FindZoneNear(-1,(Vec4 *)q));
        }
        TagBattle_SetTarget(0,1);TagBattle_SetTarget(2,1);TagBattle_SetTarget(1,0);
        if(sRelayTestCase==7){
            BattleSide *ally=TagBattle_ExtraSide(2);ally->control=BTL_CONTROL_PAD;ally->pad=1;
            *(s32 *)((u8 *)BtlChar_Get(2)+4)=1;*(s32 *)((u8 *)BtlChar_Get(2)+0x1278)=0;
        }
    }
    if(sRelayTestTick==25){
        extern s32 BtlSuper_GetHitDirKind(void *,s32);
        for(i=0;i<3;++i)BtlChar_SetFlag(BtlChar_Get(i),5);
        for(i=0;i<2;++i){s32 a=i?2:0;
            sRelayTestKi[i]=BtlSide_GetKi(a);sRelayTestCost[i]=BtlSuper_GetKiCost(BtlChar_Get(a),2);
            fprintf(stderr,"tag relay fixture: actor=%d beam=%d type=%d kind=%d cost=%d\n",a,
                TagRelay_FirstBeam(a),BtlSuper_GetType(BtlChar_Get(a),2),BtlSuper_GetHitDirKind(BtlChar_Get(a),2),sRelayTestCost[i]);
        }
        if(sRelayTestCase==8){
#ifdef BT4_TAG_TEAM_PREVIEW
            extern void Port_DevRulesRequest(unsigned,s32);extern unsigned Port_DevRulesRequested(void);
            Port_DevRulesRequest(31,1);
            TagRelay_Check("test_build_rejects_dev_rule_requests",Port_DevRulesRequested()==0);
            TagRelay_Check("test_build_has_no_shared_cheats",!DevRules_Online()&&!DevRules_Flags());
#else
            TagRelay_Check("requires_normal_test_build",0);
#endif
            fprintf(stderr,"tag relay result: case=8 %s\n",sRelayTestFailed?"FAIL":"PASS");fflush(NULL);exit(sRelayTestFailed?3:0);
        }
        if(sRelayTestCase==6){
            TagRelay_Hit(0,1,3,0xB);TagRelay_Check("ordinary_hit_does_not_start_chain",!sTagRelay[1].state);
            TagRelay_Hit(0,2,17,0xD5);TagRelay_Check("friendly_hit_does_not_start_chain",!sTagRelay[2].state);
            TagRelay_Check("guard_and_armor_not_converted",TagRelay_Reaction(0,1,1,0xD5)==1&&TagRelay_Reaction(0,1,2,0xD5)==2);
            TagRelay_Reset();TagRelay_Check("restart_clears_all_cues",!sTagRelay[1].state&&sTagRelayAction[0]==-1&&sTagRelayStrike[0]==-1);
            fprintf(stderr,"tag relay result: case=6 %s\n",sRelayTestFailed?"FAIL":"PASS");fflush(NULL);exit(sRelayTestFailed?3:0);
        }
        BtlAct_Request(BtlChar_Get(0),0x52);
    }
    if(sRelayTestCase!=5&&sRelayTestTick>25&&TagRelay_Eligible(0,1)){
        gPad[0].gameHeld=gPad[0].gamePressed=PADG_CROSS;gPad[0].held=gPad[0].pressed=PAD_CROSS;
    }
    if(sRelayTestCase==7&&TagRelay_Eligible(2,1)&&sTagRelay[1].age>=3){
        gPad[1].gameHeld=gPad[1].gamePressed=PADG_CROSS;gPad[1].held=gPad[1].pressed=PAD_CROSS;
    }
    /* The first launcher follows the fight with an ordinary native dash,
     * rather than standing hundreds of units behind the travelling victim. */
    if(sRelayTestCase!=5&&sTagRelay[1].state==1&&sTagRelay[1].last==0&&TagAssist_Idle(0))
        BtlAct_Request(BtlChar_Get(0),0x19);
    if(sRelayTestCase==7&&sTagRelay[1].state==1&&sTagRelay[1].last==2&&TagAssist_Idle(2))
        BtlAct_Request(BtlChar_Get(2),0x19);
    if(sRelayTestCase==5&&sRelayTestTick==26){
        /* Expiry test uses a human ally who never accepts. */
        BattleSide *ally=TagBattle_ExtraSide(2);ally->control=BTL_CONTROL_PAD;ally->pad=1;
        *(s32 *)((u8 *)BtlChar_Get(2)+4)=1;*(s32 *)((u8 *)BtlChar_Get(2)+0x1278)=0;
    }
    if(sRelayTestCase==2&&sTagRelay[1].links==3)BtlMember_GetActiveGauge(BtlChar_Get(2))[3]=0;
    if(sRelayTestCase==4){
        if(sRelayTestTick==60){sRelayTestPause=sTagRelay[1].ticks;Battle_GetWork()->flags|=BATTLE_FLAG_PAUSE;}
        if(sRelayTestTick==90){TagRelay_Check("pause_freezes_window",sRelayTestPause==sTagRelay[1].ticks);Battle_GetWork()->flags&=~BATTLE_FLAG_PAUSE;}
    }
}
static void TagRelay_TestInput(void){
    if(!TagRelay_TestOn())return;
    extern void BtlCharApi_SetInjectedInput(s32,u32,f32,f32);
    for(s32 i=0;i<TagBattle_Count();++i)if(BattleSide_GetControl(i)!=BTL_CONTROL_PAD)
        BtlCharApi_SetInjectedInput(BattleSide_GetObjId(i),0,0,0);
}
static void TagRelay_TestAfter(void){
    if(!TagRelay_TestOn())return;
    s32 i;
    if(sRelayTestTick==150||sRelayTestTick==210){extern void Port_TagUiCapture(s32);Port_TagUiCapture(sRelayTestTick==150?101:102);}
    if(sTagRelay[1].links>sRelayTestMax){sRelayTestMax=sTagRelay[1].links;
        fprintf(stderr,"tag relay debug: idle=%d,%d distance=%d,%d target=%d,%d eligible=%d,%d pair=%d,%d\n",
            TagAssist_Idle(0),TagAssist_Idle(2),(s32)sqrtf(TagAssist_Distance(0,1)),(s32)sqrtf(TagAssist_Distance(2,1)),TagBattle_Target(0),TagBattle_Target(2),TagRelay_Eligible(0,1),TagRelay_Eligible(2,1),TagBattle_Pair(0),TagBattle_Pair(2));
        extern void Port_TagUiCapture(s32);Port_TagUiCapture(97+sRelayTestMax);}
    if(sRelayTestTick%30==0)fprintf(stderr,"tag relay trace: tick=%d actions=%X,%X,%X links=%d state=%d age=%d time=%d\n",
        sRelayTestTick,BtlAct_GetCurrent(BtlChar_Get(0)),BtlAct_GetCurrent(BtlChar_Get(1)),BtlAct_GetCurrent(BtlChar_Get(2)),
        sTagRelay[1].links,sTagRelay[1].state,sTagRelay[1].age,sTagRelay[1].ticks);
    if(sRelayTestTick%15==0&&sRelayTestTick<150){
        extern s32 BtlAtk_GetId(void *);extern f32 BtlOpp_GetYawFromFacing(void *);
        f32 p[4],q[4];BtlCharApi_GetBasePos(BattleSide_GetObjId(0),p);BtlCharApi_GetBasePos(BattleSide_GetObjId(1),q);
        fprintf(stderr,"tag relay positions: tick=%d own=%.0f,%.0f,%.0f target=%.0f,%.0f,%.0f gap=%d travel=%d free=%d frozen=%d\n",sRelayTestTick,p[0],p[1],p[2],q[0],q[1],q[2],(s32)sqrtf(TagAssist_Distance(0,1)),sTagRelayTravel[0],BtlChar_IsFree(BtlChar_Get(0)),BtlChar_IsFrozen(BtlChar_Get(0)));
        fprintf(stderr,"tag relay contact gates: id=%X handled=%d contact=%d hit=%d miss=%d atkvoid=%d defvoid=%d yaw=%.2f marker=%d\n",
            BtlAtk_GetId(BtlChar_Get(0)),BtlChar_TestFlag(BtlChar_Get(0),0x60),BtlChar_TestFlag(BtlChar_Get(0),0x65),BtlChar_TestFlag(BtlChar_Get(0),0x5B),BtlChar_TestFlag(BtlChar_Get(0),0x5C),BtlChar_TestFlag(BtlChar_Get(0),0xBA),BtlChar_TestFlag(BtlChar_Get(1),0x42),BtlOpp_GetYawFromFacing(BtlChar_Get(0)),sTagRelayStrike[0]);
    }
    if(sRelayTestTick>=95&&sRelayTestTick<=115){
        u8 *c=(u8 *)BtlChar_Get(1);
        fprintf(stderr,"tag relay victim: tick=%d act=%X request=%X react=%X frame=%d timer=%d boundary=%d\n",sRelayTestTick,BtlAct_GetCurrent(c),BtlAct_GetRequested(c),*(s32 *)(c+0xFB0),*(s32 *)(c+0x964),*(s32 *)(c+0x3D8),BtlChar_TestFlag(c,0x14));
    }
    for(i=0;i<2;++i){s32 a=i?2:0,action=BtlAct_GetCurrent(BtlChar_Get(a));
        if(action>=0x106&&action<=0x111&&!(sRelayTestBeam&(1<<i))){
            sRelayTestBeam|=1<<i;
            TagRelay_Check(i?"helper_first_native_move_cost":"leader_first_native_move_cost",
                BtlSide_GetKi(a)<=sRelayTestKi[i]-sRelayTestCost[i]+1000);
        }
    }
    if(BtlCharApi_GetHp(1)<200000)sRelayTestDamage=1;
    if(sRelayTestTick==660){
        if(sRelayTestCase==5)TagRelay_Check("no_helper_no_links",sRelayTestMax==0&&!sTagRelay[1].state);
        else{
            TagRelay_Check("three_real_alternating_launches",sRelayTestMax==3);
            TagRelay_Check("native_melee_dealt_damage",sRelayTestDamage);
            TagRelay_Check(sRelayTestCase==2?"no_ki_no_finisher":"both_first_beams_started",sRelayTestCase==2?!sRelayTestBeam:sRelayTestBeam==3);
        }
        fprintf(stderr,"tag relay result: case=%d %s\n",sRelayTestCase,sRelayTestFailed?"FAIL":"PASS");fflush(NULL);exit(sRelayTestFailed?3:0);
    }
}
