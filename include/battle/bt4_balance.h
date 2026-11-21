#ifndef BATTLE_BT4_BALANCE_H
#define BATTLE_BT4_BALANCE_H

/* BT4 roster IDs for the stock fighters adjusted by the balance patch. */
enum {
    BT4_GOKU_GT = 7,
    BT4_GOKU_GT_SSJ3 = 9,
    BT4_GOHAN_SSJ2 = 19,
    BT4_GOTENKS = 48,
    BT4_VIDEL = 56,
    BT4_SUPREME_KAI = 58,
    BT4_YAJIROBE = 60,
    BT4_BUU_PURE_EVIL = 192,
    BT4_HIRUDEGARN = 195,
    BT4_GAMMA_2 = 207,
    BT4_CELL_PERFECT = 224,
    BT4_TRUNKS_SUPER = 225
};

enum {
    BT4_GOKU_END = 3,
    BT4_GOKU_MASTERED_INSTINCT = 64,
    BT4_ZAMASU_FUSED = 76,
    BT4_JIREN = 151,
    BT4_GOKU_INSTINCT_SIGN = 220,
    BT4_ZAMASU_NORMAL_OUTFIT = 0,
    BT4_ZAMASU_CORRUPTED_OUTFIT = 1,
    BT4_ZAMASU_GIANT_OUTFIT = 2,
    BT4_JIREN_FULL_POWER_OUTFIT = 2,
    BT4_CLASH_CORRUPTION_PENDING = 1
};

#ifdef BT4_TAG_TEAM_EXPANDED
static inline s32 Bt4_ExtraFormsAllowed(void) {
    extern int Port_RankedActive(void);
    return !Port_RankedActive();
}
#endif

static inline s32 Bt4Balance_IsBroly(s32 fighter) {
    return (fighter >= 131 && fighter <= 133) ||
           (fighter >= 188 && fighter <= 190) || fighter == 209;
}

#endif
