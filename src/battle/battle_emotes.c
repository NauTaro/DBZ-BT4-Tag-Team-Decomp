#include "common.h"
#include "battle/emotes.h"
#include "battle/btl_input.h"
#include "battle/btl_obj.h"

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
    const int available = idle && selected >= 1 && selected <= BTL_EMOTE_COUNT && !(buttons & cancel);
    const int claimed = available && (buttons & chord) == chord;
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
    *frame = state->steps * (state->selected >= BTL_EMOTE_MIXAMO_FIRST ?
                            1000.0f / (float)Port_SpeedMilli() : 2.0f);
    return state->selected;
}
#endif
