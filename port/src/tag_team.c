#include "../include/tag_team.h"
#include <string.h>
#include <stdio.h>
int strcmp(const char *,const char *);

static int capacity(const TtRoom *r) { return r->slot_count; }
static int valid(const TtRoom *r, uint32_t sender, uint32_t epoch, int phase) {
    if (epoch != r->epoch) return TT_STALE;
    if (r->phase != phase) return TT_BAD_PHASE;
    if (Tt_Seat(r, sender) < 0) return TT_BAD_MEMBER;
    return TT_OK;
}
static int owner(const TtRoom *r, uint32_t sender, uint32_t epoch, int phase) {
    int rc = valid(r, sender, epoch, phase);
    if (rc) return rc;
    return sender == r->host ? TT_OK : TT_NOT_OWNER;
}
static void name_copy(char out[20], const char *in) {
    int i = 0;
    memset(out, 0, 20);
    if (in) for (; i < 19 && in[i]; ++i) {
        unsigned char c = (unsigned char)in[i];
        out[i] = c >= 32 && c < 127 ? c : '?';
    }
    if (!i) memcpy(out, "Player", 6);
}
static void clear_choices(TtRoom *r) {
    int i;
    for (i = 0; i < TT_MAX_PLAYERS; ++i) {
        r->players[i].chara = -1;
        r->players[i].costume = 0;
        r->players[i].confirmed = r->players[i].loaded = r->players[i].armed = 0;
        r->players[i].ended=0;
    }
    r->load_digest = 0;
    for(int j=0;j<TT_MAX_PLAYERS;++j)memset(r->players[j].items,0,sizeof(r->players[j].items));
    r->options_hash=Tt_OptionsHash(r);
}

static const char *const option_keys[TT_OPTION_SCALARS]={
    "tag_off_custom","tag_off_senzu","tag_off_joint","tag_off_fusion","tag_fusion_seconds",
    "tag_revive_team1","tag_revive_team2","tag_revive_seconds","tag_revive_charge",
    "tag_revive_health","tag_revive_radius","tag_revive_cpu","tag_revive_opacity"};
