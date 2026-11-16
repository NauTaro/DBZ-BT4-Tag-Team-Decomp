/* Runs through the same split-screen pad and camera path as a local match. */
static s32 sSpectatorTick, sSpectatorFailed, sSpectatorSecondAlly, sSpectatorRevive;
static Vec4 sSpectatorPreviousEye;
extern void BtlCam_TestSplitSpectatorEye(s32 player, Vec4 *eye);
extern void DemoCam_SetFixedPose(Vec4 *pos, Vec4 *rot), DemoCam_ClearFixed(void);

static void TagSpectator_Check(const char *name, s32 pass) {
    fprintf(stderr, "tag spectator check: %s=%s\n", name, pass ? "PASS" : "FAIL");
    if (!pass) sSpectatorFailed = 1;
}

static void TagSpectator_TestPads(void) {
    if (!getenv("BT4_RENDER_TEST") || !getenv("BT4_TAG_SPLIT_SPECTATOR_TEST") || !BtlSeq_IsFighting()) return;
    ++sSpectatorTick;
    sSpectatorRevive = atoi(getenv("BT4_TAG_SPLIT_SPECTATOR_TEST")) == 1;
    for (s32 player = 0; player < PAD_COUNT; ++player) {
        gPad[player].gamePressed = 0;
        gPad[player].right[0] = gPad[player].right[1] = 0;
    }
    if (sSpectatorTick == 10) {
        TagSpectator_Check("local_two_player", Battle_IsSplitScreen() && !TagBattle_Network() &&
            BattleSide_GetControl(0) == BTL_CONTROL_PAD && BattleSide_GetControl(1) == BTL_CONTROL_PAD);
        TagSpectator_Check("requested_roster", TagBattle_Count() == (sSpectatorRevive ? 4 : 10) &&
            TagBattle_Allied(0, 1) == sSpectatorRevive);
        for (s32 actor = 0; actor < TagBattle_Count(); ++actor) {
            s32 *g = BtlMember_GetActiveGauge(BtlChar_Get(actor));
            g[0] = g[1] = 40000;
            BtlAct_Request(BtlChar_Get(actor), 0xB);
            TagJoint_Place(actor, sSpectatorRevive ? (actor == 1 ? 15.f : 0.f) : 36.f * actor, 0);
            if (actor > 1 && TagBattle_Allied(actor, 1) && !sSpectatorSecondAlly)
                sSpectatorSecondAlly = actor;
        }
        BtlMember_GetActiveGauge(BtlChar_Get(0))[0] = 0;
        BtlAct_Request(BtlChar_Get(0), 0xD8);
    }
    if (sSpectatorTick == 28) gPad[0].gamePressed = PADG_R1;
    if (sSpectatorTick == 34) gPad[1].gamePressed = PADG_R1;
    if (sSpectatorTick == 40 && sSpectatorRevive) gPad[0].gamePressed = PADG_L1;
    if (sSpectatorTick >= 44 && sSpectatorTick < 65) gPad[0].right[0] = .85f;
    if (sSpectatorRevive && sSpectatorTick == 74) gPad[0].gamePressed = PADG_R1;
    if (!sSpectatorRevive && sSpectatorTick == 36) {
        const s32 ally = TagBattle_SplitViewActor(0);
        if (ally != 0) {
            BtlMember_GetActiveGauge(BtlChar_Get(ally))[0] = 0;
            BtlAct_Request(BtlChar_Get(ally), 0xD8);
        }
        BtlMember_GetActiveGauge(BtlChar_Get(1))[0] = 0;
        BtlAct_Request(BtlChar_Get(1), 0xD8);
    }
    if (!sSpectatorRevive && sSpectatorTick == 40) gPad[1].gamePressed = PADG_R1;
    if (!sSpectatorRevive && sSpectatorTick == 48) {
        Battle_GetWork()->flags |= BATTLE_FLAG_PAUSE;
        gPad[1].gamePressed = PADG_R1;
    }
    if (!sSpectatorRevive && sSpectatorTick == 52)
        Battle_GetWork()->flags &= ~BATTLE_FLAG_PAUSE;
    if (sSpectatorTick == 70) {
        Vec4 pos = {0, -15, -40, 1}, rot = {0, 0, 0, 0};
        DemoCam_SetFixedPose(&pos, &rot);
    }
    if (sSpectatorTick == 73) DemoCam_ClearFixed();
}

