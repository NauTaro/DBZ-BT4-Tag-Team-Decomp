/* Hidden loopback regression. Scripted input travels over the actual room
 * transport; setup is applied on identical delivered simulation frames. */
static s32 sCoopAuditClash,sCoopAuditFailed,sCoopAuditPlaced,sCoopAuditJoint,sCoopAuditFusion,sCoopAuditRevived,sCoopAuditIndex=-1;
static void TagCoop_Check(const char *name,s32 ok){
    fprintf(stderr,"tag online coop check: %s=%s seat=%d\n",name,ok?"PASS":"FAIL",Port_TagSeat());
    if(!ok)sCoopAuditFailed=1;
}
static void TagCoop_Place(s32 a,s32 allied){
    f32 p[4],r[4];BtlStage_GetStartPlace(allied?0:1,(Vec4*)p,(Vec4*)r,0);
    if(a==2)p[0]+=24;else if(a>2)p[0]+=300;
    BtlChar_Place((BtlCtlChr*)BtlChar_Get(a),(Vec4*)p,(Vec4*)r,BtlStage_FindZoneNear(-1,(Vec4*)p));
}
static void TagCoop_NetworkSetup(void){
    const char *test=getenv("BT4_RENDER_TEST")?getenv("BT4_TAG_COOP_TEST"):NULL;
    if(!test||!TagBattle_Network()||!BtlSeq_IsFighting())return;
    s32 mode=atoi(test);if(mode==4)mode=2;
    for(s32 i=0;i<TagBattle_Count();++i)if(Port_TagCPU(i))BtlCharApi_SetInjectedInput(BattleSide_GetObjId(i),0,0,0);
    if(!sCoopAuditPlaced&&sTagOnlineFrame>=500){
        sCoopAuditPlaced=1;
        TagCoop_Check("human_allies_in_independent_seats",!Port_TagCPU(0)&&!Port_TagCPU(2)&&TagBattle_Allied(0,2));
        const TagHealthRule *health=TagBattle_HealthRule(0);
        TagCoop_Check("host_health_reaches_guest",health&&health->maximum==80000&&health->starting==40000&&BtlCharApi_GetHpMax(0)==80000);
        TagCoop_Check("host_gameplay_options",Port_TagOption("tag_off_senzu",0)==3&&Port_TagOption("tag_revive_team1",0)==1);
        if(mode==5){
            extern s32 TagBattle_Boss(s32),TagBattle_CombatDamage(s32,s32,s32);
            extern int Port_TagRulesV2(void);
            TagCoop_Check("host_boss_rules_reach_guest",Port_TagRulesV2()&&TagBattle_Boss(0)&&!TagBattle_Boss(2));
            TagCoop_Check("independent_damage_and_resistance",TagBattle_CombatDamage(0,1,200)==300&&
                TagBattle_CombatDamage(1,0,200)==100&&TagBattle_CombatDamage(2,1,200)==200);
        }
        for(s32 i=0;i<TagBattle_Count();++i){
            s32 *g=BtlMember_GetActiveGauge(BtlChar_Get(i));g[0]=g[1]=200000;g[3]=g[4];g[5]=g[6]=500000;
            BtlAct_ClearQueue(BtlChar_Get(i));BtlAct_Request(BtlChar_Get(i),0xB);BtlChar_SetFlag(BtlChar_Get(i),5);
            TagCoop_Place(i,TagBattle_Allied(0,i));TagBattle_SetTarget(i,TagBattle_Allied(0,i)?1:0);
        }
        if(mode==3){s32 *g=BtlMember_GetActiveGauge(BtlChar_Get(0));g[0]=0;BtlAct_Request(BtlChar_Get(0),0xD8);}
    }
    if(mode==2&&sTagOnlineFrame==600){
        extern s32 BtlParam_GetUnkBA(void*,s32,s32),BtlParam_GetUnkB4(void*,s32),BtlParam_GetUnkB1(void*,s32);
        for(s32 i=0;i<3;++i){
            s32 compatible=0;for(s32 j=0;j<4;++j)if(BtlParam_GetUnkBA(BtlChar_Get(0),i,j)==Port_TagPlayerChara(2))compatible=1;
            if(compatible&&BtlParam_GetUnkB1(BtlChar_Get(0),i)==1)sCoopAuditIndex=i;
        }
        if(sCoopAuditIndex>=0)TagFusion_Request(0,sCoopAuditIndex);
        TagCoop_Check("native_fusion_request",sCoopAuditIndex>=0&&TagFusion_Busy(0)&&TagFusion_Busy(2));
    }
}
static void TagCoop_NetworkAfter(void){
    const char *test=getenv("BT4_RENDER_TEST")?getenv("BT4_TAG_COOP_TEST"):NULL;
    if(!test||!TagBattle_Network())return;s32 mode=atoi(test);if(mode==4)mode=2;
    if(sTagAssist[0].active&&sTagAssist[2].active)sCoopAuditJoint=1;
    s32 otherActor,fusionTicks,fusionTotal;
    if(TagFusion_Hud(0,&otherActor,&fusionTicks,&fusionTotal)==3&&!sCoopAuditFusion){s32 c=TagFusion_Controller(0);sCoopAuditFusion=1;
        TagCoop_Check("fused_controller_input_mapping",TagOnline_InputActor(0)==c&&TagOnline_InputActor(2)==(c==0?2:0));}
    if(mode==2&&sTagOnlineFrame>980&&sTagOnlineFrame<1080&&sCoopAuditFusion&&TagFusion_Busy(0)&&!TagFusion_Separating(0)){
        s32 controller=TagFusion_Controller(0),other=controller==0?2:0;
        if(TagBattle_SourcePressed(other)&BTLB_CIRCLE){
            s32 *current=(s32*)((u8*)BtlChar_Get(0)+0x948),action=*current;
            *current=0x130;TagCoop_Check("spectator_shared_beam_input",TagFusion_ClashExtra(0,0x50));
            *current=0xFA;TagCoop_Check("spectator_shared_melee_input",TagFusion_ClashExtra(0,0x4F));
            *current=0xFC;TagCoop_Check("spectator_shared_aerial_input",TagFusion_ClashChoice(0,0,-1)==1);
            *current=action;sCoopAuditClash=1;
        }
    }
    if(mode==3&&sCoopAuditPlaced&&BtlCharApi_GetHp(0)>0)sCoopAuditRevived=1;
    if(mode==5){
        extern s32 TagBattle_Focus(s32);
        if(sTagOnlineFrame==610)TagCoop_Check("delivered_input_focuses_ally",TagBattle_Focus(0)==2&&TagBattle_Target(0)==1);
        if(sTagOnlineFrame==630)TagCoop_Check("delivered_input_unlocks_camera",TagBattle_Focus(0)==-2&&TagBattle_Target(0)==1);
        if(sTagOnlineFrame==645)TagCoop_Check("free_camera_ignores_target_switch",TagBattle_Focus(0)==-2&&TagBattle_Target(0)==1);
        if(sTagOnlineFrame==665)TagCoop_Check("select_restores_enemy_camera",TagBattle_Focus(0)==-1);
        if(sTagOnlineFrame==685)TagCoop_Check("dpad_selection_disables_l3",TagBattle_Target(0)==1);
        if(sTagOnlineFrame==695)TagCoop_Check("right_stick_selection_disables_l3",TagBattle_Target(0)==1);
        if(sTagOnlineFrame==700){
            fprintf(stderr,"tag online coop result: %s seat=%d\n",sCoopAuditFailed?"FAIL":"PASS",Port_TagSeat());
            fflush(stderr);exit(sCoopAuditFailed?3:0);
        }
    }
    if(sTagOnlineFrame==1450){
        TagCoop_Check("native_joint_launch",mode!=1||sCoopAuditJoint);
        TagCoop_Check("native_fusion_and_separation",mode!=2||(sCoopAuditClash&&sCoopAuditFusion&&!TagFusion_Busy(0)&&!TagFusion_Busy(2)));
        TagCoop_Check("human_revives_human",mode!=3||sCoopAuditRevived);
        fprintf(stderr,"tag online coop result: %s seat=%d\n",sCoopAuditFailed?"FAIL":"PASS",Port_TagSeat());fflush(stderr);exit(sCoopAuditFailed?3:0);
    }
}
static void TagCoop_NetworkInput(void){
    if(!getenv("BT4_RENDER_TEST")||!getenv("BT4_TAG_COOP_TEST")||!TagBattle_Network())return;
    for(s32 i=0;i<TagBattle_Count();++i)if(Port_TagCPU(i))BtlCharApi_SetInjectedInput(BattleSide_GetObjId(i),0,0,0);
}
