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
typedef struct Bt4TurnCredentials {
    char server[128], username[160], password[160];
} Bt4TurnCredentials;
int Bt4Discovery_Begin(unsigned generation,int role,int protocol,uint64_t content,const char *name,const char *code,const char *addresses_json,const char *queue);
int Bt4Discovery_Poll(unsigned generation,Bt4DiscoveryStatus *out);
void Bt4Discovery_End(unsigned generation);
int Bt4Discovery_RelayAsk(unsigned generation);
int Bt4Discovery_RelayPoll(unsigned generation, Bt4TurnCredentials *out);
int Bt4Discovery_PublishRelay(unsigned generation, const char *address);
int Bt4Discovery_Counts(int protocol,uint64_t content,int counts[5]);
int Port_OnlinePresence(int active); /* -1 until an authenticated aggregate is received */
int Bt4Discovery_TagCounts(uint64_t content,int counts[3]);
#ifdef __cplusplus
}
#endif
#endif
