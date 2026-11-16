/* Hidden two-client fixture. Runs only on delivered, shared simulation frames. */
static void TagRules_NetworkTest(void) {
    const char *test=getenv("BT4_RENDER_TEST")?getenv("BT4_TAG_NET_RULE_TEST"):NULL;
    if(!test||!TagBattle_Network()||getenv("BT4_TAG_COOP_TEST"))return;
    static s32 passed=1,target=-1,initialStage=-1,initialAsset=-1;
    extern s32 Battle_GetAnnouncer(void),Battle_GetRuleUnk10(void),Battle_GetStage(void);
    extern s32 BtlStage_GetChangeTarget(void),BtlStage_GetId(void),BtlStage_IsReady(void);
    extern s32 EftHit_TestStageDestruction(s32),BtlAct_CanTransform(void*,u32,s32,s32);
    extern unsigned Port_TagBattleRules(void);
    const unsigned rules=(unsigned)atoi(test);
    if(sTagOnlineFrame==600){
        target=BtlStage_GetChangeTarget();initialStage=Battle_GetStage();initialAsset=BtlStage_GetId();
        const s32 referee=(rules&7)?(rules&7)-1:4,destruction=(rules&8)!=0;
        passed&=Port_TagBattleRules()==rules&&Battle_GetAnnouncer()==referee&&Battle_GetRuleUnk10()==destruction;
        fprintf(stderr,"tag net rules setup: seat=%d rules=%u referee=%d destruction=%d stage=%d target=%d\n",
                Port_TagSeat(),Port_TagBattleRules(),Battle_GetAnnouncer(),Battle_GetRuleUnk10(),initialStage,target);
        for(s32 i=0;i<TagBattle_Count();++i){
            void *chr=BtlChar_Get(i);s32 *g=BtlMember_GetActiveGauge(chr),old=g[3];g[3]=g[4];
            s32 has=0,can=0;
            for(s32 slot=0;slot<4;++slot){has|=BtlAct_CanTransform(chr,slot,0,1);can|=BtlAct_CanTransform(chr,slot,1,1);}
            g[3]=old;
            const s32 allowed=BattleSide_GetControl(i)!=BTL_CONTROL_CPU||!(rules&(16u<<TagBattle_Team(i)));
            passed&=can==(has&&allowed);
            fprintf(stderr,"tag net rules actor: slot=%d team=%d control=%d form=%d can=%d expected=%d\n",
                    i,TagBattle_Team(i),BattleSide_GetControl(i),has,can,has&&allowed);
        }
    }
    if(sTagOnlineFrame==650){
        const s32 enemy=TagBattle_Target(0);TagBattle_SetTarget(0,enemy);
        s32 *hp=BtlMember_GetActiveGauge(BtlChar_Get(enemy));hp[0]=hp[1]*2/5;
        const s32 accepted=EftHit_TestStageDestruction(BattleSide_GetObjId(0));
        passed&=target>=0&&accepted==((rules&8)!=0);
        fprintf(stderr,"tag net rules hit: accepted=%d\n",accepted);
    }
    if(sTagOnlineFrame>=650&&sTagOnlineFrame%30==0)
        fprintf(stderr,"tag net rules stage: frame=%u id=%d asset=%d ready=%d\n",sTagOnlineFrame,Battle_GetStage(),BtlStage_GetId(),BtlStage_IsReady());
    if(sTagOnlineFrame==1500){
        passed&=BtlStage_IsReady()&&((rules&8)?Battle_GetStage()==target&&BtlStage_GetId()!=initialAsset:Battle_GetStage()==initialStage);
        fprintf(stderr,"tag net rules result: %s seat=%d\n",passed?"PASS":"FAIL",Port_TagSeat());fflush(stderr);
        exit(passed?0:3);
    }
}

/* Hidden integration fixture: exercise match setup, the transformation gate and native stage loading. */
static void TagRules_Test(void) {
    TagRules_NetworkTest();
    static s32 tick, target, passed = 1;
    const char *test = getenv("BT4_RENDER_TEST") ? getenv("BT4_TAG_BATTLE_RULE_TEST") : NULL;
    if (!test || !TagBattle_Local() || !BtlSeq_IsFighting()) return;

    extern s32 Battle_GetAnnouncer(void), Battle_GetRuleUnk10(void), Battle_GetStage(void);
    extern s32 BtlStage_GetChangeTarget(void), BtlStage_GetId(void), BtlStage_IsReady(void);
    extern s32 BtlChar_GetCount(void), BtlAct_CanTransform(void *, u32, s32, s32);
    extern s32 EftHit_TestStageDestruction(s32 objId);
    const s32 destruction = *test == '1';
    if (++tick == 5) {
        const s32 humans = atoi(test + 2);
        target = BtlStage_GetChangeTarget();
        passed = Battle_GetAnnouncer() == 5 && Battle_GetRuleUnk10() == destruction && target >= 0;
        fprintf(stderr, "tag rules setup: referee=%d destruction=%d stage=%d target=%d actors=%d\n",
                Battle_GetAnnouncer(), Battle_GetRuleUnk10(), Battle_GetStage(), target, BtlChar_GetCount());
        s32 eligible[2] = {0, 0};
        for (s32 i = 0; i < TagBattle_Count(); ++i) {
            void *chr = BtlChar_Get(i);
            s32 *gauges = BtlMember_GetActiveGauge(chr);
            const s32 oldKi = gauges[3];
            gauges[3] = gauges[4];
            s32 hasForm = 0, canTransform = 0;
            for (s32 slot = 0; slot < 4; ++slot) {
                hasForm |= BtlAct_CanTransform(chr, slot, 0, 1);
                canTransform |= BtlAct_CanTransform(chr, slot, 1, 1);
            }
            gauges[3] = oldKi;
            const s32 expected = i < humans || TagBattle_Team(i) == 1;
            fprintf(stderr, "tag rules actor: slot=%d team=%d form=%d voluntary=%d expected=%d\n",
                    i, TagBattle_Team(i), hasForm, canTransform, expected);
            eligible[expected] += hasForm != 0;
            passed &= canTransform == (hasForm && expected);
        }
        passed &= eligible[0] > 0 && eligible[1] > 0;
    }
    if (tick == 25) {
        if (target >= 0) {
            TagBattle_SetTarget(0, 1);
            s32 *hp = BtlMember_GetActiveGauge(BtlChar_Get(1));
            hp[0] = hp[1] * 2 / 5;
            const s32 accepted = EftHit_TestStageDestruction(BattleSide_GetObjId(0));
            fprintf(stderr, "tag rules: qualifying hit accepted=%d\n", accepted);
            passed &= accepted == destruction;
        }
    }
    if (tick == 430) {
        passed &= BtlStage_IsReady() && BtlChar_GetCount() == TagBattle_Count() &&
            (destruction ? Battle_GetStage() == target && BtlStage_GetId() != Battle_GetStartStage()
                         : Battle_GetStage() != target);
        fprintf(stderr, "tag rules result: %s selected-stage=%d stage-asset=%d actors=%d\n",
                passed ? "PASS" : "FAIL", Battle_GetStage(), BtlStage_GetId(), BtlChar_GetCount());
        exit(passed ? 0 : 3);
    }
}
