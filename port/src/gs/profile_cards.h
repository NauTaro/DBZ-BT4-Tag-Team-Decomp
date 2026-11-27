#ifndef BT4_PROFILE_CARDS_H
#define BT4_PROFILE_CARDS_H
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    int nameColor, background, decoration, premium, checkout;
    char avatar[37], motion[37], banner[37], bannerMotion[37];
    int hasRankedRecord, rankedWins, rankedLosses;
    unsigned nameRgb;
    int hasNameRgb;
    unsigned bannerRgb;
    int hasBannerRgb;
} Bt4ProfileCard;
#define BT4_PROFILE_AVATAR_MAX 320
#define BT4_PROFILE_ANIMATION_FRAMES 120
#define BT4_PROFILE_ANIMATION_COLUMNS 8
#define BT4_PROFILE_PIXEL_CAPACITY (BT4_PROFILE_ANIMATION_FRAMES * 192 * 192 * 4)
#define BT4_PROFILE_MEDIA_WIRE_CAP 1200000
void Port_ProfileCardForPlayer(const char *name, Bt4ProfileCard *out);
void Port_ProfileCardForTagPlayer(const char *name, Bt4ProfileCard *out);
int Port_ProfileCardView(Bt4ProfileCard *out);
enum Bt4ProfileCardStatus { BT4_CARD_FAILED=-1, BT4_CARD_EMPTY, BT4_CARD_LOADING, BT4_CARD_READY, BT4_CARD_SAVING, BT4_CARD_SAVE_FAILED };
enum Bt4ProfileImportStatus { BT4_PROFILE_IMPORT_INVALID=-1, BT4_PROFILE_IMPORT_DONORS=-2, BT4_PROFILE_IMPORT_SIZE=-3 };
int Port_ProfileCardError(char *out, int capacity);
void Port_ProfileCardRequest(const Bt4ProfileCard *card);
int Port_ProfileMediaSave(const char *kind, const char *path);
int Port_ProfileMediaRead(const char *id, unsigned char **pixels, int *width, int *height, int *count, int *columns, int delays[BT4_PROFILE_ANIMATION_FRAMES]);
int Port_ProfileDraftFile(const char *kind,const char *path);
int Port_ProfileDraftRoster(int character);
int Port_ProfileDraftPreview(float x,float y,float zoom,unsigned char *rgba,int *width,int *height,int *count,int *columns,int delays[BT4_PROFILE_ANIMATION_FRAMES]);
int Port_ProfileDraftUpload(float x,float y,float zoom);
void Port_ProfileDraftReset(void);
int Port_ProfilePortraitPath(int chara,char *out,int capacity);
int Port_ProfileRosterPortrait(int chara,unsigned char *rgba);
int Port_ProfileRosterName(int chara,char *name,int capacity);
int Port_ProfileCheckout(int action, char *url, unsigned capacity);
void Port_UiEncounter(const char *localName, const char *rivalName, const char *region, int latency);
int Port_ProfileRankedRating(const char *name);
void Port_UiRankedSettlement(int rating, int delta);
#ifdef __cplusplus
}
#endif
#endif
