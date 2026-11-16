/* Isolated native integration fixtures, opt-in only with BT4_RENDER_TEST. */
static s32 sJointTestTick,sJointTestCase,sJointTestFired,sJointTestDamage,sJointTestFailed,sJointTestPartner=-1;
static s32 sJointTestKi[2],sJointTestCost[2],sJointTestSlot[2];
static s32 sJointTestPauseWait;
extern s32 BtlSuper_GetKiCost(void *,s32),BtlSuper_GetId(void *,s32);
extern void BtlChar_SetFlag(void *,s32),BtlAct_Request(void *,s32);
extern s32 BtlStage_FindZoneNear(s32,Vec4 *);
extern s32 BtlStage_GetStartPlace(s32,Vec4 *,Vec4 *,s32);
static void TagJoint_Check(const char *name,s32 condition){
    fprintf(stderr,"tag joint check: %s=%s\n",name,condition?"PASS":"FAIL");
    if(!condition)sJointTestFailed=1;
}
static void TagJoint_Place(s32 actor,f32 x,f32 z){
    f32 p[4]={0,0,0,1},r[4]={0,0,0,0};
    BtlStage_GetStartPlace(TagBattle_Allied(actor,0)?0:1,(Vec4 *)p,(Vec4 *)r,0);
    p[0]+=x;p[2]+=z;
    BtlChar_Place((BtlCtlChr *)BtlChar_Get(actor),(Vec4 *)p,(Vec4 *)r,BtlStage_FindZoneNear(-1,(Vec4 *)p));
}
static void TagJoint_PlaceInBeam(s32 actor,f32 along,f32 lateral){
    f32 p[4],q[4],r[4]={0,0,0,0},dx,dz,n;
    BtlCharApi_GetBasePos(BattleSide_GetObjId(0),p);
    BtlCharApi_GetBasePos(BattleSide_GetObjId(1),q);
    dx=q[0]-p[0];dz=q[2]-p[2];n=sqrtf(dx*dx+dz*dz);
    p[0]+=dx*along+dz/n*lateral;p[2]+=dz*along-dx/n*lateral;
    BtlChar_Place((BtlCtlChr *)BtlChar_Get(actor),(Vec4 *)p,(Vec4 *)r,BtlStage_FindZoneNear(-1,(Vec4 *)p));
}
static s32 TagJoint_TestOn(void){return getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_ASSIST_TEST")&&BtlSeq_IsFighting();}
static void TagJoint_TestInput(void){
    extern void BtlCharApi_SetInjectedInput(s32,u32,f32,f32);
    s32 i;
    if(!TagJoint_TestOn())return;
    if(atoi(getenv("BT4_TAG_ASSIST_TEST"))==13&&sJointTestTick>210)return;
    for(i=0;i<TagBattle_Count();++i)if(BattleSide_GetControl(i)!=BTL_CONTROL_PAD)
        BtlCharApi_SetInjectedInput(BattleSide_GetObjId(i),0,0,0);
}
static void TagJoint_TestPads(void){
    s32 i;
    if(!TagJoint_TestOn())return;
    sJointTestCase=atoi(getenv("BT4_TAG_ASSIST_TEST"));++sJointTestTick;
    if(sJointTestTick==1&&sJointTestCase>=14&&sJointTestCase<=19&&TagBattle_Count()!=4){
        fprintf(stderr,"tag joint fixture: cases 14..19 require exactly four actor slots\n");exit(2);
    }
    if(sJointTestCase==13&&(sJointTestTick==1||sJointTestTick==210||sJointTestTick==840)){
        extern void Port_TagJointRecordMark(s32);Port_TagJointRecordMark(sJointTestTick);
    }
    for(i=0;i<PAD_COUNT;++i){gPad[i].held=gPad[i].pressed=gPad[i].repeat=0;gPad[i].gameHeld=gPad[i].gamePressed=gPad[i].gameRepeat=0;}
    if(sJointTestTick==10){
        for(i=0;i<TagBattle_Count();++i){
            s32 *g=BtlMember_GetActiveGauge(BtlChar_Get(i));
            g[0]=g[1]=sJointTestCase==13?50000:200000;g[3]=g[4];
            BtlAct_Request(BtlChar_Get(i),0xB);
            TagJoint_Place(i,i>=2?24.f:0.f,0.f);
            if(sJointTestCase==12){
                g[1]=i==3?100000:40000;
                g[0]=i==0?35000:i==1?25000:i==4?0:i==3?87500:15000+i*2000;
                TagJoint_Place(i,(i/2)*70.f,(i/2)*30.f);
            }
            if(i!=0&&TagBattle_Allied(i,0)&&sJointTestPartner<0)sJointTestPartner=i;
        }
        TagBattle_SetTarget(0,sJointTestCase==18?2:1);
        if(sJointTestCase==18){
            TagJoint_Place(1,24.f,0);
            TagJoint_Check("cooperative_human_teams",TagBattle_Allied(0,1)&&!TagBattle_Allied(0,2)&&!TagBattle_Allied(0,3));
            TagJoint_Check("two_native_controllers",BattleSide_GetControl(0)==BTL_CONTROL_PAD&&BattleSide_GetControl(1)==BTL_CONTROL_PAD&&BattleSide_GetPad(0)==0&&BattleSide_GetPad(1)==1);
        }
        if(sJointTestCase==14&&TagBattle_Count()==4){TagBattle_SetTarget(1,0);TagBattle_SetTarget(3,0);}
        if(sJointTestCase==15&&TagBattle_Count()==4){
            BattleSide *human=TagBattle_ExtraSide(3);
            human->control=BTL_CONTROL_PAD;human->pad=1;
            *(s32 *)((u8 *)BtlChar_Get(3)+4)=1;
            *(s32 *)((u8 *)BtlChar_Get(3)+0x1278)=0;
            TagBattle_SetTarget(1,0);TagBattle_SetTarget(3,0);
        }
        if(sJointTestCase==16||sJointTestCase==17){
            TagJoint_PlaceInBeam(2,.25f,0);
            /* Some native energy moves expand into radius-120 spheres. The
             * exclusion fixture must be outside that authored area as well. */
            TagJoint_PlaceInBeam(3,.55f,sJointTestCase==17?240.f:0.f);
        }
        if(sJointTestCase==19){
            TagJoint_PlaceInBeam(1,.16f,0.f);
            TagJoint_PlaceInBeam(2,.04f,10.f);
            TagJoint_PlaceInBeam(3,.08f,-14.f);
        }
        if(sJointTestPartner>=0){
            TagBattle_SetTarget(sJointTestPartner,sJointTestCase==18?2:1);
            if(sJointTestCase==3||sJointTestCase==4||sJointTestCase==7){
                BattleSide *side=TagBattle_ExtraSide(sJointTestPartner);
                side->control=BTL_CONTROL_PAD;side->pad=1;
                *(s32 *)((u8 *)BtlChar_Get(sJointTestPartner)+4)=1;
                *(s32 *)((u8 *)BtlChar_Get(sJointTestPartner)+0x1278)=0;
            }
        }
    }
    if(sJointTestTick==25&&sJointTestCase==6&&sJointTestPartner>=0)TagJoint_Place(sJointTestPartner,900.f,-100.f);
    if(sJointTestTick==30){
        s32 p=sJointTestPartner;
        for(i=0;i<TagBattle_Count();++i)BtlChar_SetFlag(BtlChar_Get(i),5);
        for(i=0;i<TagBattle_Count();++i){void *c=BtlChar_Get(i);
            fprintf(stderr,"tag joint debug: actor=%d action=%X request=%X queued=%X free=%d frozen=%d slot2type=%d flags=%X can=%d slot3type=%d flags=%X can=%d\n",
                i,BtlAct_GetCurrent(c),BtlAct_GetRequested(c),BtlAct_GetQueued(c),BtlChar_IsFree(c),BtlChar_IsFrozen(c),
                BtlSuper_GetType(c,2),BtlSuper_GetFlags(c,2),BtlAct_CanUseTechnique(c,2),
                BtlSuper_GetType(c,3),BtlSuper_GetFlags(c,3),BtlAct_CanUseTechnique(c,3));}
        if(p>=0&&sJointTestCase==2)BtlMember_GetActiveGauge(BtlChar_Get(p))[3]=0;
        if(sJointTestCase==10)*(s32 *)((u8 *)BtlChar_Get(0)+0xE40)=60;
        sJointTestKi[0]=BtlSide_GetKi(0);
        sJointTestKi[1]=p>=0?BtlSide_GetKi(p):0;
        sJointTestSlot[0]=TagAssist_Technique(0);
        sJointTestSlot[1]=p>=0?TagAssist_Technique(p):-1;
        for(i=0;i<2;++i)if(sJointTestSlot[i]>=0)sJointTestCost[i]=BtlSuper_GetKiCost(BtlChar_Get(i?p:0),sJointTestSlot[i]);
        fprintf(stderr,"tag joint fixture: case=%d partner=%d slots=%d,%d moves=%d,%d ki=%d,%d costs=%d,%d\n",
            sJointTestCase,p,sJointTestSlot[0],sJointTestSlot[1],
            sJointTestSlot[0]>=0?BtlSuper_GetId(BtlChar_Get(0),sJointTestSlot[0]):-1,
            p>=0&&sJointTestSlot[1]>=0?BtlSuper_GetId(BtlChar_Get(p),sJointTestSlot[1]):-1,
            sJointTestKi[0],sJointTestKi[1],sJointTestCost[0],sJointTestCost[1]);
        if((sJointTestCase!=12&&sJointTestCase<14)||sJointTestCase==18){gPad[0].held=PAD_L2|PAD_R3;gPad[0].pressed=PAD_R3;}
        if(sJointTestCase==16||sJointTestCase==17){
            TagJoint_Check("native_beam_queued",BtlAct_QueueTechnique(BtlChar_Get(0),2));
        }
        if(sJointTestCase==19){
            TagJoint_Check("native_wave_queued",BtlAct_QueueTechnique(BtlChar_Get(0),3));
        }
    }
    if(sJointTestCase!=12&&sJointTestCase<14&&sJointTestTick>30&&sJointTestTick<37)gPad[0].held=PAD_L2|PAD_R3;
    if(sJointTestCase==13&&sJointTestTick>240&&sJointTestTick<510)gPad[0].held=PAD_L2;
    if(sJointTestCase==11&&sJointTestTick>=34){
        gPad[0].held=0;
        if(sJointTestTick==34){
            TagAssist_Reset();
            TagJoint_Check("restart_clears_joint_state",!sTagAssist[0].wait&&!sTagAssist[0].cooldown&&
                sTagAssist[0].launch==-1&&sTagAssist[0].partner==-1&&!sTagAssist[sJointTestPartner].wait);
        }
    }
    if((sJointTestCase==4||sJointTestCase==18)&&sJointTestTick==34){gPad[1].held=PAD_R3;gPad[1].pressed=PAD_R3;}
    if(sJointTestCase==5&&sJointTestTick==34&&sJointTestPartner>=0)
        BtlMember_GetActiveGauge(BtlChar_Get(sJointTestPartner))[0]=0;
    if(sJointTestCase==7){
        if(sJointTestTick==35){sJointTestPauseWait=sTagAssist[0].wait;Battle_GetWork()->flags|=BATTLE_FLAG_PAUSE;}
        if(sJointTestTick==135)TagJoint_Check("pause_keeps_invitation_time",sTagAssist[0].wait==sJointTestPauseWait);
        if(sJointTestTick==136)Battle_GetWork()->flags&=~BATTLE_FLAG_PAUSE;
    }
    if(((sJointTestCase!=12&&sJointTestCase<14)||sJointTestCase==18)&&sJointTestTick==31){
        s32 accepted=sJointTestCase!=2&&sJointTestCase!=6&&sJointTestCase!=9&&sJointTestCase!=10&&sJointTestPartner>=0;
        TagJoint_Check("request_admission",(sTagAssist[0].wait>0)==accepted);
        if(accepted)TagJoint_Check("owned_R3_still_consumed",sTagAssist[0].consume);
    }
}
static void TagJoint_TestAfter(void){
    s32 a[2]={0,sJointTestPartner},i;
    if(!TagJoint_TestOn())return;
    if(sJointTestCase==16||sJointTestCase==17||sJointTestCase==19){
        if(sJointTestTick==330){
            fprintf(stderr,"tag beam health: owner=%d ally=%d locked=%d crossing=%d\n",BtlCharApi_GetHp(0),BtlCharApi_GetHp(2),BtlCharApi_GetHp(1),BtlCharApi_GetHp(3));
            TagJoint_Check("locked_enemy_hit",BtlCharApi_GetHp(1)<200000);
            TagJoint_Check(sJointTestCase==17?"enemy_outside_beam_unharmed":"unlocked_enemy_hit",
                sJointTestCase==17?BtlCharApi_GetHp(3)==200000:BtlCharApi_GetHp(3)<200000);
            TagJoint_Check("ally_in_beam_unharmed",BtlCharApi_GetHp(2)==200000);
            TagJoint_Check("owner_unharmed",BtlCharApi_GetHp(0)==200000);
            TagJoint_Check("beam_keeps_selected_target",TagBattle_Target(0)==1);
            fprintf(stderr,"tag joint result: case=%d %s\n",sJointTestCase,sJointTestFailed?"FAIL":"PASS");fflush(NULL);exit(sJointTestFailed?3:0);
        }
        return;
    }
    if(sJointTestCase==15){
        if(sJointTestTick==135){
            TagJoint_Check("CPU_does_not_recruit_human",sTagAssist[1].partner==-1&&!sTagAssist[1].wait);
            /* Reset represents no partner as -1, never as actor zero. */
            TagJoint_Check("human_keeps_control",sTagAssist[3].partner==-1&&!sTagAssist[3].wait&&
                sTagAssist[3].launch==-1&&BattleSide_GetControl(3)==BTL_CONTROL_PAD);
            fprintf(stderr,"tag joint result: case=15 %s\n",sJointTestFailed?"FAIL":"PASS");fflush(NULL);exit(sJointTestFailed?3:0);
        }
        return;
    }
    if(sJointTestCase==12){
        if(sJointTestTick==75)TagBattle_SetTarget(0,6);
        if(sJointTestTick==95)Battle_GetWork()->flags|=BATTLE_FLAG_PAUSE;
        if(sJointTestTick==120)Battle_GetWork()->flags&=~BATTLE_FLAG_PAUSE;
        if(sJointTestTick==55||sJointTestTick==80||sJointTestTick==110||sJointTestTick==140){
            extern void Port_TagUiCapture(s32);
            Port_TagUiCapture(98+(sJointTestTick==55?0:sJointTestTick==80?1:sJointTestTick==110?2:3));
        }
        if(sJointTestTick==150){
            TagJoint_Check("ten_independent_actors",TagBattle_Count()==10);
            TagJoint_Check("own_partial_health_preserved",BtlCharApi_GetHp(0)==35000);
            TagJoint_Check("target_switch_persists",TagBattle_Target(0)==6);
            TagJoint_Check("ko_ally_remains_ko",BtlCharApi_GetHp(4)==0);
            fprintf(stderr,"tag joint result: case=12 %s\n",sJointTestFailed?"FAIL":"PASS");fflush(NULL);exit(sJointTestFailed?3:0);
        }
        return;
    }
    if(sJointTestTick==30&&(sJointTestCase==9||sJointTestCase==10))
        TagJoint_Check("ordinary_R3_preserved",!sTagAssist[0].consume&&(gPad[0].held&PAD_R3));
    if(sJointTestCase==14){
        a[0]=1;a[1]=3;
        if(sJointTestTick==31){
            for(i=0;i<2;++i){
                sJointTestSlot[i]=TagAssist_Technique(a[i]);
                sJointTestKi[i]=BtlSide_GetKi(a[i]);
                sJointTestCost[i]=sJointTestSlot[i]>=0?BtlSuper_GetKiCost(BtlChar_Get(a[i]),sJointTestSlot[i]):0;
            }
        }
    }
    if(sJointTestCase==1||sJointTestCase==4||sJointTestCase==13||sJointTestCase==14||sJointTestCase==18){
        for(i=0;i<2;++i)if(a[i]>=0&&!(sJointTestFired&(1<<i))){
            s32 action=BtlAct_GetCurrent(BtlChar_Get(a[i]));
            if(action>=0x106&&action<=0x111){
                if(i==0&&sJointTestCase!=14&&BattleSide_GetControl(sJointTestPartner)!=BTL_CONTROL_PAD){
                    f32 p[4],q[4],t[4],dx,dz,n,behind,side;
                    BtlCharApi_GetBasePos(BattleSide_GetObjId(0),p);
                    BtlCharApi_GetBasePos(BattleSide_GetObjId(sJointTestPartner),q);
                    BtlCharApi_GetBasePos(BattleSide_GetObjId(1),t);
                    dx=t[0]-p[0];dz=t[2]-p[2];n=sqrtf(dx*dx+dz*dz);dx/=n;dz/=n;
                    behind=(q[0]-p[0])*dx+(q[2]-p[2])*dz;
                    side=(q[0]-p[0])*dz-(q[2]-p[2])*dx;
                    TagJoint_Check("helper_behind_leader",behind < -3.0f);
                    TagJoint_Check("helper_outside_beam",side*side > 14.0f*14.0f);
                    TagJoint_Check("helper_near_leader",behind*behind+side*side < 35.0f*35.0f);
                    fprintf(stderr,"tag joint formation: forward=%.2f lateral=%.2f leader=%.2f,%.2f,%.2f helper=%.2f,%.2f,%.2f\n",
                        behind,side,p[0],p[1],p[2],q[0],q[1],q[2]);
                }
                sJointTestFired|=1<<i;
                TagJoint_Check(i?"ally_native_ki_cost":"leader_native_ki_cost",
                    BtlSide_GetKi(a[i])<=sJointTestKi[i]-sJointTestCost[i]+1000);
                fprintf(stderr,"tag joint native: actor=%d action=%X ki=%d expected=%d\n",a[i],action,BtlSide_GetKi(a[i]),sJointTestKi[i]-sJointTestCost[i]);
            }
        }
        if(sJointTestTick>30&&BtlCharApi_GetHp(sJointTestCase==14?0:sJointTestCase==18?2:1)<(sJointTestCase==13?50000:200000))sJointTestDamage=1;
        if(sJointTestTick==55||sJointTestTick==75||sJointTestTick==95||sJointTestTick==115){
            extern void Port_TagUiCapture(s32);Port_TagUiCapture(98+(sJointTestTick-55)/20);
        }
        if(sJointTestTick==(sJointTestCase==13?840:330)){
            TagJoint_Check("both_native_actions_started",sJointTestFired==3);
            TagJoint_Check("native_attack_dealt_damage",sJointTestDamage);
            TagJoint_Check("actors_alive_after_attack",BtlCharApi_GetHp(a[0])>0&&BtlCharApi_GetHp(a[1])>0);
            if(sJointTestCase!=13)TagJoint_Check("returned_from_techniques",BtlAct_GetCurrent(BtlChar_Get(a[0]))<0xFD&&BtlAct_GetCurrent(BtlChar_Get(a[1]))<0xFD);
            fprintf(stderr,"tag joint result: case=%d %s\n",sJointTestCase,sJointTestFailed?"FAIL":"PASS");fflush(NULL);exit(sJointTestFailed?3:0);
        }
    }else if(sJointTestTick==(sJointTestCase==7?260:135)){
        TagJoint_Check("no_launch_or_cooldown",!sTagAssist[0].active&&!sTagAssist[0].cooldown&&!sTagAssist[0].wait);
        TagJoint_Check("no_ki_deducted",BtlSide_GetKi(0)>=sJointTestKi[0]);
        fprintf(stderr,"tag joint result: case=%d %s\n",sJointTestCase,sJointTestFailed?"FAIL":"PASS");fflush(NULL);exit(sJointTestFailed?3:0);
    }
}
