#ifndef BTL_EMOTES_H
#define BTL_EMOTES_H

enum { BTL_EMOTE_CELEBRATE_ME = 28, BTL_EMOTE_COUNT = BTL_EMOTE_CELEBRATE_ME,
       BTL_EMOTE_MIXAMO_FIRST = 23, BTL_EMOTE_PAD_BYTE = 16 };
/* Older peers see the existing emote and consume the same taunt input. */
enum { BTL_EMOTE_LEGACY_FALLBACK = 27, BTL_EMOTE_PAD_FLAGS = 17,
       BTL_EMOTE_PAD_VARIANT = 0x08, BTL_EMOTE_TAG_VARIANT = 0x20 };
static inline int BtlEmote_Decode(const unsigned char *pad, int tag) {
    const int emote = pad[BTL_EMOTE_PAD_BYTE];
    const int variant = tag ? BTL_EMOTE_TAG_VARIANT : BTL_EMOTE_PAD_VARIANT;
    return emote == BTL_EMOTE_LEGACY_FALLBACK && (pad[BTL_EMOTE_PAD_FLAGS] & variant)
        ? BTL_EMOTE_CELEBRATE_ME : emote;
}
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
int BtlEmote_AudioActive(void);
int BtlEmote_WheelSeats(void);
#ifdef __cplusplus
}
#endif
#endif
