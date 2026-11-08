#ifndef BT4_MATCH_SUPABASE_H
#define BT4_MATCH_SUPABASE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct Bt4DiscoveryStatus {
    int state,role,lobby,peerCount,watchCount,watchGen;
    char code[8],error[256],peerName[20],peerAddr[4][64],watchAddr[32][64];
} Bt4DiscoveryStatus;
int Bt4Discovery_Begin(unsigned generation,int role,int protocol,uint64_t content,const char *name,const char *code,const char *addresses_json,const char *queue);
int Bt4Discovery_Poll(unsigned generation,Bt4DiscoveryStatus *out);
void Bt4Discovery_End(unsigned generation);
int Bt4Discovery_Counts(int protocol,uint64_t content,int counts[5]);
int Port_OnlinePresence(int active); /* -1 until an authenticated aggregate is received */
int Bt4Discovery_TagCounts(uint64_t content,int counts[3]);
typedef struct Bt4RankedEntry {char name[64];int rating,wins,losses;} Bt4RankedEntry;
typedef struct Bt4RankedView {
    int state,rating,wins,losses,count,page,pages; /* 0 loading, 1 prepared, -1 unavailable */
    char season[40],updated[40];
    Bt4RankedEntry top[5];
} Bt4RankedView;
void Port_RankedRefresh(int nativeTeams);
void Port_RankedView(Bt4RankedView *out);
void Port_RankedPage(int direction);
void Port_RankedSearch(int selected);
int Port_RankedActive(void);
void Port_RankedObserve(int battleState,int outcomeBits,unsigned tick,unsigned confirmed,int resimulating);
int Port_RankedResult(int *rating,int *delta);
#ifdef __cplusplus
}
#endif
#endif
