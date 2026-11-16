/* Native TAG TEAM session model. GPL-3.0-or-later, as the enclosing port.
 * Separate from the two-peer protocol: no PS2 struct layouts are enlarged.
 * All mutations are host-authoritative and validated before changing state.
 */
#ifndef PORT_TAG_TEAM_H
#define PORT_TAG_TEAM_H
#include <stdint.h>
#define TT_MAX_PLAYERS 10
#define TT_TEAM_MAX 5
#define TT_PAD_BYTES 18 /* 0..3 buttons, 4..5 stick, 6 target, 7 status, 8 sequence, 10 camera distance, 17 cooperative commands */
#define TT_INPUT_HISTORY 512
#define TT_PROTOCOL 0x54540009u
/* Versioned private-room rules, carried on the reliable room channel. */
#define TT_OPTIONS_VERSION 1u
#define TT_OPTION_SCALARS 13
#define TT_OPTION_COMBAT_VERSION 2u
#define TT_OPTION_LEGACY_WORDS (TT_OPTION_SCALARS + TT_MAX_PLAYERS * 5)
#define TT_OPTION_WORDS (TT_OPTION_LEGACY_WORDS + TT_MAX_PLAYERS)
#define TT_COMBAT_PERCENT_MASK 511u
#define TT_COMBAT_BOSS (1u << 18)
#define TT_COMBAT_DEFAULT (100u | (100u << 9))
enum {TT_OPT_PRESETS,TT_OPT_BEANS,TT_OPT_JOINT,TT_OPT_FUSION,TT_OPT_FUSION_TIME,
      TT_OPT_REVIVE_1,TT_OPT_REVIVE_2,TT_OPT_REVIVE_TIME,TT_OPT_REVIVE_CHARGE,
      TT_OPT_REVIVE_HP,TT_OPT_REVIVE_RADIUS,TT_OPT_REVIVE_CPU,TT_OPT_REVIVE_OPACITY};
typedef struct TtOptions {uint32_t version;int32_t value[TT_OPTION_WORDS];} TtOptions;
#define TT_CPU_MEMBER 0x80000000u
/* Zero preserves the legacy defaults. The low three bits encode referee + 1. */
#define TT_RULE_REFEREE_MASK 7u
#define TT_RULE_DESTROY_STAGE 8u
#define TT_RULE_CPU_FORMS_DISABLED 16u
#define TT_RULE_MASK 63u
enum { TT_CLOSED, TT_LOBBY, TT_SELECT, TT_LOADING, TT_ARMED, TT_FIGHT, TT_ABORTED };
enum { TT_OK, TT_BAD_PHASE, TT_BAD_MEMBER, TT_FULL, TT_NOT_OWNER,
       TT_NOT_TURN, TT_BAD_CHOICE, TT_NOT_READY, TT_STALE, TT_MISMATCH, TT_DESYNC };
typedef struct TtPlayer {
    uint32_t member; /* Nonzero connection identity, never a client-supplied seat. */
    char name[20];
    uint16_t items[8];
    int chara, costume, confirmed, loaded, armed, cpu, difficulty, team, lobby_slot,ended;
} TtPlayer;
typedef struct TtRoom {
    uint32_t epoch, revision, host;
    int phase, team_size, time, delay, rollback, slot_count;
    int stage,bgm,attacks,result_choice,free_for_all;
    uint32_t seed;
    uint32_t battle_rules, options_hash;
    TtOptions options;
    uint64_t content, load_digest;
    TtPlayer players[TT_MAX_PLAYERS]; /* Team identity is explicit and independent of the seat. */
} TtRoom;
typedef struct TtInputFrame {
    uint32_t frame, mask;
    unsigned char pad[TT_MAX_PLAYERS][TT_PAD_BYTES];
} TtInputFrame;
typedef struct TtInputs {
    uint32_t epoch, next;
    int seats;
    TtInputFrame ring[TT_INPUT_HISTORY];
} TtInputs;
void Tt_OptionsDefault(TtOptions *o);
int Tt_OptionsValid(const TtOptions *o);
int Tt_OptionIndex(const char *key);
uint32_t Tt_OptionsHash(const TtRoom *r);
int Tt_SetOptions(TtRoom *r,uint32_t sender,uint32_t epoch,const TtOptions *o);
int Tt_ChooseItems(TtRoom *r,uint32_t sender,uint32_t epoch,int seat,int chara,int costume,const uint16_t items[8]);
void Tt_Open(TtRoom *r, uint32_t host, const char *name, int size, uint64_t content);
int Tt_Count(const TtRoom *r);
int Tt_Capacity(const TtRoom *r);
int Tt_TeamCount(const TtRoom *r,int team,int occupied);
int Tt_SetTeams(TtRoom *r,uint32_t sender,uint32_t epoch,int left,int right);
int Tt_SetMode(TtRoom *r,uint32_t sender,uint32_t epoch,int free_for_all);
int Tt_SetSlot(TtRoom *r,uint32_t sender,uint32_t epoch,int seat,int cpu,int difficulty);
int Tt_MovePlayer(TtRoom *r,uint32_t sender,uint32_t epoch,int from,int to);
int Tt_SetArena(TtRoom *r,uint32_t sender,uint32_t epoch,int stage,int bgm,int attacks);
int Tt_SetBattleRules(TtRoom *r,uint32_t sender,uint32_t epoch,uint32_t rules);
int Tt_Humans(const TtRoom *r);
int Tt_FillCPU(TtRoom *r, uint32_t sender, uint32_t epoch, int difficulty);
int Tt_Seat(const TtRoom *r, uint32_t member);
int Tt_Join(TtRoom *r, uint32_t member, const char *name, uint64_t content);
int Tt_Leave(TtRoom *r, uint32_t member);
int Tt_BeginSelect(TtRoom *r, uint32_t sender, uint32_t epoch);
int Tt_CancelSelect(TtRoom *r, uint32_t sender, uint32_t epoch);
int Tt_Turn(const TtRoom *r, int team);
int Tt_Choose(TtRoom *r, uint32_t sender, uint32_t epoch, int chara, int costume);
int Tt_ChooseSeat(TtRoom *r, uint32_t sender, uint32_t epoch, int seat, int chara, int costume);
int Tt_BeginLoad(TtRoom *r, uint32_t sender, uint32_t epoch, uint64_t digest);
int Tt_Loaded(TtRoom *r, uint32_t sender, uint32_t epoch, uint64_t digest);
uint64_t Tt_LoadDigest(const TtRoom *r);
int Tt_Arm(TtRoom *r, uint32_t sender, uint32_t epoch);
int Tt_ArmAck(TtRoom *r, uint32_t sender, uint32_t epoch);
int Tt_Start(TtRoom *r, uint32_t sender, uint32_t epoch);
void Tt_InputsReset(TtInputs *q, uint32_t epoch, int seats);
int Tt_InputPut(TtInputs *q, uint32_t epoch, int seat, uint32_t frame, const unsigned char *pad);
int Tt_InputTake(TtInputs *q, unsigned char out[TT_MAX_PLAYERS][TT_PAD_BYTES]);
int Tt_Target(int actor, int current, int direction, int count, uint32_t alive);
#endif
