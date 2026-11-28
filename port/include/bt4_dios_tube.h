#ifndef BT4_DIOS_TUBE_H
#define BT4_DIOS_TUBE_H

enum { BT4_DIOS_LIMIT = 10 };
enum { BT4_DIOS_MAX_FRAMES = 9000, BT4_DIOS_INPUT_BYTES = 12 * BT4_DIOS_MAX_FRAMES };
enum { BT4_DIOS_RANDOM_WORDS = 627 };
typedef struct Bt4DiosHud {
    char names[2][97];
    int ping[2];
} Bt4DiosHud;
enum { BT4_DIOS_GIF_W = 160, BT4_DIOS_GIF_H = 90, BT4_DIOS_GIF_FPS = 30,
       BT4_DIOS_GIF_FRAMES = 10*BT4_DIOS_GIF_FPS, BT4_DIOS_GIF_WIRE = 3400000 };
typedef struct Bt4DiosReplay {
    int stage, fighters[2][2], frames, trackFrames[2], time, hasRandom;
    unsigned random[BT4_DIOS_RANDOM_WORDS];
    Bt4DiosHud hud;
    unsigned char inputs[BT4_DIOS_INPUT_BYTES];
} Bt4DiosReplay;
typedef struct Bt4DiosEntry {
    char id[64];
    char players[2][97];
    int likes, views, liked, hasPreview;
} Bt4DiosEntry;
typedef struct Bt4DiosCatalog {
    int state, count;
    Bt4DiosEntry entries[BT4_DIOS_LIMIT];
} Bt4DiosCatalog;

#ifdef __cplusplus
extern "C" {
#endif
void Port_DiosTubeCatalog(Bt4DiosCatalog *result, int refresh);
void Port_DiosTubeLike(const char *id, int liked);
void Port_DiosTubeFetch(const char *id);
int Port_DiosTubeTake(Bt4DiosReplay *result);
void Port_DiosTubeCancel(void);
void Port_DiosTubeViewed(void);
void Port_DiosViewerBegin(const Bt4DiosHud *hud, int exportVideo);
void Port_DiosViewerStartVideo(void);
int Port_DiosViewerReadHud(Bt4DiosHud *hud);
int Port_DiosViewerActive(void);
void Port_DiosViewerEnd(void);
void Port_DiosViewerToggleCamera(void);
unsigned Port_DiosCaptureEpoch(void);
void Port_DiosCaptureFrame(unsigned epoch,const unsigned char *rgba);
int Port_DiosPreviewSlot(unsigned slot,const char *id);
int Port_DiosPreviewPixels(unsigned slot,unsigned *generation,unsigned *frame,unsigned *pixels);
void BattleReplay_RankedHit(int combo);
void BattleReplay_RankedCounter(void);
void BattleReplay_RankedStats(int stats[3]);
void BattleReplay_DiosRandomAtLoad(void);
const unsigned *BattleReplay_DiosRandom(void);
#ifdef __cplusplus
}
#endif
#endif
