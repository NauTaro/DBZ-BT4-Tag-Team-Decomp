#ifndef BTL_EMOTES_H
#define BTL_EMOTES_H

enum { BTL_EMOTE_COUNT = 27, BTL_EMOTE_MIXAMO_FIRST = 23, BTL_EMOTE_PAD_BYTE = 16 };
enum { BTL_EMOTE_SLOTS = 4 };
static const char *const kBattleEmoteSettings[BTL_EMOTE_SLOTS] = {
    "battle_emote", "battle_emote_right", "battle_emote_down", "battle_emote_left"
};

#ifdef __cplusplus
extern "C" {
#endif
void BtlEmote_Reset(void);
int BtlEmote_Input(int object, unsigned buttons, int idle, int selected);
int BtlEmote_Pose(int object, float *frame);
#ifdef __cplusplus
}
#endif
#endif