static const int option_defaults[TT_OPTION_SCALARS]={1,0,1,1,150,0,0,10,0,25,40,1,30};
static const int option_min[TT_OPTION_SCALARS]={0,0,0,0,10,0,0,5,0,1,20,0,10};
static const int option_max[TT_OPTION_SCALARS]={1,5,1,1,600,1,1,20,1,100,100,1,80};
void Tt_OptionsDefault(TtOptions *o){
    int i;memset(o,0,sizeof(*o));o->version=TT_OPTIONS_VERSION;
    for(i=0;i<TT_OPTION_SCALARS;++i)o->value[i]=option_defaults[i];
    for(i=0;i<10;++i){o->value[TT_OPTION_SCALARS+i*5]=1;o->value[TT_OPTION_SCALARS+i*5+3]=100;}
}
int Tt_OptionIndex(const char *key){
    int i;static const char *const field[]={"scale","max","start","percent","freeze"};char name[24];
    for(i=0;i<TT_OPTION_SCALARS;++i)if(!strcmp(key,option_keys[i]))return i;
    /* Persisted health keys are team/ordinal, mapped to lobby slots by the UI. */
    for(i=0;i<50;++i){snprintf(name,sizeof(name),"tag_hp%d%d_%s",i/25+1,(i/5)%5+1,field[i%5]);
        if(!strcmp(name,key))return TT_OPTION_SCALARS+i;}
    return -1;
}
int Tt_OptionsValid(const TtOptions *o){
    int i;if(o->version!=TT_OPTIONS_VERSION&&o->version!=TT_OPTION_COMBAT_VERSION)return 0;
    for(i=0;i<TT_OPTION_SCALARS;++i)if(o->value[i]<option_min[i]||o->value[i]>option_max[i])return 0;
    for(i=0;i<10;++i){const int32_t *v=&o->value[TT_OPTION_SCALARS+i*5];
        if(v[0]<1||v[0]>100||v[1]<0||v[1]>9999999||v[2]<0||v[2]>9999999||v[3]<1||v[3]>100||v[4]<0||v[4]>1)return 0;}
    if(o->version==TT_OPTION_COMBAT_VERSION)for(i=0;i<TT_MAX_PLAYERS;++i){
        const uint32_t word=(uint32_t)o->value[TT_OPTION_LEGACY_WORDS+i];
        const uint32_t damage=word&TT_COMBAT_PERCENT_MASK,resistance=(word>>9)&TT_COMBAT_PERCENT_MASK;
        if(word>=(TT_COMBAT_BOSS<<1)||damage<25||damage>400||resistance<25||resistance>400)return 0;
    }
    return 1;
}
uint32_t Tt_OptionsHash(const TtRoom *r){
    uint32_t h=2166136261u;int i,j;
    if(!r->options.version)return r->options_hash; /* Input-only direct coordinator. */
    h=(h^r->options.version)*16777619u;
    for(i=0;i<(r->options.version==TT_OPTION_COMBAT_VERSION?TT_OPTION_WORDS:TT_OPTION_LEGACY_WORDS);++i){uint32_t v=(uint32_t)r->options.value[i];for(j=0;j<4;++j)h=(h^(uint8_t)(v>>(j*8)))*16777619u;}
    for(i=0;i<TT_MAX_PLAYERS;++i)for(j=0;j<8;++j){unsigned v=r->players[i].items[j];h=(h^(v&255))*16777619u;h=(h^(v>>8))*16777619u;}
    return h?h:1;
}
int Tt_SetOptions(TtRoom *r,uint32_t sender,uint32_t epoch,const TtOptions *o){
    int rc=owner(r,sender,epoch,TT_LOBBY);if(rc)return rc;
    if(!o||!Tt_OptionsValid(o))return TT_BAD_CHOICE;
    if(r->options.version&&r->options.version!=o->version)return TT_MISMATCH;
    if(!memcmp(&r->options,o,sizeof(*o)))return TT_OK;
    r->options=*o;clear_choices(r);r->options_hash=Tt_OptionsHash(r);++r->epoch;++r->revision;return TT_OK;
}
int Tt_ChooseItems(TtRoom *r,uint32_t sender,uint32_t epoch,int seat,int chara,int costume,const uint16_t items[8]){
    int rc,i;for(i=0;i<8;++i)if(items[i]>350)return TT_BAD_CHOICE;
    if(!r->options.version||!r->options.value[TT_OPT_PRESETS])for(i=0;i<8;++i)if(items[i])return TT_BAD_CHOICE;
    rc=Tt_ChooseSeat(r,sender,epoch,seat,chara,costume);if(rc)return rc;
    memcpy(r->players[seat].items,items,16);r->options_hash=Tt_OptionsHash(r);return TT_OK;
}

