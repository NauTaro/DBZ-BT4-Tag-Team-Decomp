/* Isolated native fixtures. Never enabled without the private render-test flag. */
static s32 sFusionTestTick,sFusionTestFailed,sFusionTestStarted,sFusionTestPlayable,sFusionTestIndex=-1;
static s32 sFusionTestCase,sFusionTestAt,sFusionTestSaved,sFusionTestInjected,sFusionTestClash;
static s32 sCpuTestActor=-1,sCpuTestAlly=-1,sCpuTestInvites,sCpuTestPausing,sCpuTestCaptured;
static s32 fusion_test_on(void){return getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_FUSION_TEST")&&BtlSeq_IsFighting();}
static s32 fusion_test_allows_cpu(void){
    if(!getenv("BT4_RENDER_TEST"))return 1;
    if(getenv("BT4_TAG_FUSION_TEST"))return sFusionTestCase>=30;
    return !getenv("BT4_TAG_ASSIST_TEST") && !getenv("BT4_TAG_REVIVE_TEST") &&
        !getenv("BT4_TAG_SPLIT_SPECTATOR_TEST");
}
static void fusion_check(const char *name,s32 ok){fprintf(stderr,"tag fusion check: %s=%s\n",name,ok?"PASS":"FAIL");if(!ok)sFusionTestFailed=1;}
static void fusion_test_cpu_place(s32 actor,f32 offset){
    extern void BtlChar_Place(BtlMgrChr *,f32 *,f32 *,s32);
    extern s32 BtlStage_GetStartPlace(s32,f32 *,f32 *,s32),BtlStage_FindZoneNear(s32,f32 *);
    f32 p[4],r[4];
    BtlStage_GetStartPlace(0,p,r,0);
    p[0]+=offset;
    BtlChar_Place(BtlChar_Get(actor),p,r,BtlStage_FindZoneNear(-1,p));
}
static void fusion_test_cpu_pads(void){
    extern void TagFusion_TestPause(s32);
    if(sFusionTestTick==15){
        for(s32 i=0;i<TagBattle_Count();++i){
            BtlMgrChr *c=BtlChar_Get(i);
            BtlMgrGauge *g=&BtlMember_GetActive(c)->gauge;
            g->health=25000;g->healthMax=40000;g->ki=65000;
            g->blast=g->blastMax=500000;c->unk1300=1;
            BtlAct_ClearQueue(c);BtlAct_Request(c,0xB);
            fusion_test_cpu_place(i,TagBattle_Allied(0,i)?25.f*i:360.f+20.f*i);
        }
        if(sFusionTestCase==32)fusion_test_cpu_place(1,38.f);
    }
    if(sFusionTestCase==32 && sFusionTestTick==235)fusion_test_cpu_place(1,400.f);
    if(sCpuTestActor>=0 && sFusion[sCpuTestActor].state==F_INVITE){
        if(sFusionTestCase==30 && !sCpuTestPausing && sFusionTestTick==sFusionTestAt+6){
            sCpuTestPausing=1;
            sFusionTestSaved=sFusion[sCpuTestActor].ticks;
            TagFusion_TestPause(1);
        }
        if(sCpuTestPausing && sFusionTestTick==sFusionTestAt+21){
            fusion_check("pause_keeps_invitation_timer",sFusion[sCpuTestActor].ticks==sFusionTestSaved);
            sCpuTestPausing=0;
            TagFusion_TestPause(0);
        }
        if(sFusionTestCase==30 && sFusionTestTick==sFusionTestAt+26)gPad[0].held=gPad[0].pressed=PAD_R3;
    }
    if(sFusionTestCase==30 && sFusionTestPlayable && sFusionTestTick==sFusionTestAt+45){
        gPad[0].held=PAD_L3|PAD_R3;
        gPad[0].pressed=PAD_R3;
    }
    if(sFusionTestCase==30 && sFusionTestPlayable && sFusionTestTick==sFusionTestAt+28){
        extern u32 BtlInput_GetKeyMask(BtlMgrChr *,s32);
        gPad[0].held=BtlInput_GetKeyMask(BtlChar_Get(sCpuTestActor),0);
    }
    if(!sCpuTestCaptured && sCpuTestActor>=0 && sFusionTestTick==sFusionTestAt+7 && sFusionTestCase==31){
        extern void Port_TagUiCapture(s32);
        Port_TagUiCapture(171);sCpuTestCaptured=1;
    }
    if(sFusionTestTick==1100){
        if(sFusionTestCase==31){
            fusion_check("ignored_invites_never_fuse",!sFusionTestStarted && !sFusionTestPlayable && sCpuTestInvites>=2);
        }else{
            fusion_check("cpu_initiated_native_fusion",sFusionTestStarted && sFusionTestPlayable);
            if(sCpuTestActor>=0 && sCpuTestAlly>=0){
                fusion_check("cpu_and_ally_return_to_original_slots",!sFusion[sCpuTestActor].state &&
                    !sFusion[sCpuTestAlly].state && BtlChar_Get(sCpuTestActor)->injectOn &&
                    BtlChar_Get(sCpuTestAlly)->injectOn==(sFusionTestCase==32));
            }
        }
        fprintf(stderr,"tag fusion result: %s\n",sFusionTestFailed?"FAIL":"PASS");
        exit(sFusionTestFailed?2:0);
    }
}
static void fusion_test_pads(void){
    s32 i,j;if(!fusion_test_on())return;++sFusionTestTick;sFusionTestCase=atoi(getenv("BT4_TAG_FUSION_TEST"));
    if(sFusionTestCase==18)sFusionTestCase=8;
    if(sFusionTestCase==28)sFusionTestCase=8;
    for(i=0;i<PAD_COUNT;++i){gPad[i].held=gPad[i].pressed=gPad[i].repeat=0;gPad[i].gameHeld=gPad[i].gamePressed=gPad[i].gameRepeat=0;}
    if(sFusionTestCase>=30){fusion_test_cpu_pads();return;}
    if(sFusionTestTick==15){
        extern void BtlChar_Place(BtlMgrChr *,f32 *,f32 *,s32);
        extern s32 BtlStage_GetStartPlace(s32,f32 *,f32 *,s32),BtlStage_FindZoneNear(s32,f32 *);
        f32 p[4],r[4];
        for(i=0;i<TagBattle_Count();++i){BtlMgrChr *c=BtlChar_Get(i);BtlMgrGauge *g=&BtlMember_GetActive(c)->gauge;
            g->healthMax=i==0?40000:60000;g->health=i==0?25000:35000;
            if(sFusionTestCase==10){g->healthMax*=100;g->health*=100;}
            g->ki=65000;g->blast=g->blastMax=500000;c->unk1300=1;
            BtlAct_ClearQueue(c);BtlAct_Request(c,0xB);
            BtlStage_GetStartPlace(TagBattle_Allied(0,i)?0:1,p,r,0);if(i==1){p[0]+=25;}
            BtlChar_Place(c,p,r,BtlStage_FindZoneNear(-1,p));
        }
        if(sFusionTestCase==15){Battle_GetSetup()->sides[1].control=2;BtlChar_Get(1)->injectOn=1;}
    }
    if(sFusionTestTick==30){
        extern s32 BtlParam_GetUnkBA(BtlMgrChr *,s32,s32);
        for(i=0;i<3;++i){
            fprintf(stderr,"tag fusion fixture: actor=0 index=%d result=%d sequence=%d cost=%d compatible=%d partners=",i,BtlParam_GetUnkB4(BtlChar_Get(0),i),BtlParam_GetUnkB1(BtlChar_Get(0),i),BtlParam_GetCostAE(BtlChar_Get(0),i),fusion_compatible(0,1,i));
            for(j=0;j<4;++j)fprintf(stderr,"%d,",BtlParam_GetUnkBA(BtlChar_Get(0),i,j));fprintf(stderr,"\n");
            if(fusion_compatible(0,1,i)&&(sFusionTestIndex<0||BtlParam_GetUnkB1(BtlChar_Get(0),i)==1))sFusionTestIndex=i;
        }
        fusion_check("native_compatible_pair",sFusionTestIndex>=0);
        if(sFusionTestCase==5||sFusionTestCase==12)sFusionTestIndex=0;
        if(sFusionTestCase==3){extern void BtlChar_Place(BtlMgrChr *,f32 *,f32 *,s32);extern s32 BtlStage_FindZoneNear(s32,f32 *);f32 p[4],r[4]={0,0,0,0};BtlCharApi_GetBasePos(BattleSide_GetObjId(1),p);p[0]+=500;BtlChar_Place(BtlChar_Get(1),p,r,BtlStage_FindZoneNear(-1,p));}
        if(sFusionTestCase==4)BtlMember_GetActive(BtlChar_Get(0))->gauge.blast=0;
        for(i=1;i<atoi(getenv("BT4_TAG_FUSION_TEST"));++i)BtlChar_Rand();
        if(sFusionTestIndex>=0&&sFusionTestCase!=14)TagFusion_Request(0,sFusionTestIndex);
        if(sFusionTestCase==16){
            for(i=0;i<3;++i)if(fusion_compatible(2,3,i)){TagFusion_Request(2,i);break;}
            fusion_check("second_native_pair_invited",sFusion[2].state==F_INVITE);
        }
    }
    if(sFusionTestTick==50&&sFusion[1].state==F_INVITE&&sFusionTestCase!=2){gPad[1].held=gPad[1].pressed=PAD_R3;}
    if(sFusionTestCase==14&&sFusionTestTick>=40&&sFusionTestTick<=54){gPad[0].held=PAD_R3|PAD_RIGHT;gPad[0].gameHeld=PADG_R3|PADG_RIGHT;}
    if(sFusionTestCase==14&&sFusionTestTick==80){gPad[1].held=gPad[1].pressed=PAD_R3;}
    if(sFusionTestPlayable&&sFusionTestCase==8&&sFusionTestTick==sFusionTestAt+20){
        extern u32 BtlInput_GetKeyMask(BtlMgrChr *,s32);
        for(i=0;i<2;++i){BtlMgrChr *c=BtlChar_Get(i);
            gPad[c->pad].held=gPad[c->pad].pressed=BtlInput_GetKeyMask(c,0)|BtlInput_GetKeyMask(c,3);}
    }
    if(sFusionTestPlayable&&sFusionTestCase==8&&sFusionTestTick==sFusionTestAt+24)
        for(i=0;i<2;++i)gPad[BtlChar_Get(i)->pad].held=gPad[BtlChar_Get(i)->pad].pressed=PAD_CIRCLE;
    if(sFusionTestPlayable&&sFusionTestCase==11&&sFusionTestTick==sFusionTestAt+60){
        Pad *pad=&gPad[BtlChar_Get(0)->pad];pad->held=PAD_L3|PAD_R3;pad->pressed=PAD_R3;
    }
    if(sFusionTestPlayable&&sFusionTestCase==9){
        extern void TagFusion_TestPause(s32);
        if(sFusionTestTick==sFusionTestAt+20){sFusionTestSaved=sFusion[0].ticks;TagFusion_TestPause(1);}
        if(sFusionTestTick==sFusionTestAt+50){fusion_check("pause_freezes_fusion_timer",sFusion[0].ticks==sFusionTestSaved);TagFusion_TestPause(0);}
    }
    if(sFusionTestTick%120==0){BtlMgrChr *c=BtlChar_Get(0);fprintf(stderr,"tag fusion fixture: tick=%d state=%d,%d action=%X char=%d hp=%d max=%d\n",sFusionTestTick,sFusion[0].state,sFusion[1].state,c->action,BtlMember_GetActive(c)->chara,BtlMember_GetActive(c)->gauge.health,BtlMember_GetActive(c)->gauge.healthMax);}
    if(sFusionTestTick>1100){
        if(sFusionTestCase>=2&&sFusionTestCase<=4)fusion_check("no_unconsented_or_invalid_fusion",!sFusionTestStarted&&!sFusion[0].state&&!sFusion[1].state);
        else{
            fusion_check("native_playable",sFusionTestPlayable);
            if(sFusionTestCase==5)fusion_check("native_potara_is_permanent",sFusion[0].state==F_FUSED&&sFusion[0].total==0);
            else{
                BtlMgrChr *a=BtlChar_Get(0),*b=BtlChar_Get(1);BtlMgrGauge *x=&BtlMember_GetActive(a)->gauge,*y=&BtlMember_GetActive(b)->gauge;
                s32 scale=sFusionTestCase==10?100:1;
                fusion_check("separation_finished",!sFusion[0].state&&!sFusion[1].state);
                fusion_check("both_original_slots_and_controllers",a->pad==0&&b->pad==1&&a->memberCount==1&&b->memberCount==1);
                fusion_check("separation_health_distribution",x->health==(sFusionTestCase==17?0:sFusionTestCase==6?13000:sFusionTestCase==7?33000:25000)*scale&&y->health==(sFusionTestCase==17?0:sFusionTestCase==6?17000:sFusionTestCase==7?47000:35000)*scale);
                fusion_check("both_visible_and_vulnerable",!BtlChar_TestFlag(a,0xB)&&!BtlChar_TestFlag(b,0xB)&&!BtlChar_TestFlag(b,0x42));
            }
            if(sFusionTestCase==8)fusion_check("both_clash_inputs_tested",sFusionTestClash);
            if(sFusionTestCase==15)fusion_check("cpu_partner_control_restored",BattleSide_GetControl(1)==2&&BtlChar_Get(1)->injectOn==1);
            if(sFusionTestCase==16)fusion_check("both_fusions_completed_and_separated",!sFusion[2].state&&!sFusion[3].state&&BtlMember_GetActive(BtlChar_Get(2))->chara==46&&BtlMember_GetActive(BtlChar_Get(3))->chara==44);
        }
        fprintf(stderr,"tag fusion result: %s\n",sFusionTestFailed?"FAIL":"PASS");exit(sFusionTestFailed?2:0);
    }
}
void TagFusion_TestInput(void){
    s32 i;if(!fusion_test_on())return;
    extern void BtlCharApi_SetInjectedInput(s32,u32,f32,f32);
    for(i=0;i<TagBattle_Count();++i)if(BattleSide_GetControl(i)!=0)BtlCharApi_SetInjectedInput(BattleSide_GetObjId(i),0,0,0);
    if(atoi(getenv("BT4_TAG_FUSION_TEST"))==28&&sFusionTestTick==50){
        /* Exercise the other genuine random branch with a known native RNG
         * state. Production ownership still uses one unmodified native draw. */
        for(i=0;i<100;++i){u64 next=((u64)(u32)gBtlChars->unk18*714025+4096)%150889;if(next&1)break;BtlChar_Rand();}
    }
}
static void fusion_test_cpu_after(void){
    for(s32 actor=0;actor<TagBattle_Count();++actor){
        TagFusionSlot *s=&sFusion[actor];
        if(s->state!=F_INVITE || s->index<0 || s->ticks!=F_INVITE_TICKS)continue;
        if(sFusionTestCase==32 ? TagBattle_Team(actor)!=0 : s->peer!=0)continue;
        const s32 previous=sCpuTestInvites?sFusionTestAt:0;
        sCpuTestActor=actor;
        sCpuTestAlly=s->peer;
        sFusionTestAt=sFusionTestTick;
        ++sCpuTestInvites;
        fusion_check("cpu_invites_compatible_nearby_ally",BattleSide_GetControl(actor)==BTL_CONTROL_CPU &&
            TagBattle_Allied(actor,s->peer) && fusion_compatible(actor,s->peer,s->index));
        if(sFusionTestCase==32)
            fusion_check("cpu_waits_until_enemies_are_clear",sFusionTestTick>=235 &&
                 BattleSide_GetControl(s->peer)==BTL_CONTROL_CPU);
        else{
            s32 other,ticks,total;
            fusion_check("human_gets_full_consent_window",s->peer==0 &&
                TagFusion_Hud(0,&other,&ticks,&total)==2 && other==actor && ticks==F_INVITE_TICKS);
        }
        if(previous)fusion_check("ignored_invites_obey_cooldown",sFusionTestTick-previous>=F_CPU_RETRY);
    }
    if(sCpuTestActor<0)return;
    if(!sFusionTestStarted && sFusion[sCpuTestActor].state==F_NATIVE){
        sFusionTestStarted=1;
        fusion_check("native_start_waits_for_consent",sFusionTestCase==32 || sFusionTestTick>=sFusionTestAt+26);
    }
    if(!sFusionTestPlayable && sFusion[sCpuTestActor].state==F_FUSED){
        BtlMgrChr *c=BtlChar_Get(sCpuTestActor);
        BtlMgrGauge *g=&BtlMember_GetActive(c)->gauge;
        sFusionTestPlayable=1;
        sFusionTestAt=sFusionTestTick;
        fusion_check("native_fusion_keeps_hp_and_cost",g->health==50000 && g->healthMax==80000 &&
            g->blast>=sFusion[sCpuTestActor].blast && g->blast<=sFusion[sCpuTestActor].blast+3000);
        fusion_check("controller_matches_participants",sFusionTestCase==32?
            c->injectOn && TagFusion_Controller(sCpuTestActor)==sCpuTestActor:
            !c->injectOn && c->pad==0 && TagFusion_Controller(sCpuTestActor)==0 && TagBattle_ViewActor()==sCpuTestActor);
    }
    if(sFusionTestCase==30 && sFusionTestPlayable && sFusionTestTick==sFusionTestAt+28){
        extern s32 BtlInput_IsHeld(BtlMgrChr *,u32);
        fusion_check("human_pad_input_reaches_fused_fighter",BtlInput_IsHeld(BtlChar_Get(sCpuTestActor),0x8)); /* BTLB_RUSH */
    }
    if(sFusionTestCase==30 && sFusionTestPlayable && sFusionTestTick==sFusionTestAt+50)
        fusion_check("human_can_manually_separate_cpu_initiated_fusion",TagFusion_Separating(sCpuTestActor));
}
static void fusion_test_after(void){
    if(!fusion_test_on())return;
    if(sFusionTestCase>=30){fusion_test_cpu_after();return;}
    if(!sFusionTestStarted&&sFusion[0].state==F_NATIVE){sFusionTestStarted=1;fusion_check("ally_reserved_without_revival",TagFusion_Absent(1));}
    if(!sFusionTestPlayable&&sFusion[0].state==F_FUSED){
        BtlMgrChr *c=BtlChar_Get(0);BtlMgrGauge *g=&BtlMember_GetActive(c)->gauge;
        s32 scale=sFusionTestCase==10?100:1;
        sFusionTestPlayable=1;sFusionTestAt=sFusionTestTick;fusion_check("no_free_heal",g->health==60000*scale&&g->healthMax==100000*scale);
        fprintf(stderr,"tag fusion gauges: blast=%d expected_after_cost=%d ki=%d at_fusion=%d\n",g->blast,sFusion[0].blast,g->ki,sFusion[0].ki);
        fusion_check("native_fusion_cost_paid_once",g->blast>=sFusion[0].blast&&g->blast<=sFusion[0].blast+3000);
        fusion_check("no_ki_refill",g->ki<g->kiMax&&g->ki>=sFusion[0].ki&&g->ki<=sFusion[0].ki+10000);
        fusion_check("one_native_member",c->memberCount==1);
        fusion_check("no_friendly_collision",!TagBattle_CanInteract(1,2));
        fusion_check("partner_watches_fusion",TagFusion_ViewActor(1)==0);
        fusion_check("random_controller_has_native_pad",c->pad==(sFusion[0].controller==0?0:1)&&BtlChar_Get(1)->pad==(c->pad^1));
        if(atoi(getenv("BT4_TAG_FUSION_TEST"))==28)fusion_check("partner_wins_the_random_draw",sFusion[0].controller==1&&c->pad==1);
    }
    if(sFusionTestPlayable&&!sFusionTestInjected&&sFusionTestTick>=sFusionTestAt+10){
        extern s32 BtlMember_Damage(BtlMgrChr *,s32,s32);extern void BtlMember_AddHealth(BtlMgrChr *,s32);
        sFusionTestInjected=1;
        if(sFusionTestCase==6)BtlMember_Damage(BtlChar_Get(0),30000,0x1FB);
        if(sFusionTestCase==7)BtlMember_AddHealth(BtlChar_Get(0),20000);
        if(sFusionTestCase==17)BtlMember_Damage(BtlChar_Get(0),60000,0x1FB);
    }
    if(sFusionTestPlayable&&sFusionTestCase==8&&sFusionTestTick==sFusionTestAt+20){
        BtlMgrChr *a=BtlChar_Get(0),*b=BtlChar_Get(1);s32 old=a->action;
        a->action=0x130;fusion_check("spectator_beam_input_accepted",TagFusion_ClashExtra(0,0x50));
        fusion_check("controlling_player_beam_input_accepted",BtlInput_TestAction(a,0x50,1));
        a->action=0xFA;fusion_check("spectator_melee_input_accepted",TagFusion_ClashExtra(0,0x4F));
        a->action=old;
    }
    if(sFusionTestPlayable&&sFusionTestCase==8&&sFusionTestTick==sFusionTestAt+24){
        BtlMgrChr *a=BtlChar_Get(0);s32 old=a->action;
        a->action=0xFC;fusion_check("spectator_correct_aerial_button",TagFusion_ClashChoice(0,0,-1)==1);
        fusion_check("spectator_wrong_aerial_button",TagFusion_ClashChoice(0,1,-1)==-1);
        a->action=old;sFusionTestClash=1;
    }
    if(sFusionTestPlayable&&sFusionTestTick==sFusionTestAt+50){extern void Port_TagUiCapture(s32);Port_TagUiCapture(140+sFusionTestCase);}
}
void TagFusion_TestAfterFrame(void){
    if(!fusion_test_on()||sFusionTestCase!=12||!sFusionTestPlayable||sFusionTestTick!=sFusionTestAt+30)return;
    extern s32 Battle_Restart(void);
    Battle_Restart();
    fusion_check("rematch_clears_fusion_and_pad_ownership",!sFusion[0].state&&!sFusion[1].state&&BtlChar_Get(0)->pad==0&&BtlChar_Get(1)->pad==1);
    fusion_check("rematch_restores_both_fighters",BtlMember_GetActive(BtlChar_Get(0))->chara==3&&BtlMember_GetActive(BtlChar_Get(1))->chara==31&&BtlMember_GetActive(BtlChar_Get(1))->gauge.health>0);
    fprintf(stderr,"tag fusion result: %s\n",sFusionTestFailed?"FAIL":"PASS");exit(sFusionTestFailed?2:0);
}
