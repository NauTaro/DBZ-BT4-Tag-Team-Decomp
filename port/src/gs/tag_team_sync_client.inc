/* Native-only diagnostics and optional per-actor detail. No game state is changed. */
#include "../../include/tag_team_sync.h"
static int tag_caps_sent,tag_paced,tag_ping=-1,tag_input_sent,tag_sync_count;
static uint32_t tag_input_epoch,tag_input_latest,tag_ping_ack,tag_sync_last_frame;
static Uint64 tag_ping_at;
static uint32_t tag_received_epoch,tag_received_latest;
static int tag_received_valid;
static SDL_AtomicInt tag_catchup;
int Port_TagCatchup(void){return SDL_GetAtomicInt(&tag_catchup);}
static int tag_backlog(void){
    int distance;
    if(!tag_paced||state.phase!=TT_FIGHT||!tag_received_valid||tag_received_epoch!=received.epoch||received.epoch!=state.epoch)return 0;
    distance=(int32_t)(tag_received_latest-received.next);
    return distance>=0?distance+1:0;
}
static void tag_update_catchup(void){
    int backlog=tag_backlog(),old=Port_TagCatchup(),active=old;
    if(backlog>=6)active=1;else if(backlog<=2)active=0;
    SDL_SetAtomicInt(&tag_catchup,active);
    if(active!=old&&getenv("BT4_RENDER_TEST"))
        fprintf(stderr,"tag catchup: active=%d backlog=%d next=%u latest=%u\n",active,backlog,received.next,tag_received_latest);
}
static unsigned tag_sync_words[TT_MAX_PLAYERS][TT_SYNC_WORDS];
static char tag_sync_log[2048];
int Port_TagPing(void){return tag_ping;}
void Port_TagHashActor(int actor,unsigned x,unsigned y,unsigned z,unsigned hp,unsigned ki,unsigned action,unsigned target,unsigned team,unsigned chara,unsigned costume){
    unsigned words[TT_SYNC_WORDS]={x,y,z,hp,ki,action,target,team,chara,costume};
    if(actor==0)tag_sync_count=0;
    if(actor<0||actor>=TT_MAX_PLAYERS)return;
    memcpy(tag_sync_words[actor],words,sizeof(words));tag_sync_count=actor+1;
}
static void tag_sync_notice(const unsigned char *p){
    static const char *fields[]={"position-x","position-y","position-z","hp","ki","action","target","team","character","costume"};
    int field=(int)Ttw_Read(p+28),i;char line[640];FILE *f;const char *base=SDL_GetBasePath();
    snprintf(line,sizeof(line),"tag sync: epoch=%u frame=%u kind=%u streak=%u actor=%d field=%s peer-a=%d value-a=%d peer-b=%d value-b=%d local-last-sent-frame=%u\n",
        Ttw_Read(p+8),Ttw_Read(p+12),Ttw_Read(p+16),Ttw_Read(p+20),(int)Ttw_Read(p+24),
        field>=0&&field<TT_SYNC_WORDS?fields[field]:"legacy-aggregate",(int)Ttw_Read(p+32),(int)Ttw_Read(p+40),
        (int)Ttw_Read(p+36),(int)Ttw_Read(p+44),tag_sync_last_frame);
    fputs(line,stderr);
    if(!tag_sync_log[0]){
        char folder[1800];unsigned pid=0;
#ifdef _WIN32
        pid=(unsigned)GetCurrentProcessId();
#else
        pid=(unsigned)getpid();
#endif
        snprintf(folder,sizeof(folder),"%stbo/logs",base?base:"");SDL_CreateDirectory(folder);
        snprintf(tag_sync_log,sizeof(tag_sync_log),"%s/tag-sync-%lld-%u.txt",folder,(long long)time(NULL),pid);
    }
    f=fopen(tag_sync_log,"a");if(!f)return;fputs(line,f);
    for(i=0;i<tag_sync_count;++i){unsigned *w=tag_sync_words[i];
        fprintf(f,"local-last-sent actor=%d pos16=%d,%d,%d hp=%u ki=%u action=%u target=%u team=%u char=%u costume=%u\n",i,
            (int)w[0],(int)w[1],(int)w[2],w[3],w[4],w[5],w[6],w[7],w[8],w[9]);
    }fclose(f);
}
