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
    BT4_CELL_PERFECT = 107,
    BT4_BUU_PURE_EVIL = 192,
    BT4_HIRUDEGARN = 195,
    BT4_GAMMA_2 = 207,
    BT4_TRUNKS_SUPER = 225
};

static inline s32 Bt4Balance_IsBroly(s32 fighter) {
    return (fighter >= 131 && fighter <= 133) ||
           (fighter >= 188 && fighter <= 190) || fighter == 209;
}

#endif