static void TagSpectator_TestInput(void) {
    if (!sSpectatorTick) return;
    for (s32 actor = 2; actor < TagBattle_Count(); ++actor)
        BtlCharApi_SetInjectedInput(BattleSide_GetObjId(actor), 0, 0, 0);
}

static void TagSpectator_TestAfter(s32 splitDraw) {
    s32 left, right;
    if (!sSpectatorTick) return;
    left = TagBattle_SplitViewActor(0);
    right = TagBattle_SplitViewActor(1);
    if (sSpectatorTick == 12)
        TagSpectator_Check("own_body_on_ko", left == 0 && right == 1 && TagBattle_SplitSpectating(0));
    if (sSpectatorTick == 28)
        TagSpectator_Check("cycles_to_living_ally", left != 0 && TagBattle_Allied(0, left) && BtlCharApi_GetHp(left) > 0 && right == 1);
    if (sSpectatorTick == 34)
        TagSpectator_Check("live_second_pad_keeps_view", right == 1 && !TagBattle_SplitSpectating(1));
    if (sSpectatorTick == 36 && !sSpectatorRevive)
        TagSpectator_Check("fallen_viewed_ally_returns_to_own_body", left == 0 && right == 1);
    if (sSpectatorTick == 40) {
        TagSpectator_Check("return_and_independent_second_pad", left == 0 &&
            right == (sSpectatorRevive ? 1 : sSpectatorSecondAlly));
        BtlCam_TestSplitSpectatorEye(0, &sSpectatorPreviousEye);
    }
    if (sSpectatorTick == 48 && !sSpectatorRevive)
        TagSpectator_Check("pause_ignores_switch", left == 0 && right == sSpectatorSecondAlly);
    if (sSpectatorTick == 65) {
        Vec4 eye;
        f32 yaw, pitch;
        BtlCam_TestSplitSpectatorEye(0, &eye);
        TagSpectator_Check("first_pad_rotates_own_render_camera", TagBattle_SplitOrbit(0, &yaw, &pitch) &&
            (fabsf(eye.x - sSpectatorPreviousEye.x) + fabsf(eye.z - sSpectatorPreviousEye.z) > 1.f));
        TagSpectator_Check("second_camera_remains_independent", !TagBattle_SplitOrbit(1, &yaw, &pitch) &&
            right == (sSpectatorRevive ? 1 : sSpectatorSecondAlly));
    }
    if (sSpectatorTick == 70)
        TagSpectator_Check("cinematic_preserves_split_views", splitDraw && left == 0 &&
            right == (sSpectatorRevive ? 1 : sSpectatorSecondAlly));
    if (sSpectatorTick == 65 || sSpectatorTick == 85 || (sSpectatorRevive && sSpectatorTick == 268)) {
        extern void Port_TagUiCapture(s32);
        Port_TagUiCapture(sSpectatorTick == 65 ? 119 : sSpectatorTick == 85 ? 120 : 121);
    }
    if (sSpectatorTick == (sSpectatorRevive ? 300 : 105)) {
        if (sSpectatorRevive) {
            f32 yaw, pitch;
            TagSpectator_Check("native_revival_restores_own_view", sTagRevive[0].completed == 1 &&
                BtlCharApi_GetHp(0) == 10000 && left == 0 && !TagBattle_SplitSpectating(0));
            TagSpectator_Check("native_pad_and_orbit_reset", BattleSide_GetPad(0) == 0 &&
                !TagBattle_SplitOrbit(0, &yaw, &pitch) && yaw == 0 && pitch == 0);
        }
        TagBattle_ResetPairs();
        TagSpectator_Check("reset_clears_spectator_choices", TagBattle_SplitViewActor(0) == 0 &&
            TagBattle_SplitViewActor(1) == 1);
        fprintf(stderr, "tag spectator result: %s\n", sSpectatorFailed ? "FAIL" : "PASS");
        fflush(NULL);
        exit(sSpectatorFailed);
    }
}