int Tt_Capacity(const TtRoom *r){return capacity(r);}
int Tt_TeamCount(const TtRoom *r,int team,int occupied){
    int i,n=0;if(team<0||team>1)return 0;
    for(i=0;i<capacity(r);++i)if(r->players[i].team==team && (!occupied||r->players[i].member))++n;
    return n;
}
int Tt_SetTeams(TtRoom *r,uint32_t sender,uint32_t epoch,int left,int right){
    int i,a=1,b=1,total=left+right,rc=owner(r,sender,epoch,TT_LOBBY);
    if(rc)return rc;

    if(left<1||right<1||left>TT_TEAM_MAX||right>TT_TEAM_MAX||total>TT_MAX_PLAYERS)return TT_BAD_CHOICE;
    for(i=total;i<capacity(r);++i)if(r->players[i].member && !r->players[i].cpu)return TT_NOT_READY;
    for(i=0;i<TT_MAX_PLAYERS;++i){
        r->players[i].lobby_slot=i;
        if(i>=total){memset(&r->players[i],0,sizeof(TtPlayer));r->players[i].chara=-1;}
        else if(i==0)r->players[i].team=0;
        else if(i==1)r->players[i].team=1;
        else if(a<left){r->players[i].team=0;++a;}
        else{r->players[i].team=1;++b;}
    }
    r->slot_count=total;clear_choices(r);++r->epoch;++r->revision;
    return TT_OK;
}
int Tt_SetMode(TtRoom *r,uint32_t sender,uint32_t epoch,int free_for_all){
    int rc=owner(r,sender,epoch,TT_LOBBY);
    if(rc)return rc;
    if(free_for_all<0||free_for_all>1)return TT_BAD_CHOICE;

    if(r->free_for_all!=free_for_all){r->free_for_all=free_for_all;clear_choices(r);++r->epoch;++r->revision;}
    return TT_OK;
}
static uint32_t cpu_id(const TtRoom *r,int seat){
    uint32_t id=TT_CPU_MEMBER|1u;int i;
    for(;;++id){for(i=0;i<TT_MAX_PLAYERS;++i)if(i!=seat&&r->players[i].member==id)break;if(i==TT_MAX_PLAYERS)return id;}
}
int Tt_SetSlot(TtRoom *r,uint32_t sender,uint32_t epoch,int seat,int cpu,int difficulty){
    TtPlayer *p;int team,rc=owner(r,sender,epoch,TT_LOBBY);
    if(rc)return rc;
    if(seat<0||seat>=capacity(r)||cpu<0||cpu>1||difficulty<0||difficulty>4)return TT_BAD_CHOICE;
    p=&r->players[seat];if(p->member&&!p->cpu)return TT_NOT_OWNER;
    team=p->team;memset(p,0,sizeof(*p));p->team=team;p->lobby_slot=seat;p->chara=-1;
    if(cpu){p->member=cpu_id(r,seat);p->cpu=1;p->difficulty=difficulty;snprintf(p->name,sizeof(p->name),"CPU %02d",seat+1);}
    r->options_hash=Tt_OptionsHash(r);
    ++r->epoch;++r->revision;return TT_OK;
}
int Tt_SetBattleRules(TtRoom *r,uint32_t sender,uint32_t epoch,uint32_t rules){
    int rc=owner(r,sender,epoch,TT_LOBBY);
    if(rc)return rc;
    if(rules>TT_RULE_MASK)return TT_BAD_CHOICE;
    if(r->battle_rules==rules)return TT_OK;
    r->battle_rules=rules;++r->epoch;++r->revision;
    return TT_OK;
}
int Tt_SetArena(TtRoom *r,uint32_t sender,uint32_t epoch,int stage,int bgm,int attacks){
    int rc=owner(r,sender,epoch,TT_SELECT);if(rc)return rc;
    if(stage<0||stage>=0x63||bgm<0||bgm>65535||attacks<0||attacks>1)return TT_BAD_CHOICE;
    if(Tt_Turn(r,0)>=0||Tt_Turn(r,1)>=0)return TT_NOT_READY;
    r->stage=stage;r->bgm=bgm;r->attacks=attacks;++r->revision;return TT_OK;
}
int Tt_MovePlayer(TtRoom *r,uint32_t sender,uint32_t epoch,int from,int to){
    TtPlayer a,b;int ta,tb,rc=owner(r,sender,epoch,TT_LOBBY);if(rc)return rc;
    if(from<0||to<0||from>=capacity(r)||to>=capacity(r)||!r->players[from].member||r->players[from].cpu)return TT_BAD_CHOICE;
    a=r->players[from];b=r->players[to];ta=a.team;tb=b.team;
    r->players[from]=b;r->players[from].team=ta;r->players[from].lobby_slot=from;
    r->players[to]=a;r->players[to].team=tb;r->players[to].lobby_slot=to;
    r->options_hash=Tt_OptionsHash(r);
    ++r->epoch;++r->revision;return TT_OK;
}
void Tt_Open(TtRoom *r, uint32_t host, const char *name, int size, uint64_t content) {
    memset(r, 0, sizeof(*r));
    if (!host || (host & TT_CPU_MEMBER) || size < 2 || size > TT_TEAM_MAX) return;
    r->phase = TT_LOBBY;
    r->result_choice=-1;
    r->host = host;
    r->epoch = r->revision = 1;
    r->team_size = size;r->slot_count=size*2;
    {int i;for(i=0;i<TT_MAX_PLAYERS;++i){r->players[i].team=i&1;r->players[i].lobby_slot=i;}}
    r->delay = 2;
    r->content = content;
    clear_choices(r);
    r->players[0].member = host;
    name_copy(r->players[0].name, name);
}
int Tt_Count(const TtRoom *r) {
    int i, n = 0;
    for (i = 0; i < TT_MAX_PLAYERS; ++i) n += r->players[i].member != 0;
    return n;
}
int Tt_Seat(const TtRoom *r, uint32_t member) {
    int i;
    if (!member) return -1;
    for (i = 0; i < TT_MAX_PLAYERS; ++i) if (r->players[i].member == member) return i;
    return -1;
}
int Tt_Humans(const TtRoom *r) {
    int i,n=0;
    for(i=0;i<capacity(r);++i)n+=r->players[i].member&&!r->players[i].cpu;
    return n;
}
int Tt_FillCPU(TtRoom *r,uint32_t sender,uint32_t epoch,int difficulty) {
    int i,rc=owner(r,sender,epoch,TT_LOBBY);
    if(rc)return rc;
    if(difficulty<0||difficulty>4)return TT_BAD_CHOICE;
    for(i=0;i<capacity(r);++i)if(!r->players[i].member||r->players[i].cpu){
        TtPlayer *p=&r->players[i];int team=p->team;memset(p,0,sizeof(*p));p->team=team;p->lobby_slot=i;
        p->member=cpu_id(r,i);p->chara=-1;p->cpu=1;p->difficulty=difficulty;
        snprintf(p->name,sizeof(p->name),"CPU %s %d",p->team?"Red":"Blue",i+1);
    }
    r->options_hash=Tt_OptionsHash(r);++r->revision;return TT_OK;
}
int Tt_Join(TtRoom *r, uint32_t member, const char *name, uint64_t content) {
    int i;
    if (r->phase != TT_LOBBY) return TT_BAD_PHASE;
    if (!member || (member & TT_CPU_MEMBER) || Tt_Seat(r, member) >= 0) return TT_BAD_MEMBER;
    if (content != r->content) return TT_MISMATCH;
    for (i = 0; i < capacity(r); ++i) if (!r->players[i].member || r->players[i].cpu) {
        int team=r->players[i].team;memset(&r->players[i],0,sizeof(TtPlayer));r->players[i].chara=-1;r->players[i].team=team;r->players[i].lobby_slot=i;
        r->players[i].member = member;
        name_copy(r->players[i].name, name);
        r->options_hash=Tt_OptionsHash(r);
        ++r->revision;
        return TT_OK;
    }
    return TT_FULL;
}
static void restore_lobby_slots(TtRoom *r){
    TtPlayer restored[TT_MAX_PLAYERS];int i;memset(restored,0,sizeof(restored));
    for(i=0;i<capacity(r);++i)restored[r->players[i].lobby_slot]=r->players[i];
    memcpy(r->players,restored,sizeof(restored));
}
int Tt_Leave(TtRoom *r, uint32_t member) {
    int seat=Tt_Seat(r,member);
    if(seat<0)return TT_BAD_MEMBER;
    if(r->phase==TT_SELECT)restore_lobby_slots(r);
    seat=Tt_Seat(r,member);
    if (seat < 0) return TT_BAD_MEMBER;
    {int team=r->players[seat].team;memset(&r->players[seat],0,sizeof(TtPlayer));r->players[seat].team=team;r->players[seat].lobby_slot=seat;}
    ++r->epoch;
    ++r->revision;
    if (member == r->host || r->phase >= TT_LOADING) r->phase = TT_ABORTED;
    else { r->phase = TT_LOBBY; clear_choices(r); }
    r->options_hash=Tt_OptionsHash(r);
    return TT_OK;
}
int Tt_BeginSelect(TtRoom *r, uint32_t sender, uint32_t epoch) {
    int rc = owner(r, sender, epoch, TT_LOBBY);
    if (rc) return rc;
    if(Tt_Count(r)<2 || !Tt_TeamCount(r,0,1) || !Tt_TeamCount(r,1,1))return TT_NOT_READY;
    /* Freeze a compact actor order for the round. Empty lobby slots do not
       become invisible fighters or barriers waiting for nonexistent players. */
    {TtPlayer packed[TT_MAX_PLAYERS];int i,n=0;
        memset(packed,0,sizeof(packed));
        for(i=0;i<capacity(r);++i){r->players[i].lobby_slot=i;if(r->players[i].member)packed[n++]=r->players[i];}
        for(i=0;i<capacity(r);++i)if(!r->players[i].member)packed[n++]=r->players[i];
        memcpy(r->players,packed,sizeof(packed));
    }
    clear_choices(r);
    r->phase = TT_SELECT;
    ++r->revision;
    return TT_OK;
}
int Tt_Turn(const TtRoom *r, int team) {
    int i;
    if (r->phase != TT_SELECT || team < 0 || team > 1) return -1;
    for (i = 0; i < capacity(r); ++i)
        if (r->players[i].team==team && r->players[i].member && !r->players[i].confirmed) return i;
    return -1;
}
int Tt_CancelSelect(TtRoom *r,uint32_t sender,uint32_t epoch){
    int rc=owner(r,sender,epoch,TT_SELECT);
    if(rc)return rc;
    restore_lobby_slots(r);clear_choices(r);r->phase=TT_LOBBY;++r->epoch;++r->revision;
    return TT_OK;
}
int Tt_Choose(TtRoom *r, uint32_t sender, uint32_t epoch, int chara, int costume) {
    return Tt_ChooseSeat(r,sender,epoch,Tt_Seat(r,sender),chara,costume);
}
int Tt_ChooseSeat(TtRoom *r, uint32_t sender, uint32_t epoch,int seat,int chara,int costume) {
    int rc = valid(r, sender, epoch, TT_SELECT);
    if (rc) return rc;
    if(seat<0||seat>=capacity(r)||!r->players[seat].member)return TT_BAD_MEMBER;
    if(r->players[seat].cpu ? sender!=r->host : sender!=r->players[seat].member)return TT_NOT_OWNER;
    /* Undo only the requesting player's choice, or a CPU owned by the host.
     * Keep the draft phase, epoch and every other participant's selection. */
    if(chara==-1){
        if(!r->players[seat].confirmed)return TT_NOT_READY;
        r->players[seat].confirmed=0;r->players[seat].chara=-1;r->players[seat].costume=0;
        memset(r->players[seat].items,0,sizeof(r->players[seat].items));
        r->options_hash=Tt_OptionsHash(r);++r->revision;return TT_OK;
    }
    if(r->options.version!=TT_OPTION_COMBAT_VERSION){
        if(Tt_Turn(r,r->players[seat].team)!=seat)return TT_NOT_TURN;
    }else{
        if(r->players[seat].confirmed)return TT_NOT_TURN;
    }
    /* Profile validation must additionally check the selected fighter's actual
       costume count. These are wire bounds, not a claim every ID is installed. */
    if (chara < 0 || chara >= 512 || costume < 0 || costume >= 10) return TT_BAD_CHOICE;
    r->players[seat].chara = chara;
    r->players[seat].costume = costume;
    r->players[seat].confirmed = 1;
    ++r->revision;
    return TT_OK;
}
static uint64_t load_word(uint64_t hash,uint32_t value){
    int i;for(i=0;i<4;++i)hash=(hash^(uint8_t)(value>>(i*8)))*0x100000001b3ULL;return hash;
}
uint64_t Tt_LoadDigest(const TtRoom *r){
    uint64_t h=0xcbf29ce484222325ULL;int i,count=Tt_Capacity(r);
    h=load_word(h,TT_PROTOCOL);h=load_word(h,(uint32_t)r->free_for_all);h=load_word(h,(uint32_t)r->content);h=load_word(h,(uint32_t)(r->content>>32));
    h=load_word(h,(uint32_t)r->team_size);h=load_word(h,(uint32_t)r->time);h=load_word(h,(uint32_t)r->delay);h=load_word(h,(uint32_t)count);
    if(r->options_hash)h=load_word(h,r->options_hash);
    if(r->battle_rules){
        h=load_word(h,r->battle_rules);
        h=load_word(h,(uint32_t)r->stage);
    }
    if(count<0)count=0;if(count>TT_MAX_PLAYERS)count=TT_MAX_PLAYERS;
    for(i=0;i<count;++i){const TtPlayer *p=&r->players[i];
        h=load_word(h,(uint32_t)i);h=load_word(h,(uint32_t)p->team);h=load_word(h,(uint32_t)p->chara);
        h=load_word(h,(uint32_t)p->costume);h=load_word(h,(uint32_t)p->cpu);h=load_word(h,(uint32_t)p->difficulty);h=load_word(h,(uint32_t)p->confirmed);
    }return h;
}
int Tt_BeginLoad(TtRoom *r, uint32_t sender, uint32_t epoch, uint64_t digest) {
    int rc = owner(r, sender, epoch, TT_SELECT);
    if (rc) return rc;
    if (Tt_Turn(r, 0) >= 0 || Tt_Turn(r, 1) >= 0) return TT_NOT_READY;
    r->phase = TT_LOADING;
    r->load_digest = digest;
    ++r->revision;
    return TT_OK;
}
int Tt_Loaded(TtRoom *r, uint32_t sender, uint32_t epoch, uint64_t digest) {
    int i,rc = valid(r, sender, epoch, TT_LOADING);
    if (rc) return rc;
    if (digest != r->load_digest) return TT_MISMATCH;
    r->players[Tt_Seat(r, sender)].loaded = 1;
    if(sender==r->host)for(i=0;i<capacity(r);++i)if(r->players[i].cpu)r->players[i].loaded=1;
    ++r->revision;
    return TT_OK;
}
int Tt_Arm(TtRoom *r, uint32_t sender, uint32_t epoch) {
    int i, rc = owner(r, sender, epoch, TT_LOADING);
    if (rc) return rc;
    for (i = 0; i < capacity(r); ++i) if (r->players[i].member && !r->players[i].loaded) return TT_NOT_READY;
    r->phase = TT_ARMED;
    ++r->revision;
    return TT_OK;
}
int Tt_Start(TtRoom *r, uint32_t sender, uint32_t epoch) {
    int i, rc = owner(r, sender, epoch, TT_ARMED);
    if (rc) return rc;
    for (i = 0; i < capacity(r); ++i) if (r->players[i].member && !r->players[i].armed) return TT_NOT_READY;
    r->result_choice=-1;
    r->phase = TT_FIGHT;
    ++r->revision;
    return TT_OK;
}
int Tt_ArmAck(TtRoom *r, uint32_t sender, uint32_t epoch) {
    int i,rc = valid(r, sender, epoch, TT_ARMED);
    if (rc) return rc;
    r->players[Tt_Seat(r, sender)].armed = 1;
    if(sender==r->host)for(i=0;i<capacity(r);++i)if(r->players[i].cpu)r->players[i].armed=1;
    ++r->revision;
    return TT_OK;
}
void Tt_InputsReset(TtInputs *q, uint32_t epoch, int seats) {
    memset(q, 0, sizeof(*q));
    q->epoch = epoch;
    q->seats = seats >= 2 && seats <= TT_MAX_PLAYERS ? seats : 0;
}
int Tt_InputPut(TtInputs *q, uint32_t epoch, int seat, uint32_t frame, const unsigned char *pad) {
    TtInputFrame *f;
    uint32_t bit;
    if (q->epoch != epoch || (uint32_t)(frame - q->next) >= TT_INPUT_HISTORY) return TT_STALE;
    if (!pad || seat < 0 || seat >= q->seats) return TT_BAD_MEMBER;
    f = &q->ring[frame % TT_INPUT_HISTORY];
    if (f->frame != frame) { f->frame = frame; f->mask = 0; }
    bit = 1u << seat;
    if (f->mask & bit) return memcmp(f->pad[seat], pad, TT_PAD_BYTES) ? TT_MISMATCH : TT_OK;
    memcpy(f->pad[seat], pad, TT_PAD_BYTES);
    f->mask |= bit;
    return TT_OK;
}
int Tt_InputTake(TtInputs *q, unsigned char out[TT_MAX_PLAYERS][TT_PAD_BYTES]) {
    TtInputFrame *f = &q->ring[q->next % TT_INPUT_HISTORY];
    if (!q->seats || !out || f->frame != q->next || f->mask != (1u << q->seats) - 1u) return TT_NOT_READY;
    memcpy(out, f->pad, sizeof(f->pad));
    f->mask = 0;
    ++q->next;
    return TT_OK;
}
int Tt_Target(int actor, int current, int direction, int count, uint32_t alive) {
    int i, k, step = direction < 0 ? -1 : 1;
    if (actor < 0 || actor >= count || count < 4 || count > TT_MAX_PLAYERS || !(alive & (1u << actor))) return -1;
    k = current >= 0 && current < count ? current : actor;
    for (i = 0; i < count; ++i) {
        k = (k + count + step) % count;
        if ((k & 1) != (actor & 1) && (alive & (1u << k))) return k;
    }
    return -1;
}
