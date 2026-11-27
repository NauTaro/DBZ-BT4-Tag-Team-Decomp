#include "common.h"
#include "battle/emotes.h"
#include "battle/battle.h"
#include "battle/battle_setup.h"
#include "battle/btl_input.h"
#include "battle/btl_obj.h"
#include "battle/btl_seq.h"

#ifdef PORT
/* This state follows the synchronized controller samples and is restored by rollback. */
typedef struct BtlEmoteState {
    unsigned previous;
    unsigned steps;
    int selected;
} BtlEmoteState;

static BtlEmoteState sEmotes[BTL_OBJ_MAX];

void BtlEmote_Reset(void) {
    for (unsigned i = 0; i < sizeof(sEmotes) / sizeof(sEmotes[0]); ++i) {
        sEmotes[i] = (BtlEmoteState){0};
    }
}

int BtlEmote_Input(int object, unsigned buttons, int idle, int selected) {
    const unsigned chord = BTLB_LOCKON | BTLB_GUARD;
    const unsigned cancel = BTLB_DIR_MASK | BTLB_DASH | BTLB_ASCEND | BTLB_DESCEND |
                            BTLB_RUSH | BTLB_BLAST | BTLB_CHARGE;
    const int available = idle && !(buttons & cancel);
    const int claimed = available && selected >= 1 && selected <= BTL_EMOTE_COUNT && (buttons & chord) == chord;
    BtlEmoteState *state;

    if (object < 0 || object >= BTL_OBJ_MAX) return 0;
    state = &sEmotes[object];
    if (!available) {
        state->selected = 0;
    } else if (claimed && (state->previous & chord) != chord) {
        state->selected = selected;
        state->steps = 0;
    } else if (state->selected) {
        ++state->steps;
    }
    state->previous = buttons;
    return claimed;
}

int BtlEmote_Pose(int object, float *frame) {
    extern int Port_SpeedMilli(void);
    BtlEmoteState *state;
    if (object < 0 || (unsigned)object >= sizeof(sEmotes) / sizeof(sEmotes[0])) return 0;
    state = &sEmotes[object];
    if (!state->selected) return 0;
    float framesPerTick = 2.0f;
    if (state->selected >= BTL_EMOTE_MIXAMO_FIRST) {
        /* Celebrate Me's loop uses a 60 Hz timeline and battle input ticks at 30 Hz. */
        const float tempo = state->selected == BTL_EMOTE_CELEBRATE_ME ? 2000.0f : 1000.0f;
        framesPerTick = tempo / (float)Port_SpeedMilli();
    }
    *frame = state->steps * framesPerTick;
    return state->selected;
}

int BtlEmote_AudioActive(void) {
    return BtlSeq_IsFighting() && !(Battle_GetWork()->flags & (BATTLE_FLAG_PAUSE | BATTLE_FLAG_LOADING));
}

int BtlEmote_WheelSeats(void) {
    extern s32 Port_NetActive(void), Port_NetMe(void), Port_NetSpectating(void);
    extern s32 TagBattle_Active(void), TagBattle_Network(void), TagBattle_LocalActor(void);
    extern s32 TagBattle_Count(void), TagBattle_Spectating(void);
    extern BtlInputChr *BtlChar_Get(s32);
    extern s32 BtlAct_GetCurrent(BtlInputChr *);
    const int tag = TagBattle_Active();
    const int online = tag ? TagBattle_Network() : Port_NetActive();
    int first = 0, last = tag ? TagBattle_Count() : 2;
    if (online) {
        if (tag ? TagBattle_Spectating() : Port_NetSpectating()) return 0;
        first = tag ? TagBattle_LocalActor() : Port_NetMe();
        if (first < 0 || first >= last) return 0;
        last = first + 1;
    }
    int mask = 0;
    for (int side = first; side < last; ++side) {
        int pad = online ? 0 : BattleSide_GetPad(side);
        if (pad < 0 || pad > 1 || BattleSide_GetControl(side) != BTL_CONTROL_PAD) continue;
        BtlInputChr *fighter = BtlChar_Get(side);
        if (fighter && BtlAct_GetCurrent(fighter) == 0x0B) mask |= 1 << pad;
    }
    return mask | (!online && Battle_IsSplitScreen() ? 4 : 0);
}
#endif
