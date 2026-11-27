#ifndef PORT_BT4_GOKU_COSTUMES_H
#define PORT_BT4_GOKU_COSTUMES_H

/* A range outside the disc and the port's added stage and song files. */
#define BT4_GOKU_COSTUME_PART_INDEX 131072
#define BT4_GOKU_COSTUME_FILE_FIRST (0x20000 + BT4_GOKU_COSTUME_PART_INDEX)
#define BT4_GOKU_COSTUME_SUPER_FIRST 56
#define BT4_GOKU_COSTUME_FILE_COUNT 92
#define BT4_GOKU_OUTFIT_COUNT 6
#define BT4_GOKU_COSTUME_PATH "tbo/goku-costumes/"

int Port_GokuCostumesReady(void);
void Port_GokuCostumePath(const char *relative, char *path, unsigned capacity);

static inline int Bt4_GokuSharedOutfits(int chara) {
    return (chara >= 3 && chara <= 6) || chara == 64 || chara == 220;
}

static inline int Bt4_GokuOutfitLabelChara(int chara) {
#ifdef BT4_TAG_TEAM_EXPANDED
    extern int Port_RankedActive(void);
    if ((chara == 64 || chara == 220) && !Port_RankedActive() && Port_GokuCostumesReady()) return 3;
#endif
    return chara;
}

#endif
