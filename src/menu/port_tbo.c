#include <stdio.h>
extern int Port_TagTeams(int,int),Port_TagSlot(int,int,int),Port_TagSlots(void),Port_TagTeam(int),Port_TagTeamIndex(int),Port_TagTeamCount(int),Port_TagTeamSlots(int),Port_TagCanStart(void);
#include "common.h"
#include "../../port/src/gs/online_language.h"
#include "menu/menu_e.h"
#include "sys/pad.h"
#include "sys/save.h"

#ifdef PORT
#include <stdlib.h>
/*
 * PC only: "Tenkaichi Budokai Online", the main menu's item 11 (menu_a.c). The port's screen mode (port_screen.c)
 * runs this screen. Its own look for online play, after the mockups of online_design: every picture, piece,
 * letter and button is its own (tbo/ui/*.png, drawn by the renderer at full resolution; see "the pictures" below).
 *
 * The hub lists nine modes. What runs today is what the room-code lobby of port/src/gs/net.c can do (the same as
 * the Dragon Net Battle window, without the window):
 *   Create Room   the host's rules, then a room code to give the other player;
 *   Join Room     a pad keypad (the PC keyboard types too) for the other player's code;
 *   the search    a Dragon Radar while the two games find each other; square: "Train While Waiting" leaves for the
 *                 Ultimate Training menu with the room still open (net.c polls it each blank and starts the match
 *                 from wherever the game is when the other player comes);
 *   Profile       the name the other player sees and the defaults of the connection.
 * Quick Match, Ranked Match, Friends, Tournament, Replays and Leaderboard need the matchmaking service to do more
 * than room codes (a public queue, player ids, ratings): they are listed, marked SOON, and say what they will be.
 * Once both players are there the session is the game's own versus mode (character select, the fight, results).
 */

/* The PC keyboard (port/src/gs/gs_input.c): on a keypad it types (a character at a time: 8 backspace, 13 enter,
   27 escape (back), 0 none) and the arrow keys and the controller move on it. On the other screens it is the pad,
   and Esc asks to close the game, as everywhere. */
extern void PortTbo_TextMode(int on);
extern int PortTbo_TextRead(void);
extern void PortTbo_Copy(const char *text); /* onto the clipboard ("Copy Code") */
extern void PortTbo_Paste(void);            /* the clipboard's room code, typed (Join Room's PASTE) */

/* The game's windows of every menu (battle/view_a_b.c, view_a_c.c), made from the Duel menu's data as
   DuelMenu_Init makes them: the heading's window (IconWin: the cloud swirls in the corners and the line under the
   heading) and the message window (MsgWin: the teal box with the meander corners). */
extern void MsgWin_SetText(void *text); /* (the rest: menu/menu_a.h) */
extern void IconWin_PortHideWords(void);
extern void MsgWin_PortDrawString(u16 *str); /* (battle/view_a_b.c) the box with a line of ours, in the game's font */
extern void MsgWin_PortDrawStringHd(u16 *str);
extern unsigned PortTbo_TextGeneration(void);
extern s32 MsgWin_PortLineRoom(s32 right);
extern void MsgWin_PortSameTop(void);
extern s32 Font_GetWidth(u16 *str); /* (battle/col_c.c) a line's width in the game's font */
#define Port_TextMode PortTbo_TextMode
#define Port_TextRead PortTbo_TextRead
static void Port_EscQuit(s32 on) {
    (void)on;
}
static s32 Port_EscTake(void) {
    return 0;
}

/* The fighters' names (Train While Waiting's choice), by character id. */
static const char *const sTbNames[] = {
    "Goku (Early)",
    "Goku (Med)",
    "Goku (Med) Super Saiyan",
    "Goku (End)",
    "Goku (End) Super Saiyan",
    "Goku (End) Super Saiyan 2",
    "Goku (End) Super Saiyan 3",
    "Goku (GT)",
    "Goku (GT) Super Saiyan",
    "Goku (GT) Super Saiyan 3",
    "Goku (GT) Super Saiyan 4",
    "Kid Goku",
    "Great Ape",
    "Kid Gohan",
    "Teen Gohan",
    "Teen Gohan Super Saiyan",
    "Teen Gohan Super Saiyan 2",
    "Gohan",
    "Gohan Super Saiyan",
    "Gohan Super Saiyan 2",
    "Great Saiyaman",
    "Ultimate Gohan",
    "Piccolo (Early)",
    "Piccolo (End)",
    "Nail",
    "Krillin",
    "Yamcha",
    "Tien",
    "Chiaotzu",
    "Vegeta (Scouter)",
    "Vegeta (Scouter) Great Ape",
    "Vegeta",
    "Vegeta Super Saiyan",
    "Super Vegeta",
    "Vegeta (2nd Form)",
    "Vegeta (2nd Form) Super Saiyan",
    "Vegeta (2nd Form) Super Saiyan 2",
    "Majin Vegeta",
    "Vegeta (2nd Form) Super Saiyan 4",
    "Trunks (Sword)",
    "Trunks (Sword) Super Saiyan",
    "Trunks",
    "Trunks Super Saiyan",
    "Super Trunks",
    "Kid Trunks",
    "Kid Trunks Super Saiyan",
    "Goten",
    "Goten Super Saiyan",
    "Gotenks",
    "Gotenks Super Saiyan",
    "Gotenks Super Saiyan 3",
    "Vegito",
    "Super Vegito",
    "Super Gogeta",
    "Gogeta Super Saiyan 4",
    "Mr Satan",
    "Videl",
    "Great Saiyaman 2",
    "Supreme Kai",
    "Kibito Kai",
    "Yajirobe",
    "Paikuhan",
    "Tapion",
    "Pan",
    "Uub",
    "Majuub",
    "Master Roshi",
    "Master Roshi Full Power",
    "Grandpa Gohan",
    "Namu",
    "Hatchan",
    "King Vegeta",
    "King Vegeta Great Ape",
    "Bardock",
    "Bardock Great Ape",
    "Celipa",
    "Celipa Great Ape",
    "Raditz",
    "Raditz Great Ape",
    "Saibamen",
    "Nappa",
    "Nappa Great Ape",
    "Zarbon",
    "Zarbon Transformed",
    "Dodoria",
    "Cui",
    "Ginyu",
    "Recoome",
    "Burter",
    "Jeice",
    "Guldo",
    "Frieza 1st Form",
    "Frieza 2nd Form",
    "Frieza 3rd Form",
    "Frieza Final Form",
    "Frieza 100% Power",
    "Mecha Frieza",
    "King Cold",
    "Appule",
    "Frieza Soldier",
    "Android 16",
    "Android 17",
    "Android 18",
    "Android 19",
    "Android 2",
    "Cell 1st Form",
    "Cell 2nd Form",
    "Perfect Cell",
    "Super Perfect Cell",
    "Cell Jr",
    "Babidi",
    "Dabura",
    "Majin Buu",
    "Evil Buu",
    "Super Buu",
    "Super Buu Gotenks Absorbed",
    "Super Buu Gohan Absorbed",
    "Kid Buu",
    "Garlic Jr",
    "Super Garlic Jr",
    "Wheelo",
    "Turles",
    "Turles Great Ape",
    "Slug",
    "Slug Giant",
    "Sauza",
    "Cooler",
    "Cooler Final Form",
    "Metal Cooler",
    "Android 13",
    "Android 13 Fusion",
    "Broly",
    "Broly Super Saiyan",
    "Broly Legendary Super Saiyan",
    "Zangya",
    "Bojack",
    "Bojack 100% Power",
    "Janemba",
    "Super Janemba",
    "Hildegarn",
    "Baby Vegeta",
    "Super Baby 1",
    "Super Baby 2",
    "Super Baby Great Ape",
    "Super 17",
    "Nuova Shenron",
    "Shin Shenron",
    "Omega Shenron",
    "Taopaipai",
    "Cyborg Tao",
    "General Blue",
    "Akuman",
    "Pilaf Machine",
    "Pilaf Machine Fusion",
    "Tambourine",
    "Demon King Piccolo",
    "Arale",
    "Chichi",
    "Spopovich",
    "Future Gohan",
    "Future Gohan Super Saiyan",
};
#include "menu/tbo_art.h"
#include "menu/tbo_sizes.h"
#include "menu/port_screen.h"

extern void Job_Run(void);
extern void Gfx_BeginFrame(void);
extern void Gfx_EndFrame(s32 arg);
extern void Dma_Flush(void);
extern void Dma_ResetBuffers(void);
extern void File_Stub264D90(void);
extern void ColorFade_StartIn(s32 r, s32 g, s32 b, s32 frames);
extern void ColorFade_StartOut(s32 r, s32 g, s32 b, s32 frames);
extern void ColorFade_Update(void);
extern void ColorFade_Draw(void);
extern s32 ColorFade_IsInDone(void);
extern s32 ColorFade_IsOutDone(void);
extern s32 sceGsSyncPath(s32 mode, u16 timeout);

/* port/src/gs/net.c, state.c, plat_settings.c */
extern void Port_NetOptions(int rollback, int delay);
extern void Port_NetRulesSet(int type, int dp, int time);
extern const char *Port_NetNameGet(void);
extern void Port_NetNameSave(const char *name);
extern int Port_LobbyStartCode(int host, const char *code);
extern int Port_LobbyPoll(void);
extern void Port_LobbyCancel(void);
extern void Port_LobbyLaunch(void);
extern int Port_NetSessionsEnded(void); /* gs/net.c */
extern const char *Port_LobbyCode(void);
extern const char *Port_LobbyError(void);
extern int Port_LobbyOther(void);
extern void Port_LobbyBackground(int on);
extern int Port_LobbyInBackground(void);
extern int Port_RollCan(void);
extern int Port_LobbyStartQuick(int queue);   /* a quick match: 0 one on one, 1 teams (the service's queues S, T) */
extern void Port_LobbyQueuesAsk(void);        /* asks how many wait in each queue */
extern int Port_LobbyQueueWaiting(int queue); /* ... as last heard, -1 if not known */
extern void Port_NetSpeedSet(int percent);
extern int Port_NetSpeedStandard(void);
extern void Port_NetNameSet(const char *name);
extern void Battle_ClearWork(void);
extern void BattleSetup_SetRule(s32 screenMode, s32 mode, s32 bgm, s32 timeLimit, s32 announcer, s32 stage, s32 unk10);
extern void BattleSetup_SetSide(s32 sideNo, s32 control, s32 pad, s32 memberCount, s32 unk1FC, s32 unk200, s32 lead, void *unk);
extern void BattleSetup_SetMember(s32 sideNo, s32 idx, s32 chara, s32 costume, s32 variant, s32 cpuLevel, f32 health, void *items);
extern void BattleSetup_Finish(void);
extern int Port_DataFingerprint(unsigned long long *out); /* plat_fprint.c: 1 known, 0 being worked out, -1 not done */
extern int Port_DataFingerprintOriginal(void);           /* 1 the data the game computes with is the original disc's */
extern const char *Port_DataFingerprintText(void);
extern int Port_Setting(const char *name, int def);
extern void Port_SettingSave(const char *name, int value);
extern void Port_SettingsWrite(void);

/* TAG TEAM has its own transport. It never joins the existing two-peer pool. */
extern int Port_TagConnect(const char *server),Port_TagNetwork(void),Port_TagChatCount(void);
extern int Port_TagCopyCode(void),Port_TagJoinCode(const char *code);
extern int Port_TagMovePlayer(int from,int to);
extern int Port_TagDataFingerprint(unsigned long long *out);
extern const char *Port_TagEndpoint(void),*Port_TagChatLine(int i);
extern void Port_TagInitialTeams(int,int),Port_TagChatSend(const char *);
extern void Port_TagClose(void), Port_TagPoll(void);
extern void Port_TagOpen(int size, int time, int delay, const char *name, unsigned long long content);
extern void Port_TagCPUOptions(int fill, int difficulty);
extern int Port_TagHumans(void), Port_TagCPU(int seat), Port_TagDifficulty(int seat);
extern void Port_TagJoin(const char *code, const char *name, unsigned long long content);
extern void Port_TagCommand(int command, int a, int b);
extern int Port_TagPhase(void), Port_TagCount(void), Port_TagCapacity(void), Port_TagSeat(void), Port_TagTurn(void), Port_TagHost(void);
extern int Port_TagDraftPending(void);
extern const char *Port_TagPlayerName(int seat), *Port_TagCode(void), *Port_TagError(void);
/* the screens */
#define TB_HUB 0
#define TB_CREATE 1
#define TB_JOIN 2
#define TB_WAIT 3
#define TB_PROFILE 4
#define TB_NAME 5
#define TB_QUICK 6
#define TB_SPAR 7 /* Train While Waiting: the fighters of the sparring fight */
#define TB_BLOCK 8 /* "Cannot Go Online": this game's data is not the disc's */
#define TB_TAG 9
#define TB_TAG_ROOM 10
#define TB_FIGHTERS 161
#define TB_TOURNAMENT_STAGE 4 /* the World Tournament Stage, at noon */

/* the hub's modes, in the order of the list */
#define HUB_QUICK 0
#define HUB_RANKED 1
#define HUB_CREATE 2
#define HUB_JOIN 3
#define HUB_TAG 4
#define HUB_FRIENDS 5
#define HUB_TOURNAMENT 6
#define HUB_REPLAYS 7
#define HUB_LEADERBOARD 8
#define HUB_PROFILE 9
#define HUB_COUNT 10

/* Create Room's rows */
#define CR_BATTLE 0
#define CR_TIME 1
#define CR_DELAY 2
#define CR_ROLL 3
#define CR_OPEN 4
#define CR_COUNT 5

/* Profile's rows */
/* (the input delay and the rollback are the room's: Create Room sets them; a joiner gets the host's, a quick match
   has its own) */
#define PR_NAME 0
#define PR_COUNT 1

#define KEY_COLS 9
#define KEY_ROWS 4
#define CODE_LEN 6

typedef struct TboScreen {
    s32 tagLeft,tagRight,tagRoomCur;
    s32 tagCur, tagSize, tagJoin, tagTestSent, tagCPU, tagDifficulty, tagAttacks,tagDelay;
    s32 tagChatTyping,tagChatLen,tagMoving;char tagChat[160];
    s32 frames;
    s32 mode;
    s32 modeT;
    s32 leaving;
    s32 train;          /* it is left for the Ultimate Training, the room still open */
    s32 note;
    char noteLine[96];
    s32 hubCur;
    f32 hubY;           /* where the hub's cursor plate is drawn: it glides */
    /* the host's choices (kept in the settings) */
    s32 battle;         /* 0 one on one, 1 teams of up to five */
    s32 time;           /* 0..4: 60, 90, 180, 240 seconds, no limit */
    s32 delay;          /* -1 automatic, else frames */
    s32 roll;           /* 0 off, else frames (2..8) */
    s32 crCur;
    s32 prCur;
    /* the keypad */
    char code[CODE_LEN + 1];
    s32 codeLen;
    s32 keyCol;
    s32 keyRow;         /* KEY_ROWS: the row of DELETE / JOIN */
    /* the search */
    s32 host;           /* this side opened the room */
    s32 waitFrom;       /* frames when the search began */
    s32 found;          /* the other player is there: the match is starting */
    s32 ended;          /* Port_NetSessionsEnded when it started: a change says the match has been played */
    s32 waitBack;       /* the screen a cancelled search goes back to */
    /* Quick Match */
    s32 queue;          /* 0 one on one, 1 teams */
    s32 qkCur;
    s32 asked;          /* frames when the queues were last asked about */
    /* Train While Waiting */
    s32 spMe;           /* your fighter */
    s32 spFoe;          /* your sparring partner */
    s32 spCur;
    s32 blockBack;      /* the screen "Cannot Go Online" goes back to */
    s32 typing;         /* the PC keyboard types (circle on a keypad); else its keys are the pad's, as everywhere */
    s32 lower;          /* the name pad shows small letters */
    /* typing the name */
    char typed[20];
    s32 typedLen;
} TboScreen;

static TboScreen *sTb;
static s32 sTbBgKind;  /* what the search left open for "Train While Waiting" is: 0 joining, 1 hosting, 2 a quick match */
static s32 sTbBgQueue;
static s32 sTbBgFrames; /* how long it had been searching when the sparring fight began; the sign counts on from it */
s32 gPortTboSpar;       /* the battle that runs is Train While Waiting's sparring fight (its sign, its pause menu) */
static char sTbBgCode[CODE_LEN + 1];
static f32 Tb_Beat(s32 period);

static char *sTbHubName[HUB_COUNT] = {"Quick Match", "Ranked Match", "Create Room", "Join Room", "Tag Team", "Friends", "Tournament", "Replays", "Leaderboard", "Profile"};
static const s32 sTbHubReady[HUB_COUNT] = {1, 0, 1, 1, 1, 0, 0, 0, 0, 1};
static char *sTbHubSay[HUB_COUNT] = {
    "Fight whoever is searching right now! No time limit, and you can train while you wait.",
    "Fights that move your rank, season by season. That one is coming soon!",
    "Open a room with your own rules, then give your rival its code. No port forwarding needed!",
    "Got a room code from your rival? Type it in and jump right into their room!",
    "Simultaneous team battles for up to ten fighters. Arrange the teams and choose from five CPU difficulty levels.",
    "Your rivals, who is online, and invites. That one is coming soon!",
    "The Tenkaichi Budokai itself: brackets of 4 to 16 players. That one is coming soon!",
    "Your last matches, saved to watch again. That one is coming soon!",
    "The strongest in the world, among your friends, and with each fighter. Coming soon!",
    "The name your rivals see.",
};
static char *sTbTime[5] = {"60 seconds", "90 seconds", "180 seconds", "240 seconds", "No limit"};
static char *sTagDifficulty[5]={"Very easy","Easy","Normal","Hard","Very hard"};
static char *sTagTime[5]={"60 s","90 s","180 s","240 s","No limit"};
static char *sTbDifficulty[5] = {"VERY EASY", "EASY", "NORMAL", "HARD", "VERY HARD"};
static char sTbKeys[KEY_ROWS][KEY_COLS + 1] = {"ABCDEFGHI", "JKLMNOPQR", "STUVWXYZ0", "123456789"};

/* ---------------------------------------------------------------------------------------------- small things */
static f32 Tb_Beat(s32 period) {
    s32 k = sTb->frames % period;

    return (f32)(k * 2 < period ? k * 2 : (period - k) * 2) / (f32)period;
}

static f32 Tb_In(s32 delay) {
    f32 t = (f32)(sTb->modeT - delay) / 16.0f;

    if (t < 0.0f) {
        t = 0.0f;
    }
    if (t > 1.0f) {
        t = 1.0f;
    }
    t = 1.0f - t;
    return 1.0f - t * t * t;
}

static void Tb_Note(char *line) {
    Ps_Copy(sTb->noteLine, line, 96);
    sTb->note = 150;
}

static void Tb_SetMode(s32 mode) {
    sTb->typing = mode == TB_JOIN || mode == TB_NAME; /* the keyboard types on a keypad (elsewhere I is triangle ...) */
    Port_TextMode(sTb->typing);
    if (sTb->mode != mode) { /* (the page that is up again, as when the rival comes on the search: it does not come in anew) */
        sTb->modeT = 0;
        if(mode==TB_TAG_ROOM){sTb->note=0;sTb->noteLine[0]=0;}
    }
    sTb->mode = mode;
}

static void Tb_SaveChoices(void) {
    Port_SettingSave("tbo_battle", sTb->battle);
    Port_SettingSave("tbo_time", sTb->time);
    Port_SettingSave("net_delay2", sTb->delay);
    Port_SettingSave("net_rollback2", sTb->roll);
    Port_SettingsWrite();
}

static void Tb_LoadChoices(void) {
    sTb->tagSize = Port_Setting("tag_team_size", 2);
    sTb->tagAttacks=1;
    sTb->tagCPU = Port_Setting("tag_team_cpu", 1) == 1;
    sTb->tagDifficulty = Port_Setting("tag_team_difficulty", 2);
    if (sTb->tagDifficulty < 0 || sTb->tagDifficulty > 4) sTb->tagDifficulty = 2;
    if (sTb->tagSize < 2 || sTb->tagSize > 5) sTb->tagSize = 2;
    sTb->tagLeft=Port_Setting("tag_team_left",sTb->tagSize);sTb->tagRight=Port_Setting("tag_team_right",sTb->tagSize);
    if(sTb->tagLeft<1||sTb->tagRight<1||sTb->tagLeft+sTb->tagRight>10)sTb->tagLeft=sTb->tagRight=2;
    sTb->battle = Port_Setting("tbo_battle", 0);
    sTb->time = Port_Setting("tbo_time", 3);
    sTb->delay = Port_Setting("net_delay2", -1);
    sTb->roll = Port_RollCan() ? Port_Setting("net_rollback2", 8) : 0;
    if (sTb->battle < 0 || sTb->battle > 1) {
        sTb->battle = 0;
    }
    if (sTb->time < 0 || sTb->time > 4) {
        sTb->time = 3;
    }
    if (sTb->delay < -1 || sTb->delay > 6) {
        sTb->delay = -1;
    }
    if (sTb->roll < 0 || sTb->roll > 8) {
        sTb->roll = 8;
    }
    sTb->roll &= ~1;
    sTb->queue = Port_Setting("tbo_queue", 0) == 1;
    sTb->spMe = Port_Setting("tbo_spar_me", 0);
    sTb->spFoe = Port_Setting("tbo_spar_foe", 31);
    if (sTb->spMe < 0 || sTb->spMe >= TB_FIGHTERS) {
        sTb->spMe = 0;
    }
    if (sTb->spFoe < 0 || sTb->spFoe >= TB_FIGHTERS) {
        sTb->spFoe = 31;
    }
}

static void Tb_DelayText(char *out, s32 delay) {
    if (delay < 0) {
        sprintf(out, "Automatic");
    } else {
        sprintf(out, (char *)PortOnline_Text("%d frame"), (int)delay);
        if (delay != 1) {
            Ps_Cat(out, "s", 32);
        }
    }
}

static void Tb_RollText(char *out, s32 roll) {
    if (!Port_RollCan()) {
        sprintf(out, "Unavailable");
    } else if (roll <= 0) {
        sprintf(out, "Off");
    } else {
        sprintf(out, (char *)PortOnline_Text("Up to %d frames"), (int)roll);
    }
}

/* ---------------------------------------------------------------------------------------------- the rule */
/* Online matches are only between two matching BT4 B14 Rev2 builds. Fighter mods are switched off by themselves while a session
   runs (plat_mods.c); what is checked here is the rest of the data the game computes with, against the original
   disc (the gameplay fingerprint, plat_fprint.c: files added under gamedata/mods count). Returns 1 when a search may
   start; else the note or the "Cannot Go Online" window says why. */
static s32 Tb_Vanilla(void) {
    extern s32 Port_DataState(void);
    if(Port_DataState()==1)return 1;
    Tb_Note("BT4 base data is missing or unsupported. Repair the game data with Install.exe.");
    Snd_PlaySe(1,2);return 0;
}

static void Tb_BlockUpdate(s32 live) {
    Pad *pad = &gPad[0];

    Ps_Prompt(0, sTb->blockBack==TB_TAG||(sTb->blockBack==TB_JOIN&&sTb->tagJoin)?"Back":"Back");
    if (live && (pad->gamePressed & (PADG_CROSS | PADG_CIRCLE | PADG_TRIANGLE))) {
        Snd_PlaySe(1, 2);
        Tb_SetMode(sTb->blockBack);
    }
}

/* ---------------------------------------------------------------------------------------------- the search */
static void Tb_BeginSearch(s32 host) {
    s32 state;

    if (!Tb_Vanilla()) {
        return;
    }
    if (host) {
        Port_NetOptions(sTb->roll, sTb->delay < 0 ? -2 : sTb->delay);
        Port_NetRulesSet(sTb->battle, 0, sTb->time);
        Tb_SaveChoices();
    } else {
        Port_NetOptions(-1, -1); /* joining: the host's choices arrive with its answer */
    }
    Port_NetNameGet(); /* (the saved name is the one sent) */
    state = Port_LobbyStartCode(host, host ? "" : sTb->code);
    if (state < 0) {
        Tb_Note("THE NETWORK COULD NOT BE OPENED ON THIS PC");
        return;
    }
    sTb->host = host;
    sTb->waitFrom = sTb->frames;
    sTb->found = 0;
    sTb->waitBack = sTb->mode;
    Tb_SetMode(TB_WAIT);
}

/* What the search says (each frame on the screens that wait): the rival came (the match starts), or it failed.
   Returns 1 when the screen that called has nothing more to do this frame. */
static s32 Tb_SearchNews(void) {
    s32 state;

    /* The session is over: the game is back as it was when it was asked for (the rival had just come), so these
       screens are again where they were then. The match has been played: back to the first page. */
    if (sTb->found && Port_NetSessionsEnded() != sTb->ended) {
        sTb->found = 0;
        Tb_SetMode(TB_HUB);
        return 1;
    }
    state = sTb->found ? 2 : Port_LobbyPoll();
    if (state == 2 && !sTb->found) {
        sTb->found = 1;
        sTb->ended = Port_NetSessionsEnded();
        Snd_PlaySe(1, 1);
        Tb_SetMode(TB_WAIT);
        Port_LobbyLaunch(); /* the session begins at the game's next blank, from the game's first moment */
        return 1;
    }
    if (state <= 0 && !sTb->found) {
        if (state == -3) {
            Tb_Note((char *)Port_LobbyError());
        } else if (state == -4) {
            Tb_Note("THE TWO GAMES COULD NOT REACH EACH OTHER, EVEN THROUGH THE RELAY");
        } else if (state < 0) {
            Tb_Note("THE ROOM COULD NOT BE OPENED");
        }
        Port_LobbyCancel();
        Tb_SetMode(sTb->waitBack == TB_WAIT || sTb->waitBack == TB_SPAR ? TB_HUB : sTb->waitBack);
        return 1;
    }
    return sTb->found;
}

static void Tb_WaitUpdate(s32 live) {
    Pad *pad = &gPad[0];

    if (!sTb->found) {
        if (sTb->host == 1 && Port_LobbyCode()[0] != 0) {
            Ps_Prompt(0, "Copy Code");
        }
        Ps_Prompt(1, "Train While Waiting");
        Ps_Prompt(4, "Cancel");
    }
    if (Tb_SearchNews()) {
        return;
    }
    if (!live || sTb->found) {
        return;
    }
    if ((pad->gamePressed & PADG_CROSS) && sTb->host == 1 && Port_LobbyCode()[0] != 0) { /* the code, to paste to the rival */
        PortTbo_Copy(Port_LobbyCode());
        Snd_PlaySe(1, 1);
        Tb_Note("COPIED: PASTE THE CODE TO YOUR RIVAL");
    } else if (pad->gamePressed & PADG_SQUARE) { /* the fighters of the sparring fight first; the search goes on */
        Snd_PlaySe(1, 1);
        sTb->spCur = 2;
        Tb_SetMode(TB_SPAR);
    } else if (pad->gamePressed & (PADG_CIRCLE | PADG_TRIANGLE)) {
        Snd_PlaySe(1, 2);
        Port_LobbyCancel();
        Tb_SetMode(sTb->waitBack == TB_WAIT ? TB_HUB : sTb->waitBack);
    }
}

/* ---------------------------------------------------------------------------------------------- Train While Waiting */
static char *Tb_Fighter(s32 chara) {
    static char name[TB_FIGHTERS][48];
    static u8 got[TB_FIGHTERS];

    if (chara < 0 || chara >= TB_FIGHTERS) {
        return "-";
    }
    if (!got[chara]) {
        /* (not `?:` between the table's entry and a literal: the 32-bit pointer build read the entry as 8 bytes) */
        if (chara < (s32)(sizeof(sTbNames) / sizeof(sTbNames[0]))) {
            Ps_Copy(name[chara], sTbNames[chara], 48);
        } else {
            Ps_Copy(name[chara], "?", 48);
        }
        got[chara] = 1;
    }
    return name[chara];
}

static void Tb_SparUpdate(s32 live) {
    Pad *pad = &gPad[0];
    s32 *who;

    Ps_Prompt(3, "Change");
    Ps_Prompt(0, sTb->spCur == 2 ? "Start Training" : "Next");
    Ps_Prompt(2, "Back");
    if (Tb_SearchNews()) {
        return;
    }
    if (!live) {
        return;
    }
    Ps_Move(&sTb->spCur, 3);
    who = sTb->spCur == 0 ? &sTb->spMe : sTb->spCur == 1 ? &sTb->spFoe : NULL;
    if (who != NULL) {
        s32 step = Ps_Side();

        if (pad->gameRepeat & PADG_R1) {
            step = 10;
        } else if (pad->gameRepeat & PADG_L1) {
            step = -10;
        }
        if (step != 0) {
            *who = (*who + step + TB_FIGHTERS * 4) % TB_FIGHTERS;
            Snd_PlaySe(1, 0);
        }
    }
    if (pad->gamePressed & (PADG_CROSS | PADG_START)) {
        if (sTb->spCur == 2 || (pad->gamePressed & PADG_START)) { /* the search stays open: net.c polls it from here on */
            Snd_PlaySe(1, 1);
            Port_SettingSave("tbo_spar_me", sTb->spMe);
            Port_SettingSave("tbo_spar_foe", sTb->spFoe);
            Port_SettingsWrite();
            Port_LobbyBackground(1);
            sTbBgKind = sTb->host;
            sTbBgQueue = sTb->queue;
            sTbBgFrames = sTb->frames - sTb->waitFrom;
            Ps_Copy(sTbBgCode, sTb->code, CODE_LEN + 1);
            sTb->train = 1;
            sTb->leaving = 1;
            ColorFade_StartOut(0, 0, 0, 0x14);
        } else {
            Snd_PlaySe(1, 0);
            sTb->spCur++;
        }
    } else if (pad->gamePressed & PADG_TRIANGLE) {
        Snd_PlaySe(1, 2);
        Tb_SetMode(TB_WAIT);
    }
}

/* ---------------------------------------------------------------------------------------------- Quick Match */
static void Tb_QuickStart(void) {
    s32 state;

    if (!Tb_Vanilla()) {
        return;
    }
    Port_NetSpeedSet(Port_NetSpeedStandard());
    Port_NetRulesSet(sTb->queue, 0, 4); /* (a queue's rules: its battle type, no time limit, the standard speed) */
    Port_NetOptions(Port_RollCan() ? 8 : 0, -2);
    Port_NetNameSet(Port_NetNameGet());
    Port_SettingSave("tbo_queue", sTb->queue);
    Port_SettingsWrite();
    state = Port_LobbyStartQuick(sTb->queue);
    if (state < 0) {
        Tb_Note("THE NETWORK COULD NOT BE OPENED ON THIS PC");
        return;
    }
    sTb->host = 2;
    sTb->waitFrom = sTb->frames;
    sTb->found = 0;
    sTb->waitBack = TB_QUICK;
    Tb_SetMode(TB_WAIT);
}

static void Tb_QuickUpdate(s32 live) {
    Pad *pad = &gPad[0];

    Ps_Prompt(3, "Change");
    Ps_Prompt(0, sTb->qkCur == 1 ? "Search" : "Next");
    Ps_Prompt(2, "Back");
    if (sTb->frames - sTb->asked > 1800) { /* how many wait in each queue, asked again every half minute */
        sTb->asked = sTb->frames;
        Port_LobbyQueuesAsk();
    }
    if (!live) {
        return;
    }
    Ps_Move(&sTb->qkCur, 2);
    if (sTb->qkCur == 0 && Ps_Side() != 0) {
        sTb->queue = !sTb->queue;
        Snd_PlaySe(1, 0);
    }
    if (pad->gamePressed & (PADG_CROSS | PADG_START)) {
        if (sTb->qkCur == 1 || (pad->gamePressed & PADG_START)) {
            Snd_PlaySe(1, 1);
            Tb_QuickStart();
        } else {
            Snd_PlaySe(1, 0);
            sTb->qkCur = 1;
        }
    } else if (pad->gamePressed & PADG_TRIANGLE) {
        Snd_PlaySe(1, 2);
        Tb_SetMode(TB_HUB);
    }
}

/* TAG TEAM room setup and live membership. The battle handoff remains gated
 * until the multi-actor loader, cameras and collision pass have been validated. */
static void Tb_TagUpdate(s32 live){
    Pad *pad=&gPad[0];s32 dir;
    Ps_Prompt(3,"Change");Ps_Prompt(0,"Confirm");Ps_Prompt(2,"Back");if(!live)return;
    Ps_Move(&sTb->tagCur,8);dir=Ps_Side();
    if(dir){
        if(sTb->tagCur==0)sTb->tagLeft=Ps_Wrap(sTb->tagLeft+dir,1,10-sTb->tagRight);
        if(sTb->tagCur==1)sTb->tagRight=Ps_Wrap(sTb->tagRight+dir,1,10-sTb->tagLeft);
        if(sTb->tagCur==2)sTb->time=Ps_Wrap(sTb->time+dir,0,4);
        if(sTb->tagCur==3)sTb->tagDelay=Ps_Wrap(sTb->tagDelay+dir,0,12);
        if(sTb->tagCur==4)sTb->tagCPU=!sTb->tagCPU;
        if(sTb->tagCur==5)sTb->tagDifficulty=Ps_Wrap(sTb->tagDifficulty+dir,0,4);
        Snd_PlaySe(1,0);
    }
    if(pad->gamePressed&PADG_TRIANGLE){Tb_SetMode(TB_HUB);return;}
    if(!(pad->gamePressed&PADG_CROSS))return;
    if(sTb->tagCur<6){++sTb->tagCur;return;}
    if(sTb->tagCur==7){sTb->tagJoin=1;sTb->code[0]=0;sTb->codeLen=sTb->keyCol=sTb->keyRow=0;Tb_SetMode(TB_JOIN);return;}
    {unsigned long long content=0;s32 offline=getenv("BT4_RENDER_TEST")&&!getenv("BT4_TAG_ONLINE_TEST");
        if(!offline&&(!Tb_Vanilla()||Port_TagDataFingerprint(&content)!=1))return;
        Port_TagCPUOptions(sTb->tagCPU,sTb->tagDifficulty);
        if(!Port_TagConnect(offline?NULL:Port_TagEndpoint())){Tb_Note((char*)Port_TagError());return;}
        if(!offline)Port_TagInitialTeams(sTb->tagLeft,sTb->tagRight);
        Port_TagOpen(offline?2:5,sTb->time,sTb->tagDelay,Port_NetNameGet(),content);
        if(offline){Port_TagTeams(sTb->tagLeft,sTb->tagRight);if(sTb->tagCPU){s32 i;for(i=1;i<Port_TagSlots();++i)Port_TagSlot(i,1,sTb->tagDifficulty);}}
    }
    if(getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_TEST_EMPTY")){s32 i=atoi(getenv("BT4_TAG_TEST_EMPTY"));Port_TagSlot(i,0,sTb->tagDifficulty);}
    Port_SettingSave("tag_team_delay",sTb->tagDelay);Port_SettingSave("tag_team_left",sTb->tagLeft);Port_SettingSave("tag_team_right",sTb->tagRight);
    Port_SettingSave("tag_team_attacks",sTb->tagAttacks);Port_SettingSave("tag_team_cpu",sTb->tagCPU);
    Port_SettingSave("tag_team_difficulty",sTb->tagDifficulty);Port_SettingsWrite();sTb->tagRoomCur=0;
    Tb_SetMode(TB_TAG_ROOM);
}
static void Tb_TagRoomUpdate(s32 live){
    Pad *pad=&gPad[0];s32 dir,local=!strcmp(Port_TagCode(),"LOCAL0");
    Port_TagPoll();
    if(Port_TagError()[0]&&(Port_TagSeat()<0||Port_TagPhase()==6)){
        char why[128];Ps_Copy(why,(char*)Port_TagError(),128);Port_TagClose();
        Tb_SetMode(sTb->tagJoin?TB_JOIN:TB_TAG);Tb_Note(why);return;
    }
    if(getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_ONLINE_MOVE_TEST")&&Port_TagHost()&&Port_TagPhase()==1&&Port_TagHumans()==2){
        static s32 moved;if(!moved){Port_TagMovePlayer(0,2);moved=1;return;}if(Port_TagSeat()!=2)return;
    }
    if(getenv("BT4_TAG_DRAFT_TEST")&&(!getenv("BT4_TAG_ONLINE_TEST")||Port_TagHumans()>=(getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_TEST_MIN_HUMANS")?atoi(getenv("BT4_TAG_TEST_MIN_HUMANS")):2))&&!getenv("BT4_TAG_VIDEO_TEST")&&!getenv("BT4_TAG_LOBBY_PREVIEW")&&!getenv("BT4_TAG_LOBBY_TEST")&&!sTb->tagTestSent&&Port_TagHost()&&Port_TagCanStart()){
        Port_TagCommand(3,0,0);sTb->tagTestSent=1;}
    Ps_Prompt(0,Port_TagHost()?"Select":"Copy");
    if(!local)Ps_Prompt(4,"Chat");Ps_Prompt(2,"Back");
    if(Port_TagHost()&&Port_TagCanStart())Ps_Prompt(1,"Start");
    if(!live)return;
    if(getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_LOBBY_TEST")&&Port_TagPhase()==1){
        static s32 tick;++tick;
        if(tick==20)pad->gameRepeat=PADG_DOWN;
        if(tick==40)pad->gamePressed=PADG_CROSS;
        if(tick==60)pad->gameRepeat=PADG_RIGHT;
        if(tick==80)pad->gamePressed=PADG_R1;
        if(tick==100){fprintf(stderr,"tag lobby fixture: count=%d slot2cpu=%d slot3difficulty=%d selected=%d\n",Port_TagCount(),Port_TagCPU(2),Port_TagDifficulty(3),sTb->tagRoomCur);pad->gamePressed=PADG_SQUARE;}
    }
    if(Port_TagPhase()==2&&Port_TagSeat()>=0&&!Port_TagDraftPending()){sTb->train=2;sTb->leaving=1;ColorFade_StartOut(0,0,0,0x14);return;}
    if(!local&&(pad->gamePressed&PADG_CIRCLE)&&!sTb->tagChatTyping){sTb->tagChatTyping=1;sTb->tagChatLen=0;sTb->tagChat[0]=0;Port_TextMode(1);return;}
    if(sTb->tagChatTyping){s32 c;Ps_PromptsClear();Ps_Prompt(2,"Close");while((c=Port_TextRead())!=0){
        if(c==13){Port_TagChatSend(sTb->tagChat);sTb->tagChatTyping=0;Port_TextMode(0);}
        else if(c==27){sTb->tagChatTyping=0;Port_TextMode(0);}
        else if(c==8){if(sTb->tagChatLen)sTb->tagChat[--sTb->tagChatLen]=0;}
        else if(c>=32&&c<127&&sTb->tagChatLen<150){sTb->tagChat[sTb->tagChatLen++]=c;sTb->tagChat[sTb->tagChatLen]=0;}}
        if(pad->gamePressed&PADG_TRIANGLE){sTb->tagChatTyping=0;Port_TextMode(0);}return;
    }
    if(Port_TagHost()){
        s32 old=sTb->tagRoomCur,team=Port_TagTeam(old),row=Port_TagTeamIndex(old),i,next=old;
        if(pad->gameRepeat&(PADG_UP|PADG_DOWN)){
            s32 rows=Port_TagTeamSlots(team),wanted=Ps_Wrap(row+((pad->gameRepeat&PADG_UP)?-1:1),0,rows-1);
            for(i=0;i<Port_TagSlots();++i)if(Port_TagTeam(i)==team&&Port_TagTeamIndex(i)==wanted)next=i;
        }
        if(pad->gameRepeat&(PADG_LEFT|PADG_RIGHT)){
            s32 rows=Port_TagTeamSlots(1-team),wanted=row<rows?row:rows-1;
            for(i=0;i<Port_TagSlots();++i)if(Port_TagTeam(i)!=team&&Port_TagTeamIndex(i)==wanted)next=i;
        }
        sTb->tagRoomCur=next;if(next!=old)Snd_PlaySe(1,0);
        if(pad->gamePressed&PADG_CROSS){
            if(sTb->tagMoving>=0){if(Port_TagMovePlayer(sTb->tagMoving,next))Snd_PlaySe(1,1);sTb->tagMoving=-1;}
            else if(Port_TagPlayerName(next)[0]&&!Port_TagCPU(next)){sTb->tagMoving=next;Tb_Note("CHOOSE A DESTINATION SLOT. TRIANGLE CANCELS.");}
            else if(Port_TagSlot(next,!Port_TagCPU(next),sTb->tagDifficulty))Snd_PlaySe(1,1);
        }
        dir=(pad->gamePressed&PADG_R1)?1:(pad->gamePressed&PADG_L1)?-1:0;
        if(dir&&Port_TagCPU(next)){Port_TagSlot(next,1,Ps_Wrap(Port_TagDifficulty(next)+dir,0,4));Snd_PlaySe(1,0);}
    }else if(pad->gamePressed&PADG_CROSS){if(Port_TagCopyCode())Tb_Note("ROOM CODE COPIED.");}
    if(!local&&(pad->gamePressed&PADG_SELECT)){if(Port_TagCopyCode())Tb_Note("ROOM CODE COPIED.");}
    if(Port_TagError()[0])Tb_Note((char*)Port_TagError());
    if((pad->gamePressed&(PADG_SQUARE|PADG_START))&&Port_TagHost()){
        if(Port_TagCanStart())Port_TagCommand(3,0,0);else Tb_Note("EACH TEAM NEEDS AT LEAST ONE FIGHTER.");}
    if(pad->gamePressed&PADG_TRIANGLE){if(sTb->tagMoving>=0){sTb->tagMoving=-1;return;}Port_TagClose();Tb_SetMode(TB_TAG);}
}
/* ---------------------------------------------------------------------------------------------- the hub */
static void Tb_HubUpdate(s32 live) {
    Pad *pad = &gPad[0];

    Ps_Prompt(0, "Enter");
    Ps_Prompt(2, "Back");
    if (!live) {
        return;
    }
    Ps_Move(&sTb->hubCur, HUB_COUNT);
    if (pad->gamePressed & PADG_CROSS) {
        switch (sTb->hubCur) {
        case HUB_QUICK:
            Snd_PlaySe(1, 1);
            sTb->qkCur = 1;
            sTb->asked = sTb->frames - 100000;
            Tb_SetMode(TB_QUICK);
            break;
        case HUB_CREATE:
            Snd_PlaySe(1, 1);
            sTb->crCur = CR_OPEN;
            Tb_SetMode(TB_CREATE);
            break;
        case HUB_JOIN:
            sTb->tagJoin = 0;
            Snd_PlaySe(1, 1);
            sTb->code[0] = 0;
            sTb->codeLen = 0;
            sTb->keyCol = 0;
            sTb->keyRow = 0;
            Tb_SetMode(TB_JOIN);
            break;
        case HUB_TAG:
            sTb->tagCur = 0; sTb->tagJoin = 0;
            Snd_PlaySe(1, 1); Tb_SetMode(TB_TAG);
            break;
        case HUB_PROFILE:
            Snd_PlaySe(1, 1);
            sTb->prCur = 0;
            Tb_SetMode(TB_PROFILE);
            break;
        default:
            Snd_PlaySe(1, 2);
            Tb_Note("NOT OPEN YET: IT NEEDS MORE FROM THE MATCHMAKING SERVICE THAN ROOM CODES");
            break;
        }
    } else if (pad->gamePressed & PADG_TRIANGLE) {
        Snd_PlaySe(1, 2);
        sTb->leaving = 1;
        ColorFade_StartOut(0, 0, 0, 0x14);
    }
}

/* ---------------------------------------------------------------------------------------------- Create Room */
static void Tb_CreateUpdate(s32 live) {
    Pad *pad = &gPad[0];
    s32 side;

    Ps_Prompt(3, "Change");
    Ps_Prompt(0, sTb->crCur == CR_OPEN ? "Open Room" : "Next");
    Ps_Prompt(2, "Back");
    if (!live) {
        return;
    }
    Ps_Move(&sTb->crCur, CR_COUNT);
    side = Ps_Side();
    if (side != 0) {
        switch (sTb->crCur) {
        case CR_BATTLE:
            sTb->battle = !sTb->battle;
            break;
        case CR_TIME:
            sTb->time = Ps_Wrap(sTb->time + side, 0, 4);
            break;
        case CR_DELAY:
            sTb->delay = Ps_Wrap(sTb->delay + side, -1, 6);
            break;
        case CR_ROLL:
            if (Port_RollCan()) {
                sTb->roll = Ps_Wrap(sTb->roll + side * 2, 0, 8);
            }
            break;
        }
        if (sTb->crCur != CR_OPEN) {
            Snd_PlaySe(1, 0);
        }
    }
    if (pad->gamePressed & (PADG_CROSS | PADG_START)) {
        if (sTb->crCur == CR_OPEN || (pad->gamePressed & PADG_START)) {
            Snd_PlaySe(1, 1);
            Tb_BeginSearch(1);
        } else {
            Snd_PlaySe(1, 0);
            sTb->crCur++;
        }
    } else if (pad->gamePressed & PADG_TRIANGLE) {
        Snd_PlaySe(1, 2);
        Tb_SaveChoices();
        Tb_SetMode(TB_HUB);
    }
}

/* ---------------------------------------------------------------------------------------------- the keypads */
/* A pad keypad: rows of nine keys and, under them, a row of wide buttons, each over a run of the columns. Up / down
   / left / right move, cross presses; circle switches the PC keyboard to typing and back. */
typedef struct TbKeyBtn {
    char *label;
    s32 c0;
    s32 c1;
    s32 act; /* TB_KEY_ */
} TbKeyBtn;
#define TB_KEY_DELETE 1
#define TB_KEY_OK 2
#define TB_KEY_CASE 3
#define TB_KEY_SPACE 4
#define TB_KEY_PASTE 5

static const TbKeyBtn sTagJoinBtn[3]={{"DELETE",0,2,TB_KEY_DELETE},{"PASTE",3,5,TB_KEY_PASTE},{"JOIN",6,8,TB_KEY_OK}};
static const TbKeyBtn sTbJoinBtn[3] = {{"DELETE", 0, 2, TB_KEY_DELETE}, {"PASTE", 3, 5, TB_KEY_PASTE}, {"JOIN", 6, 8, TB_KEY_OK}};
static const TbKeyBtn sTbNameBtn[4] = {{"Aa", 0, 1, TB_KEY_CASE}, {"SPACE", 2, 4, TB_KEY_SPACE}, {"DELETE", 5, 6, TB_KEY_DELETE}, {"DONE", 7, 8, TB_KEY_OK}};
#define NAME_ROWS 5
static char sTbNameKeys[2][NAME_ROWS][KEY_COLS + 1] = {{"ABCDEFGHI", "JKLMNOPQR", "STUVWXYZ0", "123456789", "-_.!?#&*+"},
                                                     {"abcdefghi", "jklmnopqr", "stuvwxyz0", "123456789", "-_.!?#&*+"}};

static s32 Tb_BtnAt(const TbKeyBtn *b, s32 nb, s32 col) {
    s32 k;

    for (k = 0; k < nb; k++) {
        if (col >= b[k].c0 && col <= b[k].c1) {
            return k;
        }
    }
    return 0;
}

/* The cursor's moves. Returns 0, a key's letter, or -(TB_KEY_) for a button, when cross is pressed. */
static s32 Tb_PadUpdate(s32 rows, char (*keys)[KEY_COLS + 1], const TbKeyBtn *b, s32 nb) {
    Pad *pad = &gPad[0];

    if (sTb->keyRow == rows) { /* the buttons' row: a step goes to the next button */
        s32 k = Tb_BtnAt(b, nb, sTb->keyCol);

        if (pad->gameRepeat & (PADG_RIGHT | PADG_RIGHT2)) {
            sTb->keyCol = b[(k + 1) % nb].c0;
            Snd_PlaySe(1, 0);
        } else if (pad->gameRepeat & (PADG_LEFT | PADG_LEFT2)) {
            sTb->keyCol = b[(k + nb - 1) % nb].c0;
            Snd_PlaySe(1, 0);
        }
    } else if (pad->gameRepeat & (PADG_RIGHT | PADG_RIGHT2)) {
        sTb->keyCol = (sTb->keyCol + 1) % KEY_COLS;
        Snd_PlaySe(1, 0);
    } else if (pad->gameRepeat & (PADG_LEFT | PADG_LEFT2)) {
        sTb->keyCol = (sTb->keyCol + KEY_COLS - 1) % KEY_COLS;
        Snd_PlaySe(1, 0);
    }
    if (pad->gameRepeat & (PADG_DOWN | PADG_DOWN2)) {
        sTb->keyRow = (sTb->keyRow + 1) % (rows + 1);
        Snd_PlaySe(1, 0);
    } else if (pad->gameRepeat & (PADG_UP | PADG_UP2)) {
        sTb->keyRow = (sTb->keyRow + rows) % (rows + 1);
        Snd_PlaySe(1, 0);
    }
    if (sTb->keyRow == rows) {
        sTb->keyCol = b[Tb_BtnAt(b, nb, sTb->keyCol)].c0;
    }
    if (pad->gamePressed & PADG_CROSS) {
        return sTb->keyRow < rows ? (u8)keys[sTb->keyRow][sTb->keyCol] : -b[Tb_BtnAt(b, nb, sTb->keyCol)].act;
    }
    return 0;
}

/* Draws a keypad at (x, y): its keys, the cursor and the buttons' row. */
static void Tb_Keypad(f32 x, f32 y, s32 rows, char (*keys)[KEY_COLS + 1], const TbKeyBtn *b, s32 nb) {
    char one[2] = {0, 0};
    s32 r;
    s32 c;
    s32 k;
    f32 by = y + 34.0f * (f32)rows + 4.0f;

    for (r = 0; r < rows; r++) {
        for (c = 0; c < KEY_COLS; c++) {
            f32 kx = x + 36.0f * (f32)c;
            f32 ky = y + 34.0f * (f32)r;
            s32 on = r == sTb->keyRow && c == sTb->keyCol;

            if (on) {
                Ps_Pill(TBO_KIT_SEL, kx - 2.0f, ky - 2.0f, kx + 34.0f, ky + 30.0f, 0x80);
            } else {
                Ps_Pill(TBO_KIT_BAR, kx, ky, kx + 32.0f, ky + 28.0f, 0x80);
            }
            one[0] = keys[r][c];
            Ps_Align(1);
            Ps_Text(kx + 16.0f, Ps_TextTop(ky + 14.0f, 0.72f), one, 0.72f, 0.0f, on ? 1 : 0);
            Ps_Align(0);
        }
    }
    for (k = 0; k < nb; k++) {
        f32 x0 = x + 36.0f * (f32)b[k].c0;
        f32 x1 = x + 36.0f * (f32)b[k].c1 + 32.0f;
        s32 on = sTb->keyRow == rows && sTb->keyCol >= b[k].c0 && sTb->keyCol <= b[k].c1;

        /* (JOIN and DONE as the others: gold is the one the cursor is on) */
        Ps_Pill(on ? TBO_KIT_SEL : TBO_KIT_BAR, x0, by, x1, by + 28.0f, 0x80);
        Ps_Align(1);
        Ps_Text((x0 + x1) * 0.5f, Ps_TextTop(by + 14.0f, 0.6f), b[k].label, 0.6f, x1 - x0 - 12.0f, on ? 1 : 0);
        Ps_Align(0);
    }
}

/* ---------------------------------------------------------------------------------------------- Join Room */
static void Tb_CodeAdd(s32 c) {
    if (c >= 'a' && c <= 'z') {
        c -= 'a' - 'A';
    }
    if (((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) && sTb->codeLen < CODE_LEN) {
        sTb->code[sTb->codeLen++] = (char)c;
        sTb->code[sTb->codeLen] = 0;
        Snd_PlaySe(1, 0);
    }
}

static void Tb_CodeBack(void) {
    if (sTb->codeLen > 0) {
        sTb->code[--sTb->codeLen] = 0;
        Snd_PlaySe(1, 2);
    }
}

static void Tb_JoinTry(void) {
    if (sTb->codeLen == CODE_LEN) {
        Snd_PlaySe(1, 1);
        if (sTb->tagJoin) {
            if (!Port_TagJoinCode(sTb->code)) { Tb_Note((char *)Port_TagError()); return; }
            sTb->tagRoomCur=0;sTb->tagMoving=-1;sTb->tagChatTyping=0;
            Tb_SetMode(TB_TAG_ROOM);
        } else Tb_BeginSearch(0);
    } else {
        Snd_PlaySe(1, 2);
        Tb_Note(sTb->tagJoin?"ROOM CODES HAVE SIX CHARACTERS.":"A ROOM CODE HAS SIX LETTERS AND NUMBERS");
    }
}

static void Tb_JoinUpdate(s32 live) {
    Pad *pad = &gPad[0];
    s32 c;

    Ps_Prompt(0, sTb->tagJoin?"Confirm":"Enter");
    Ps_Prompt(1, sTb->tagJoin?"Delete":"Delete");
    Ps_Prompt(2, sTb->tagJoin?"Back":"Back");
    if (!live) {
        return;
    }
    while (sTb->typing && (c = Port_TextRead()) != 0) { /* the PC keyboard: letters, backspace, enter, escape */
        if (c == 13) {
            Tb_JoinTry();
            return;
        } else if (c == 27) { /* Esc: back, as triangle */
            Snd_PlaySe(1, 2);
            Tb_SetMode(sTb->tagJoin ? TB_TAG : TB_HUB);
            return;
        } else if (c == 8) {
            Tb_CodeBack();
        } else {
            Tb_CodeAdd(c);
            if (sTb->codeLen == CODE_LEN) { /* full (typed or pasted): the cursor goes to JOIN */
                sTb->keyRow = KEY_ROWS;
                sTb->keyCol = sTbJoinBtn[2].c0;
            }
        }
    }
    c = Tb_PadUpdate(KEY_ROWS, sTbKeys, sTbJoinBtn, 3);
    if (c > 0) {
        Tb_CodeAdd(c);
        if (sTb->codeLen == CODE_LEN) { /* full: the cursor goes to JOIN */
            sTb->keyRow = KEY_ROWS;
            sTb->keyCol = sTbJoinBtn[2].c0;
        }
    } else if (c == -TB_KEY_PASTE) { /* the copied code in place of what is typed (it comes with the next frames) */
        sTb->codeLen = 0;
        sTb->code[0] = 0;
        Snd_PlaySe(1, 1);
        PortTbo_Paste();
    } else if (c == -TB_KEY_DELETE) {
        Tb_CodeBack();
    } else if (c == -TB_KEY_OK || (pad->gamePressed & PADG_START)) {
        Tb_JoinTry();
    } else if (pad->gamePressed & PADG_SQUARE) {
        Tb_CodeBack();
    } else if (pad->gamePressed & PADG_TRIANGLE) {
        Snd_PlaySe(1, 2);
        Tb_SetMode(sTb->tagJoin ? TB_TAG : TB_HUB);
    }
}

/* ---------------------------------------------------------------------------------------------- Profile */
static void Tb_ProfileUpdate(s32 live) {
    Pad *pad = &gPad[0];

    Ps_Prompt(0, "Type Name");
    Ps_Prompt(2, "Back");
    if (!live) {
        return;
    }
    Ps_Move(&sTb->prCur, PR_COUNT);
    if ((pad->gamePressed & PADG_CROSS) && sTb->prCur == PR_NAME) {
        Snd_PlaySe(1, 1);
        Ps_Copy(sTb->typed, Port_NetNameGet(), 20);
        sTb->typedLen = Ps_Len(sTb->typed);
        Tb_SetMode(TB_NAME);
        sTb->keyRow = 0;
        sTb->keyCol = 0;
    } else if (pad->gamePressed & PADG_TRIANGLE) {
        Snd_PlaySe(1, 2);
        Tb_SaveChoices();
        Tb_SetMode(TB_HUB);
    }
}

/* Whether a character is one of the name keypad's (or a space): what the keyboard types into a name too, so a name
   has only letters the game's font has, and the two ways of typing give the same names. */
static s32 Tb_NameKey(s32 c) {
    s32 r;
    s32 k;

    if (c == ' ') {
        return 1;
    }
    for (r = 0; r < NAME_ROWS; r++) {
        for (k = 0; k < KEY_COLS; k++) {
            if (sTbNameKeys[0][r][k] == c || sTbNameKeys[1][r][k] == c) {
                return 1;
            }
        }
    }
    return 0;
}

static void Tb_NameAdd(s32 c) {
    if (Tb_NameKey(c) && sTb->typedLen < 16) {
        sTb->typed[sTb->typedLen++] = (char)c;
        sTb->typed[sTb->typedLen] = 0;
        Snd_PlaySe(1, 0);
    }
}

static void Tb_NameBack(void) {
    if (sTb->typedLen > 0) {
        sTb->typed[--sTb->typedLen] = 0;
        Snd_PlaySe(1, 2);
    }
}

static void Tb_NameUpdate(s32 live) {
    Pad *pad = &gPad[0];
    s32 c;
    s32 done = 0;

    Ps_Prompt(0, "Enter");
    Ps_Prompt(1, "Delete");
    Ps_Prompt(2, "Cancel");
    if (!live) {
        return;
    }
    while (sTb->typing && (c = Port_TextRead()) != 0) {
        if (c == 13) {
            done = 1;
        } else if (c == 27) { /* Esc: cancel, as triangle */
            done = -1;
            break;
        } else if (c == 8) {
            Tb_NameBack();
        } else {
            Tb_NameAdd(c);
        }
    }
    if (!done) {
        c = Tb_PadUpdate(NAME_ROWS, sTbNameKeys[sTb->lower], sTbNameBtn, 4);
        if (c > 0) {
            Tb_NameAdd(c);
        } else if (c == -TB_KEY_SPACE) {
            Tb_NameAdd(' ');
        } else if (c == -TB_KEY_CASE) {
            sTb->lower = !sTb->lower;
            Snd_PlaySe(1, 0);
        } else if (c == -TB_KEY_DELETE || (pad->gamePressed & PADG_SQUARE)) {
            Tb_NameBack();
        } else if (c == -TB_KEY_OK || (pad->gamePressed & PADG_START)) {
            done = 1;
        } else if (pad->gamePressed & PADG_TRIANGLE) {
            done = -1;
        }
    }
    if (done == 1) {
        while (sTb->typedLen > 0 && sTb->typed[sTb->typedLen - 1] == ' ') {
            sTb->typed[--sTb->typedLen] = 0;
        }
        if (sTb->typedLen > 0) {
            Port_NetNameSave(sTb->typed);
            Tb_Note("SAVED: YOUR RIVALS WILL SEE THIS NAME");
        }
        Snd_PlaySe(1, 1);
        Tb_SetMode(TB_PROFILE);
    } else if (done == -1) {
        Snd_PlaySe(1, 2);
        Tb_SetMode(TB_PROFILE);
    }
}

/* ---------------------------------------------------------------------------------------------- what is drawn */
/* ---------------------------------------------------------------------------------------------- the pictures */
/* (how a screen draws: menu/port_screen.c) */
#define TB_BAR_LEFT -30.0f /* the hub's bars start here (off the page: on a 4:3 screen, off its edge) */


/* The left of the crest and the title: in line with the bars, but on the screen. */
static f32 Tb_TitleLeft(void) {
    f32 l = Ps_Left() + 6.0f;

    return l > TB_BAR_LEFT ? l : TB_BAR_LEFT;
}

/* The tournament grounds over the whole window: the middle of the 16:9 picture, as wide as the window is. */
static void Tb_Backdrop(void) {
    f32 aspect = (f32)Port_PageAspectMilli() / 1000.0f;
    f32 pic = Ps_PicW(TBO_BG) / Ps_PicH(TBO_BG);
    f32 u = aspect < pic ? aspect / pic : 1.0f;
    f32 v = aspect < pic ? 1.0f : pic / aspect;

    Ps_SpriteScreen(TBO_BG, 0.0f, 0.0f, 512.0f, 448.0f, 0.5f - u * 0.5f, 0.5f - v * 0.5f, 0.5f + u * 0.5f, 0.5f + v * 0.5f, PS_PLAIN, 0x80);
}

/* The host in a pose (TBO_HOSTS_*), standing on `feet` around `cx`, `height` units tall; they breathe. The ones that
   stand on the message box's line are drawn half behind it (the box is drawn over them, and is see-through): their
   pictures run down to the foot of the screen. */
#define TB_HOSTS_BOX_TOP 326.0f /* the top edge of the message box (the game's, MsgWin) */
#define TB_HOSTS_SCALE 1.2f     /* those half behind the box, larger than the pages ask */
static void Tb_Hosts(s32 pose, f32 cx, f32 feet, f32 height) {
    if (feet >= 300.0f) { /* (on the box's line) half the picture behind the box: its middle on the box's top edge */
        height *= TB_HOSTS_SCALE;
        feet = TB_HOSTS_BOX_TOP + height * 0.5f;
    }
    Ps_Stand(pose, cx, feet + 2.0f * Tb_Beat(90) - 1.0f, height, 0x80);
}

/* The Dragon Radar of the search: its glass, its grid, the sweeping line, a few dragon balls blinking. */
static void Tb_Radar(f32 cx, f32 cy, f32 r) {
    f32 rx = r * 0.857f;
    f32 sweep = (f32)sTb->frames * 0.06f;
    s32 k;

    Ps_Disc(cx, cy, r + 10.0f, Ps_Rgba(0xD8, 0xDC, 0xE4, 0x80), Ps_Rgba(0x70, 0x78, 0x88, 0x80)); /* the casing */
    Ps_Disc(cx, cy, r + 3.0f, Ps_Rgba(0x20, 0x30, 0x28, 0x80), Ps_Rgba(0x10, 0x18, 0x14, 0x80));
    Ps_Disc(cx, cy, r, Ps_Rgba(0x40, 0xC8, 0x60, 0x80), Ps_Rgba(0x10, 0x60, 0x28, 0x80));    /* the glass */
    for (k = -3; k <= 3; k++) {                                                                  /* the grid */
        f32 t = (f32)k / 3.5f;
        f32 half = sqrtf(1.0f - t * t);

        Ps_Rect(cx + t * rx - 0.6f, cy - half * r, cx + t * rx + 0.6f, cy + half * r, Ps_Rgba(0x90, 0xFF, 0xA0, 0x50), Ps_Rgba(0x90, 0xFF, 0xA0, 0x50));
        Ps_Rect(cx - half * rx, cy + t * r - 0.6f, cx + half * rx, cy + t * r + 0.6f, Ps_Rgba(0x90, 0xFF, 0xA0, 0x50), Ps_Rgba(0x90, 0xFF, 0xA0, 0x50));
    }
    for (k = 0; k < 10; k++) { /* the sweep: a wedge that fades behind the line */
        f32 a0 = sweep - (f32)(k + 1) * 0.07f;
        f32 a1 = sweep - (f32)k * 0.07f;
        s32 al = 0x60 - k * 6;

        Ps_Quad(cx, cy, cx, cy, cx + cosf(a0) * rx, cy + sinf(a0) * r, cx + cosf(a1) * rx, cy + sinf(a1) * r, Ps_Rgba(0xD0, 0xFF, 0xC0, al),
                Ps_Rgba(0xD0, 0xFF, 0xC0, al), Ps_Rgba(0xD0, 0xFF, 0xC0, al / 2), Ps_Rgba(0xD0, 0xFF, 0xC0, al / 2));
    }
    {
        static const f32 ball[5][2] = {{0.35f, -0.4f}, {-0.5f, 0.2f}, {0.15f, 0.55f}, {-0.2f, -0.65f}, {0.62f, 0.25f}};

        for (k = 0; k < 5; k++) {
            f32 bx = cx + ball[k][0] * rx;
            f32 by = cy + ball[k][1] * r;
            s32 al = ((sTb->frames + k * 17) / 20) % 3 == 0 ? 0x40 : 0x80;

            Ps_Disc(bx, by, 4.0f, Ps_Rgba(0xFF, 0xF8, 0x80, al), Ps_Rgba(0xFF, 0xA0, 0x20, al));
        }
    }
    Ps_Disc(cx, cy, 3.0f, Ps_Rgba(0xFF, 0x50, 0x40, 0x80), Ps_Rgba(0xA0, 0x10, 0x10, 0x80)); /* you */
}

static f32 Tb_HubRowY(s32 i) {
    return 68.0f + 24.5f * (f32)i; /* the last bar (under the cursor: to +27) ends above the box (y 326) */
}

static f32 Tb_HubRowEnd(s32 i) {
    return 270.0f - 7.0f * (f32)i; /* the bars cascade: each a little shorter than the one above */
}

static f32 Tb_HubBadgeWidth(void){
    f32 width=Ps_TextWidth("SOON",0.34f)+12.0f;
    return width<40?40:width>90?90:width;
}
static void Tb_DrawHub(void) {
    s32 i;
    f32 slide = (1.0f - Tb_In(0)) * -160.0f;
    f32 target = Tb_HubRowY(sTb->hubCur);

    Tb_Hosts(TBO_HOSTS_MIC_WAVE, 396.0f - slide * 0.6f, 350.0f, 262.0f);
    sTb->hubY += (target - sTb->hubY) * 0.35f;
    for (i = 0; i < HUB_COUNT; i++) {
        f32 y = Tb_HubRowY(i);
        f32 x1 = Tb_HubRowEnd(i) + slide;

        if (i != sTb->hubCur) {
            Ps_Pill(TBO_KIT_BAR, TB_BAR_LEFT + slide, y, x1, y + 24.0f, sTbHubReady[i] ? 0x80 : 0x60);
        }
    }
    Ps_Pill(TBO_KIT_SEL, TB_BAR_LEFT + slide, sTb->hubY - 3.0f, Tb_HubRowEnd(sTb->hubCur) + 22.0f + slide, sTb->hubY + 27.0f, 0x80);
    for (i = 0; i < HUB_COUNT; i++) {
        if (!sTbHubReady[i]) {
            Ps_Pill(TBO_KIT_STEEL, Tb_HubRowEnd(i) - 6.0f - Tb_HubBadgeWidth() + slide + (i == sTb->hubCur ? 18.0f : 0.0f), Tb_HubRowY(i) + 5.0f,
                    Tb_HubRowEnd(i) - 6.0f + slide + (i == sTb->hubCur ? 18.0f : 0.0f), Tb_HubRowY(i) + 19.0f, 0x80);
        }
    }
}

static void Tb_DrawHubText(void) {
    s32 i;
    f32 slide = (1.0f - Tb_In(0)) * -160.0f;

    for (i = 0; i < HUB_COUNT; i++) {
        f32 y = Tb_HubRowY(i);

        /* each label's capitals in the middle of its bar (the bar under the cursor: hubY - 3 to hubY + 27) */
        if (i == sTb->hubCur) {
            Ps_Text(36.0f + slide, Ps_TextTop(sTb->hubY + 12.0f, 0.84f), sTbHubName[i], 0.84f, Tb_HubRowEnd(i)-42.0f-(sTbHubReady[i]?0:Tb_HubBadgeWidth()+8), 1);
        } else {
            Ps_Text(36.0f + slide, Ps_TextTop(y + 12.0f, 0.7f), sTbHubName[i], 0.7f, Tb_HubRowEnd(i)-42.0f-(sTbHubReady[i]?0:Tb_HubBadgeWidth()+8), sTbHubReady[i] ? 0 : 4);
        }
        if (!sTbHubReady[i]) {
            Ps_Align(1);
            Ps_Text(Tb_HubRowEnd(i) - 6.0f - Tb_HubBadgeWidth()*0.5f + slide + (i == sTb->hubCur ? 18.0f : 0.0f), Ps_TextTop(Tb_HubRowY(i) + 12.0f, 0.34f), "SOON", 0.34f,
                    Tb_HubBadgeWidth()-10.0f, 4);
            Ps_Align(0);
        }
    }
}

/* rows of label / value, as Create Room and Profile have them */
static void Tb_DrawRows(s32 count, s32 cur, f32 y0) {
    s32 i;

    Ps_Panel(TBO_KIT_WINDOW, 14.0f, y0 - 14.0f, 300.0f, y0 + 34.0f * (f32)count + 6.0f, 0x80);
    for (i = 0; i < count; i++) {
        f32 y = y0 + 34.0f * (f32)i;

        if (i == cur) {
            Ps_Pill(TBO_KIT_SEL, 20.0f, y - 2.0f, 294.0f, y + 28.0f, 0x80);
            Ps_Button(3, 278.0f, y + 13.0f, 20.0f);
        } else {
            Ps_Pill(TBO_KIT_BAR, 24.0f, y, 290.0f, y + 26.0f, 0x80);
        }
    }
}

static void Tb_DrawCreate(void) {
    Tb_Hosts(TBO_HOSTS_ARMS_CROSSED, 408.0f, 350.0f, 250.0f);
    Tb_DrawRows(CR_COUNT, sTb->crCur, 100.0f);
}

static void Tb_DrawCreateText(void) {
    static char value[CR_COUNT][32];
    static char *label[CR_COUNT] = {"Battle", "Time Limit", "Input Delay", "Rollback", "Open Room"};
    s32 i;

    sprintf(value[CR_BATTLE], sTb->battle ? "Teams (up to 5)" : "1 vs 1");
    Ps_Copy(value[CR_TIME], sTbTime[sTb->time], 32);
    Tb_DelayText(value[CR_DELAY], sTb->delay);
    Tb_RollText(value[CR_ROLL], sTb->roll);
    value[CR_OPEN][0] = 0;
    Ps_Align(1);
    Ps_Text(157.0f, Ps_TextTop(80.0f, 0.56f), "ROOM SETTINGS", 0.56f, 0.0f, 2);
    Ps_Align(0);
    for (i = 0; i < CR_COUNT; i++) {
        f32 y = 100.0f + 34.0f * (f32)i;

        Ps_Text(36.0f, Ps_TextTop(y + 13.0f, 0.66f), label[i], 0.66f, 120.0f, i == sTb->crCur ? 1 : 0);
        Ps_Align(2);
        Ps_Text(266.0f, Ps_TextTop(y + 13.0f, 0.58f), value[i], 0.58f, 124.0f, 3);
        Ps_Align(0);
    }
}

static void Tb_DrawTag(void){
    s32 i;Tb_Hosts(TBO_HOSTS_ARMS_CROSSED,408,350,250);Ps_Panel(TBO_KIT_WINDOW,14,78,300,321,0x80);
    for(i=0;i<8;++i){f32 y=84+29.0f*i;Ps_Pill(i==sTb->tagCur?TBO_KIT_SEL:TBO_KIT_BAR,22,y,292,y+23,0x80);}
}
static void Tb_DrawTagText(void){
    char value[48];s32 i;
    static char *labels[8]={"Team 1","Team 2","Time Limit","Input Delay","Empty Slots","CPU Difficulty","Create Room","Join Room"};
    Ps_Text(26,65,"SIMULTANEOUS TEAM BATTLES",.47f,274,2);
    for(i=0;i<8;++i){f32 y=84+29.0f*i;
        Ps_Text(36,Ps_TextTop(y+11,.56f),labels[i],.56f,126,i==sTb->tagCur?1:0);value[0]=0;
        if(i==0)sprintf(value,sTb->tagLeft==1?(char *)PortOnline_Text("%d slot"):(char *)PortOnline_Text("%d slots"),sTb->tagLeft);if(i==1)sprintf(value,sTb->tagRight==1?(char *)PortOnline_Text("%d slot"):(char *)PortOnline_Text("%d slots"),sTb->tagRight);
        if(i==2)Ps_Copy(value,sTagTime[sTb->time],48);if(i==3)sprintf(value,(char *)PortOnline_Text("%d frames"),sTb->tagDelay);
        if(i==4)Ps_Copy(value,sTb->tagCPU?"CPU":"Open",48);if(i==5)Ps_Copy(value,sTagDifficulty[sTb->tagDifficulty],48);
        Ps_Align(2);Ps_Text(278,Ps_TextTop(y+11,.51f),value,.51f,104,3);Ps_Align(0);
    }
}
static void Tb_DrawTagRoom(void){
    s32 i;
    if(sTb->tagChatTyping){Ps_Panel(TBO_KIT_WINDOW,28,94,484,322,0x80);Ps_Pill(TBO_KIT_BAR,40,276,472,305,0x80);return;}
    Ps_Panel(TBO_KIT_WINDOW,14,82,249,321,0x80);Ps_Panel(TBO_KIT_WINDOW,263,82,498,321,0x80);
    for(i=0;i<Port_TagSlots();++i){f32 x=Port_TagTeam(i)?271:22,y=120+21.0f*Port_TagTeamIndex(i);
        Ps_Pill(i==sTb->tagRoomCur?TBO_KIT_SEL:TBO_KIT_BAR,x,y,x+218,y+19,0x80);}
}
static void Tb_DrawTagRoomText(void){
    char line[128];s32 i;
    sprintf(line,(char *)PortOnline_Text("ROOM %s  |  %d/%d FIGHTERS"),Port_TagCode()[0]?Port_TagCode():"......",Port_TagCount(),Port_TagSlots());
    Ps_Align(1);Ps_Text(256,66,line,.56f,460,3);Ps_Align(0);
    if(sTb->tagChatTyping){
        Ps_Text(44,107,"ROOM CHAT",.65f,410,3);
        for(i=0;i<Port_TagChatCount();++i)Ps_TextRaw(44,141+i*20,(char*)Port_TagChatLine(i),.43f,420,0);
        if(sTb->tagChatLen)Ps_TextRaw(48,284,sTb->tagChat,.46f,410,0);else Ps_Text(48,284,"Type a message...",.46f,410,4);
        Ps_Text(44,309,"ENTER: SEND   |   ESC: CLOSE",.35f,410,3);return;
    }
    Ps_Text(28,90,"TEAM 1",.64f,200,3);Ps_Text(278,90,"TEAM 2",.64f,200,1);
    for(i=0;i<Port_TagSlots();++i){const char *name=Port_TagPlayerName(i);f32 x=Port_TagTeam(i)?278:28,y=123+21.0f*Port_TagTeamIndex(i);
        if(Port_TagCPU(i))sprintf(line,"%d  CPU %02d",Port_TagTeamIndex(i)+1,Port_TagTeamIndex(i)+1);
        else sprintf(line,"%d  %s%s",Port_TagTeamIndex(i)+1,name[0]?name:(char *)PortOnline_Text("Empty"),i==Port_TagSeat()?(char *)PortOnline_Text(" (YOU)"):"");
        Ps_TextRaw(x,y,line,.48f,122,i==sTb->tagRoomCur?1:name[0]?0:4);
        if(Port_TagCPU(i)){Ps_Align(2);Ps_Text(x+204,y,sTagDifficulty[Port_TagDifficulty(i)],.36f,78,3);Ps_Align(0);}
    }
    if(Port_TagNetwork()){Ps_Align(1);Ps_Text(256,106,"SELECT: COPY CODE",.33f,220,0);Ps_Align(0);}
    if(Port_TagChatCount())Ps_TextRaw(24,326,(char*)Port_TagChatLine(Port_TagChatCount()-1),.42f,464,0);
}
static void Tb_DrawJoin(void) {
    s32 c;

    Tb_Hosts(TBO_HOSTS_PUZZLED, 432.0f, 350.0f, 200.0f);
    for (c = 0; c < CODE_LEN; c++) { /* the code's six boxes */
        f32 x = 34.0f + 50.0f * (f32)c;

        Ps_Pill(c == sTb->codeLen ? TBO_KIT_SEL : TBO_KIT_BAR, x, 76.0f, x + 44.0f, 116.0f, 0x80);
    }
    Ps_Panel(TBO_KIT_WINDOW, 14.0f, 126.0f, 352.0f, 330.0f, 0x80);
}

static void Tb_DrawJoinText(void) {
    char one[2] = {0, 0};
    s32 c;

    Ps_Align(1);
    for (c = 0; c < CODE_LEN; c++) {
        f32 x = 56.0f + 50.0f * (f32)c;

        one[0] = c < sTb->codeLen ? sTb->code[c] : (c == sTb->codeLen && (sTb->frames / 16) & 1 ? '_' : ' ');
        Ps_Text(x, 81.0f, one, 1.0f, 0.0f, c < sTb->codeLen ? 3 : 0); /* (what is typed: a value) */
    }
    Ps_Align(0);
    Tb_Keypad(24.0f, 138.0f, KEY_ROWS, sTbKeys, sTb->tagJoin?sTagJoinBtn:sTbJoinBtn, 3);
}

static void Tb_DrawWait(void) {
    if (sTb->found) {
        Tb_Hosts(TBO_HOSTS_CHEER, 426.0f, 350.0f, 180.0f);
    } else {
        Tb_Hosts(TBO_HOSTS_BINOCULARS, 434.0f, 350.0f, 210.0f);
    }
    Tb_Radar(264.0f, 188.0f, 80.0f);
    Ps_Panel(TBO_KIT_WINDOW, 10.0f, 84.0f, 180.0f, 246.0f, 0x80);
    Ps_Panel(TBO_KIT_WINDOW, 176.0f, 292.0f, 348.0f, 328.0f, 0x80); /* the search's clock (a window, not a bar to choose) */
}

static void Tb_DrawWaitText(void) {
    static char line[96];
    s32 secs = (sTb->frames - sTb->waitFrom) / 60;
    const char *code = getenv("BT4_RENDER_TBO") != NULL ? "YUH9JP" : Port_LobbyCode();

    if (sTb->host == 2) {
        s32 n = Port_LobbyQueueWaiting(sTb->queue);

        Ps_Text(22.0f, 94.0f, "QUICK MATCH", 0.56f, 0.0f, 0);
        Ps_Align(1);
        Ps_Text(95.0f, 124.0f, sTb->queue ? "TEAMS" : "ONE ON ONE", 0.86f, 156.0f, 1);
        Ps_Align(0);
        Ps_Text(22.0f, 172.0f, "THE SERVICE PAIRS YOU", 0.46f, 150.0f, 0);
        Ps_Text(22.0f, 190.0f, "WITH THE NEXT RIVAL", 0.46f, 150.0f, 0);
        if (n > 0) {
            sprintf(line, (char *)PortOnline_Text("%d IN THIS QUEUE"), (int)n);
            Ps_Text(22.0f, 214.0f, line, 0.42f, 150.0f, 5);
        }
    } else if (sTb->host) {
        Ps_Text(22.0f, 94.0f, "ROOM CODE", 0.56f, 0.0f, 0);
        Ps_Align(1);
        if (code[0] != 0) {
            Ps_TextRaw(95.0f, 120.0f, (char *)code, 1.2f, 156.0f, 0);
        } else {
            Ps_Text(95.0f, 130.0f, "GETTING A CODE...", 0.5f, 156.0f, 0);
        }
        Ps_Align(0);
        Ps_Text(22.0f, 172.0f, code[0] != 0 ? "GIVE THIS CODE TO" : "ASKING THE", 0.46f, 150.0f, 0);
        Ps_Text(22.0f, 190.0f, code[0] != 0 ? "YOUR RIVAL" : "MATCHMAKING SERVICE", 0.46f, 150.0f, 0);
        if (Port_LobbyOther()) {
            Ps_Text(22.0f, 214.0f, "ANOTHER VERSION TRIED", 0.42f, 150.0f, 3);
        }
    } else {
        Ps_Text(22.0f, 94.0f, "JOINING ROOM", 0.56f, 0.0f, 0);
        Ps_Align(1);
        Ps_TextRaw(95.0f, 120.0f, sTb->code, 1.2f, 156.0f, 0);
        Ps_Align(0);
        Ps_Text(22.0f, 172.0f, "LOOKING FOR", 0.46f, 150.0f, 0);
        Ps_Text(22.0f, 190.0f, "THE HOST...", 0.46f, 150.0f, 0);
    }
    Ps_Align(1);
    if (sTb->found) {
        Ps_Text(262.0f, 302.0f, "YOUR RIVAL IS HERE!", 0.56f, 164.0f, 5);
    } else {
        sprintf(line, (char *)PortOnline_Text(sTb->host==2?"SEARCHING...  %d:%02d":"WAITING...  %d:%02d"), (int)(secs / 60), (int)(secs % 60));
        Ps_Text(262.0f, 302.0f, line, 0.52f, 164.0f, 0);
    }
    Ps_Align(0);
}

static void Tb_DrawBlock(void) {
    Ps_Panel(TBO_KIT_WINDOW, 60.0f, 96.0f, 452.0f, 316.0f, 0x80);
    Tb_Hosts(TBO_HOSTS_PUZZLED, 404.0f, 120.0f, 92.0f);
    Ps_Pill(TBO_KIT_GOLD, 206.0f, 282.0f, 306.0f, 306.0f, 0x80);
}

static void Tb_DrawBlockText(void) {
    static char parts[96];
    const char *t = Port_DataFingerprintText();
    const char *m = t;
    s32 n;

    if(sTb->blockBack==TB_TAG||(sTb->blockBack==TB_JOIN&&sTb->tagJoin)){
        Ps_Heading(80,108,"Unable to Connect",.9f,250,0x80);
        Ps_Text(80,150,"Tag Team requires matching BT4 builds",.52f,275,0);
        Ps_Text(80,172,"and the same B14 Rev2 data on every computer.",.52f,275,0);
        Ps_Text(80,207,"Game data verification has not passed.",.48f,275,3);
        Ps_Text(80,232,"Check your game files, then try again.",.48f,275,0);
        Ps_Align(1);Ps_Text(256,285,"BACK",.56f,90,1);Ps_Align(0);return;
    }

    Ps_Heading(80.0f, 108.0f, "Cannot Go Online", 0.9f, 250.0f, 0x80);
    Ps_Msg(80.0f, 146.0f, "Online matches are only between", 1.0f, 0x80);
    Ps_Msg(80.0f, 166.0f, "two matching BT4 B14 Rev2 builds.", 1.0f, 0x80);
    while (*m != 0 && strncmp(m, "modified:", 9) != 0) {
        m++;
    }
    if (*m != 0) { /* which parts differ, as the data check says it */
        Ps_Copy(parts, "Changed: ", 96);
        Ps_Cat(parts, m + 10, 9 + 81);
    } else if (Port_DataFingerprint(NULL) == 1) {
        sprintf(parts, "Your data differs from BT4 B14 Rev2.");
    } else {
        sprintf(parts, "BT4 data verification has not passed.");
    }
    for (n = 0; parts[n] != 0; n++) {
        if (parts[n] == ',') {
            parts[n] = ' ';
        }
    }
    Ps_Msg(80.0f, 196.0f, parts, 1.0f, 0x80);
    Ps_Msg(80.0f, 226.0f, "Use this BT4 build on both computers.", 1.0f, 0x80);
    Ps_Msg(80.0f, 246.0f, "(Original BT3 and other mods use different data.)", 1.0f, 0x70);
    Ps_Align(1);
    Ps_Text(256.0f, 285.0f, "BACK", 0.56f, 0.0f, 1);
    Ps_Align(0);
}

/* The name's window: over everything else on the screen (drawn last). */
static void Tb_DrawName(void) {
    static char line[40];

    Ps_Panel(TBO_KIT_WINDOW, 64.0f, 62.0f, 448.0f, 340.0f, 0x80);
    Ps_Text(84.0f, 70.0f, "TYPE YOUR NAME", 0.6f, 0.0f, 2);
    Ps_Pill(TBO_KIT_BAR, 84.0f, 94.0f, 428.0f, 124.0f, 0x80);
    Ps_Copy(line, sTb->typed, 40);
    Ps_Cat(line, (sTb->frames / 16) & 1 ? "_" : " ", 40);
    Ps_TextRaw(100.0f, 97.0f, line, 0.86f, 310.0f, 1);
    Tb_Keypad(94.0f, 134.0f, NAME_ROWS, sTbNameKeys[sTb->lower], sTbNameBtn, 4);
}

static void Tb_DrawSpar(void) {
    Tb_Hosts(TBO_HOSTS_CHEER, 406.0f, 350.0f, 200.0f);
    Tb_DrawRows(3, sTb->spCur, 110.0f);
    Ps_Panel(TBO_KIT_WINDOW, 14.0f, 230.0f, 300.0f, 326.0f, 0x80);
}

static void Tb_DrawSparText(void) {
    static char line[64];
    s32 secs = (sTb->frames - sTb->waitFrom) / 60;

    Ps_Text(26.0f, 76.0f, "SPARRING AT THE TOURNAMENT", 0.6f, 270.0f, 2);
    Ps_Text(36.0f, 115.0f, "You", 0.66f, 70.0f, sTb->spCur == 0 ? 1 : 0);
    Ps_Text(36.0f, 149.0f, "Partner", 0.66f, 80.0f, sTb->spCur == 1 ? 1 : 0);
    Ps_Text(36.0f, 183.0f, "Start Training", 0.66f, 200.0f, sTb->spCur == 2 ? 1 : 0);
    Ps_Align(2);
    Ps_Text(266.0f, 117.0f, Tb_Fighter(sTb->spMe), 0.54f, 170.0f, 3);
    Ps_Text(266.0f, 151.0f, Tb_Fighter(sTb->spFoe), 0.54f, 170.0f, 3);
    Ps_Align(0);
    Ps_Text(28.0f, 240.0f, sTb->host == 2 ? "QUICK MATCH SEARCH" : sTb->host ? "YOUR ROOM IS OPEN" : "JOINING A ROOM", 0.5f, 260.0f, 2);
    sprintf(line, (char *)PortOnline_Text("SEARCHING  %d:%02d"), (int)(secs / 60), (int)(secs % 60));
    Ps_Text(28.0f, 264.0f, line, 0.52f, 260.0f, 5);
    Ps_Text(28.0f, 292.0f, "THE MATCH STARTS THE MOMENT", 0.42f, 260.0f, 0);
    Ps_Text(28.0f, 308.0f, "YOUR RIVAL ARRIVES", 0.42f, 260.0f, 0);
}

static void Tb_DrawQuick(void) {
    Tb_Hosts(TBO_HOSTS_MIC_WAVE, 404.0f, 350.0f, 240.0f);
    Tb_DrawRows(2, sTb->qkCur, 110.0f);
    Ps_Panel(TBO_KIT_WINDOW, 14.0f, 196.0f, 300.0f, 330.0f, 0x80);
}

static void Tb_DrawQuickText(void) {
    static char line[64];
    s32 q;

    Ps_Text(26.0f, 76.0f, "PICK YOUR BATTLE", 0.62f, 0.0f, 2);
    Ps_Text(36.0f, 115.0f, "Battle", 0.66f, 110.0f, sTb->qkCur == 0 ? 1 : 0);
    Ps_Align(2);
    Ps_Text(266.0f, 117.0f, sTb->queue ? "TEAMS (UP TO 5)" : "ONE ON ONE", 0.54f, 140.0f, 3);
    Ps_Align(0);
    Ps_Text(36.0f, 149.0f, "Search", 0.66f, 110.0f, sTb->qkCur == 1 ? 1 : 0);
    Ps_Text(28.0f, 206.0f, "WAITING RIGHT NOW", 0.52f, 0.0f, 2);
    for (q = 0; q < 2; q++) {
        s32 n = Port_LobbyQueueWaiting(q);

        Ps_Text(36.0f, 234.0f + 26.0f * (f32)q, q ? "Teams" : "One on One", 0.56f, 120.0f, q == sTb->queue ? 1 : 0);
        if (n < 0) {
            sprintf(line, "...");
        } else {
            sprintf(line, (char *)PortOnline_Text("%d WAITING"), (int)n);
        }
        Ps_Align(2);
        Ps_Text(286.0f, 236.0f + 26.0f * (f32)q, line, 0.5f, 120.0f, n > 0 ? 5 : 3);
        Ps_Align(0);
    }
    sprintf(line, (char *)PortOnline_Text("NO TIME LIMIT - %d%% SPEED"), (int)Port_NetSpeedStandard());
    Ps_Text(28.0f, 296.0f, line, 0.46f, 260.0f, 3);
}

static void Tb_DrawProfile(void) {
    Tb_Hosts(TBO_HOSTS_THUMBS_UP, 408.0f, 350.0f, 250.0f);
    Tb_DrawRows(PR_COUNT, sTb->prCur, 110.0f);
}

static void Tb_DrawProfileText(void) {
    static char value[PR_COUNT][32];
    static char *label[PR_COUNT] = {"Name"};
    s32 i;

    Ps_Copy(value[PR_NAME], Port_NetNameGet(), 32);
    Ps_Text(26.0f, 76.0f, "PLAYER CARD", 0.62f, 0.0f, 2);
    for (i = 0; i < PR_COUNT; i++) {
        f32 y = 110.0f + 34.0f * (f32)i;

        Ps_Text(36.0f, Ps_TextTop(y + 13.0f, 0.66f), label[i], 0.66f, 110.0f, i == sTb->prCur ? 1 : 0);
        Ps_Align(2);
        Ps_TextRaw(266.0f, Ps_TextTop(y + 13.0f, 0.54f), value[i], 0.54f, 140.0f, 3);
        Ps_Align(0);
    }
}

/* What the hosts say in the box at the foot of the screen (two lines at most). */
static char *Tb_Help(void) {
    switch (sTb->mode) {
    case TB_HUB:
        return sTbHubSay[sTb->hubCur];
    case TB_TAG:{
        static char *help[8]={
            "Set the number of slots on Team 1. Both teams can have up to ten fighters in total.",
            "Set the number of slots on Team 2. The teams can have different numbers of fighters.",
            "Choose a time limit or play until one team is defeated.",
            "Set the input delay in frames. Higher values allow more time for network fluctuations.",
            "Fill empty slots with CPU fighters or leave them open. You can also edit each slot in the room.",
            "Set the starting CPU difficulty. Each CPU slot can have its own difficulty in the room.",
            "Create a room and share its code. You can start with empty slots if both teams have a fighter.",
            "Join your friends with a six-character room code. Everyone must use the same BT4 build."};
        return help[sTb->tagCur];}
    case TB_TAG_ROOM:
        if(sTb->tagChatTyping)return "Type your message with the keyboard. ENTER sends it and ESC closes the chat.";
        if(!Port_TagHost())return "Share the code to invite friends. The host arranges the teams and starts character selection.";
        if(sTb->tagMoving>=0)return "Choose a destination slot and confirm to move or swap the player. TRIANGLE cancels.";
        if(Port_TagCPU(sTb->tagRoomCur))return "X toggles a CPU in this slot. L1/R1 changes its difficulty. SQUARE starts character selection.";
        return "Select a player with X, then choose a destination slot. Empty slots can hold CPU fighters. SQUARE starts selection.";
    case TB_CREATE:
        return sTb->crCur == CR_DELAY ? "Choose input delay in frames. Automatic adjusts it when the match starts."
               : sTb->crCur == CR_ROLL ? "Choose the rollback window. A larger window allows more frames of prediction and correction."
               : "Choose your battle rules, create the room, and share its code with your opponent.";
    case TB_JOIN:
        return sTb->tagJoin?"Enter the six-character room code or paste it from the clipboard, then confirm to join.":"Enter the six-character room code or paste it from the clipboard, then confirm to join.";
    case TB_WAIT:
        if(sTb->found)return "Here we go! The match is starting!";
        if(sTb->host==2)return "Quick Match searches its public queue. It only matches players using the same battle type. To enter a room code, choose Join Room.";
        if(sTb->host)return "Your private room is open. Share its code; your rival must choose Join Room and enter it. Quick Match does not use room codes.";
        return "Joining the private room with this code. Wait for the host to connect.";
    case TB_BLOCK:
        return sTb->blockBack==TB_TAG||(sTb->blockBack==TB_JOIN&&sTb->tagJoin)?"Every player needs the same BT4 B14 Rev2 build and verified game data.":"Online fights require the same BT4 B14 Rev2 build and verified data on both computers.";
    case TB_SPAR:
        return "Warm up at the tournament while we search! Pick yourself and a sparring partner.";
    case TB_QUICK:
        return "Press Search to enter the public queue. It only matches players searching the same battle type. To use a room code, choose Join Room.";
    case TB_NAME:
        return "Enter your player name, then choose Done. SQUARE deletes a character; START confirms.";
    case TB_PROFILE:
        return "Choose the player name shown to opponents and other players in your rooms.";
    }
    return "";
}

/* Each page's title (by TB_ mode), as its heading shows it and the screen reader says it. */
static char *sTbTitle[11] = {"Tenkaichi Budokai Online", "Create Room", "Join Room", "Searching", "Profile", "Profile", "Quick Match",
                            "Train While Waiting", "Tenkaichi Budokai Online", "Tag Team", "Tag Team Room"};

/* ---- the screen reader (port/src/gs/a11y.c; silent unless the player has it on). The game's own screens are read
   from their state by port/src/a11y_game.c; these pages say what they show themselves: the page's title as it opens,
   then what the cursor is on (and its value), the code or the name typed so far, and the notes. The hosts' line is
   the box's, which the reader reads as it reads the game's. ---- */
extern void PortA11y_Say(const char *text, int flags);
#define TB_SAY_INTERRUPT 1

/* A room code letter by letter ("J D Y 2 T E"). */
static void Tb_Spell(char *out, const char *code, s32 max) {
    s32 n = 0;
    s32 i;

    for (i = 0; code[i] != 0 && n < max - 3; i++) {
        if (n > 0) {
            out[n++] = ' ';
        }
        out[n++] = code[i];
    }
    out[n] = 0;
}

/* What a keypad's cursor is on: a key's character, or a button's label. */
static void Tb_KeyFocus(char *out, s32 rows, char (*keys)[KEY_COLS + 1], const TbKeyBtn *b, s32 nb) {
    s32 k;

    out[0] = 0;
    if (sTb->keyRow < rows) {
        out[0] = keys[sTb->keyRow][sTb->keyCol];
        out[1] = 0;
        return;
    }
    for (k = 0; k < nb; k++) {
        if (sTb->keyCol >= b[k].c0 && sTb->keyCol <= b[k].c1) {
            Ps_Copy(out, b[k].label, 32);
        }
    }
}

/* Once a frame, after the page's update. */
static void Tb_Speak(void) {
    static s32 mode = -1;
    static char said[160];
    static char typed[40];
    static s32 noted;
    char focus[160];
    char part[64];
    char now[40];

    if (sTb->frames == 0) { /* (the screens opened again) */
        mode = -1;
        typed[0] = 0;
    }
    focus[0] = 0;
    now[0] = 0;
    switch (sTb->mode) {
    case TB_HUB:
        Ps_Copy(focus, sTbHubName[sTb->hubCur], 160);
        if (!sTbHubReady[sTb->hubCur]) {
            Ps_Cat(focus, ", soon", 160);
        }
        break;
    case TB_TAG:
        Ps_Copy(focus, "Tag Team Settings", 160); break;
    case TB_TAG_ROOM:
        sprintf(focus, "Tag Team Room, %d of %d fighters", Port_TagCount(), Port_TagCapacity()); break;
    case TB_CREATE: {
        static char *label[CR_COUNT] = {"Battle", "Time Limit", "Input Delay", "Rollback", "Open Room"};

        Ps_Copy(focus, label[sTb->crCur], 160);
        part[0] = 0;
        if (sTb->crCur == CR_BATTLE) {
            Ps_Copy(part, sTb->battle ? "Teams, up to 5" : "One on one", 64);
        } else if (sTb->crCur == CR_TIME) {
            Ps_Copy(part, sTbTime[sTb->time], 64);
        } else if (sTb->crCur == CR_DELAY) {
            Tb_DelayText(part, sTb->delay);
        } else if (sTb->crCur == CR_ROLL) {
            Tb_RollText(part, sTb->roll);
        }
        if (part[0] != 0) {
            Ps_Cat(focus, ": ", 160);
            Ps_Cat(focus, part, 160);
        }
        break;
    }
    case TB_JOIN:
        Tb_KeyFocus(focus, KEY_ROWS, sTbKeys, sTb->tagJoin?sTagJoinBtn:sTbJoinBtn, 3);
        Ps_Copy(now, sTb->code, 40);
        break;
    case TB_NAME:
        Tb_KeyFocus(focus, NAME_ROWS, sTbNameKeys[sTb->lower], sTbNameBtn, 4);
        Ps_Copy(now, sTb->typed, 40);
        break;
    case TB_WAIT:
        if (sTb->found) {
            Ps_Copy(focus, "Your rival is here! The match begins.", 160);
        } else if (sTb->host == 2) {
            Ps_Copy(focus, sTb->queue ? "Quick match, teams: searching" : "Quick match, one on one: searching", 160);
        } else if (sTb->host) {
            if (Port_LobbyCode()[0] != 0) {
                Ps_Copy(focus, "Your room code: ", 160);
                Tb_Spell(part, Port_LobbyCode(), 64);
                Ps_Cat(focus, part, 160);
            } else {
                Ps_Copy(focus, "Getting a room code", 160);
            }
        } else {
            Ps_Copy(focus, "Joining room ", 160);
            Tb_Spell(part, sTb->code, 64);
            Ps_Cat(focus, part, 160);
        }
        break;
    case TB_QUICK:
        if (sTb->qkCur == 0) {
            Ps_Copy(focus, sTb->queue ? "Battle: teams" : "Battle: one on one", 160);
        } else {
            Ps_Copy(focus, "Search", 160);
        }
        break;
    case TB_SPAR:
        if (sTb->spCur == 0) {
            Ps_Copy(focus, "You: ", 160);
            Ps_Cat(focus, Tb_Fighter(sTb->spMe), 160);
        } else if (sTb->spCur == 1) {
            Ps_Copy(focus, "Partner: ", 160);
            Ps_Cat(focus, Tb_Fighter(sTb->spFoe), 160);
        } else {
            Ps_Copy(focus, "Start training", 160);
        }
        break;
    case TB_BLOCK:
        Ps_Copy(focus, "Cannot go online. Online matches are only between two matching BT4 B14 Rev2 builds.", 160);
        break;
    default:
        Ps_Copy(focus, "Name: ", 160);
        Ps_Cat(focus, (char *)Port_NetNameGet(), 160);
        break;
    }
    if (sTb->mode != mode) {
        static char line[224];

        mode = sTb->mode;
        Ps_Copy(line, sTbTitle[mode], 224);
        Ps_Cat(line, ". ", 224);
        Ps_Cat(line, focus, 224);
        PortA11y_Say(line, TB_SAY_INTERRUPT);
        Ps_Copy(said, focus, 160);
        Ps_Copy(typed, now, 40);
    } else { /* what was typed, then where the cursor went (one line: two in one blank, the second cuts the first) */
        static char line[256];
        s32 k;

        line[0] = 0;
        for (k = 0; now[k] == typed[k] && now[k] != 0; k++) {
        }
        if (now[k] != typed[k]) { /* the code or the name typed so far */
            Ps_Copy(typed, now, 40);
            Ps_Copy(line, sTb->mode == TB_JOIN ? (sTb->tagJoin?"Code: ":"Code: ") : "Name: ", 256);
            if (sTb->mode == TB_JOIN) {
                Tb_Spell(part, now, 64);
                Ps_Cat(line, now[0] != 0 ? part : (sTb->tagJoin?"empty":"empty"), 256);
            } else {
                Ps_Cat(line, now[0] != 0 ? now : "empty", 256);
            }
        }
        for (k = 0; focus[k] == said[k] && focus[k] != 0; k++) {
        }
        if (focus[k] != said[k]) {
            Ps_Copy(said, focus, 160);
            if (line[0] != 0) {
                Ps_Cat(line, ". ", 256);
            }
            Ps_Cat(line, focus, 256);
        }
        if (line[0] != 0) {
            PortA11y_Say(line, TB_SAY_INTERRUPT);
        }
    }
    if (sTb->note > 0 && !noted) { /* a note (copied, a code that did not work ...) */
        PortA11y_Say(sTb->noteLine, 0);
    }
    noted = sTb->note > 0;
}

/* The hosts' line (Tb_Help) for the game's box, in the game's text: 16-bit, 0xFEFF first, cut at spaces into lines of
   what fits (three at most) up to the step of the box's top right corner (at about 410 of the screen's 512: the box
   is the game's movie, over the whole screen). Another buffer for another line (the screen reader tells them apart by
   that). NULL: no line. */
#define TB_BOX_LINE 224
static u16 *Tb_BoxLine(void) {
    static u16 buf[2][TB_BOX_LINE];
    static char said[TB_BOX_LINE];
    static s32 cur = -1;
    char *say = (char *)PortOnline_Text(Tb_Help());
    u16 *out;
    s32 room;
    s32 n;
    s32 start;
    s32 lines;
    s32 at;
    s32 k;

    if (say == NULL || say[0] == 0) {
        return NULL;
    }
    for (k = 0; cur >= 0 && say[k] == said[k] && say[k] != 0; k++) {
    }
    if (cur >= 0 && say[k] == said[k]) {
        return buf[cur];
    }
    Ps_Copy(said, say, TB_BOX_LINE);
    cur = cur == 0 ? 1 : 0;
    out = buf[cur];
    room = MsgWin_PortLineRoom(400);
    out[0] = 0xFEFF;
    n = 1;
    start = 1;
    lines = 1;
    at = 0;
    while (say[at] == ' ') {
        at++;
    }
    while (say[at] != 0 && n < TB_BOX_LINE - 2) {
        s32 end = at;
        s32 mark = n;

        while (say[end] != 0 && say[end] != ' ') {
            end++;
        }
        if (n > start) {
            out[n++] = ' ';
        }
        for(k=at;k<end && n<TB_BOX_LINE-1;){
            u32 c=(u8)say[k++];
            if(c>=0xC2 && c<=0xDF && k<end){c=(c&31)<<6;c|=(u8)say[k++]&63;}
            else if(c>=0xE0 && c<=0xEF && k+1<end){c=(c&15)<<12;c|=((u8)say[k++]&63)<<6;c|=(u8)say[k++]&63;}
            out[n++]=(u16)c;
        }
        out[n] = 0;
        if (mark > start && lines < 3 && Font_GetWidth(out + start) > room) { /* the word goes to the next line */
            out[mark] = '\n';
            start = mark + 1;
            lines++;
        }
        at = end;
        while (say[at] == ' ') {
            at++;
        }
    }
    out[n] = 0;
    return out;
}

/* Captions use the same proportional font pipeline as the menu labels. Their
 * original message record remains attached for screen readers. */
static s32 Tb_HelpWrap(char *text,char lines[3][128],f32 scale,f32 width){
    s32 at=0,row=0,used=0;char trial[128];
    memset(lines,0,3*128);
    while(text[at]){
        s32 end=at,k,n=used;
        while(text[at]==' ')++at;if(!text[at])break;end=at;
        while(text[end]&&text[end]!=' ')++end;
        Ps_Copy(trial,lines[row],128);
        if(n&&n<127)trial[n++]=' ';
        for(k=at;k<end&&n<127;++k)trial[n++]=text[k];trial[n]=0;
        if(used&&Ps_MsgWidth(trial,scale)>width){
            if(++row>=3)return 4;used=0;continue;
        }
        Ps_Copy(lines[row],trial,128);used=n;at=end;
    }
    return row+1;
}
static void Tb_DrawHelpCaption(void){
    static char previous[TB_BOX_LINE],lines[3][128];
    static unsigned generation;
    static s32 count;
    static f32 scale,lastWidth;
    char *text=(char *)PortOnline_Text(Tb_Help());f32 width=(512.0f-2.0f*Ps_Left())*.72f;
    unsigned current=PortTbo_TextGeneration();s32 i,same=1;
    for(i=0;text[i]||previous[i];++i)if(text[i]!=previous[i]){same=0;break;}
    if(!same||current!=generation||width!=lastWidth||!count){
        Ps_Copy(previous,text,TB_BOX_LINE);generation=current;lastWidth=width;
        for(scale=1.08f;scale>.80f;scale-=.04f){count=Tb_HelpWrap(text,lines,scale,width);if(count<=3)break;}
        if(count>3)count=3;
        if(getenv("BT4_RENDER_TEST")&&getenv("BT4_TBO_TEXT_CAPTURE"))
            fprintf(stderr,"tag typography caption: lines=%d scale=%.2f width=%.1f\n",count,scale,width);
    }
    for(i=0;i<count;++i)Ps_Msg(Ps_Left()+34.0f,344.0f+i*22.0f,lines[i],scale,0x80);
}

/* What a page draws comes in from the left as it opens, as the hub's bars do (the hub moves its own: Tb_DrawHub);
   the title, the player card and the box stay where they are. The name's keypad comes in over the profile, which
   stays (on: 2 for the keypad). */
static void Tb_PageIn(s32 on) {
    s32 page = on == 2 || (on == 1 && sTb->mode != TB_HUB && sTb->mode != TB_BLOCK && sTb->mode != TB_NAME);

    Ps_SetShift(page ? (1.0f - Tb_In(0)) * -160.0f : 0.0f);
}

static void Tb_Draw(void) {
    Tb_PageIn(1);
    switch (sTb->mode) {
    case TB_HUB:
        Tb_DrawHub();
        break;
    case TB_TAG:
        Tb_DrawTag(); break;
    case TB_TAG_ROOM:
        Tb_DrawTagRoom(); break;
    case TB_CREATE:
        Tb_DrawCreate();
        break;
    case TB_JOIN:
        Tb_DrawJoin();
        break;
    case TB_WAIT:
        Tb_DrawWait();
        break;
    case TB_QUICK:
        Tb_DrawQuick();
        break;
    case TB_SPAR:
        Tb_DrawSpar();
        break;
    case TB_BLOCK:
        break; /* (drawn last, over the rest) */
    default:
        Tb_DrawProfile();
        break;
    }
    Tb_PageIn(0);
    Ps_Panel(TBO_KIT_WINDOW, 300.0f, 10.0f, 504.0f, 44.0f, 0x80); /* the player card: the name alone */
    MsgWin_PortDrawStringHd(Tb_BoxLine());
    Tb_DrawHelpCaption();
    if (sTb->mode == TB_BLOCK) {
        Tb_DrawBlock();
    }
}

static void Tb_DrawText(void) {
    char **title = sTbTitle;
    const char *heading=title[sTb->mode];
    if(sTb->mode==TB_WAIT&&!sTb->found&&sTb->host!=2)heading=sTb->host?"Room Open":"Joining Room";

    /* its capitals centred on the crest beside it (y 32); up to the player card */
    Ps_Heading(Tb_TitleLeft() + 44.0f, Ps_TextTop(32.0f, 1.3f), (char *)PortOnline_Text((sTb->mode==TB_JOIN&&sTb->tagJoin)?"Join Room":heading), 1.3f, 248.0f, 0x80);
    Tb_PageIn(1);
    switch (sTb->mode) {
    case TB_HUB:
        Tb_DrawHubText();
        break;
    case TB_TAG:
        Tb_DrawTagText(); break;
    case TB_TAG_ROOM:
        Tb_DrawTagRoomText(); break;
    case TB_CREATE:
        Tb_DrawCreateText();
        break;
    case TB_JOIN:
        Tb_DrawJoinText();
        break;
    case TB_WAIT:
        Tb_DrawWaitText();
        break;
    case TB_QUICK:
        Tb_DrawQuickText();
        break;
    case TB_SPAR:
        Tb_DrawSparText();
        break;
    case TB_BLOCK:
        break;
    default:
        Tb_DrawProfileText();
        break;
    }
    Tb_PageIn(0);
    /* (no status and no title under it: "ONLINE" was not whether the game is connected, and the title is the
       screen's heading) */
    Ps_TextRaw(322.0f, Ps_TextTop(27.0f, 0.6f), (char *)Port_NetNameGet(), 0.6f, 166.0f, 1);
    if (sTb->mode == TB_TAG_ROOM) {
        char occupancy[40];
        sprintf(occupancy, Port_TagHumans()==1?(char *)PortOnline_Text("%d/%d | %d PLAYER + %d CPU"):(char *)PortOnline_Text("%d/%d | %d PLAYERS + %d CPU"), Port_TagCount(), Port_TagCapacity(), Port_TagHumans(), Port_TagCount() - Port_TagHumans());
        Ps_Align(2); Ps_Text(494.0f, 48.0f, occupancy, 0.50f, 190.0f, 3); Ps_Align(0);
    }
    if (sTb->mode == TB_BLOCK) {
        Tb_DrawBlockText();
    }
    if (sTb->mode == TB_NAME) {
        Tb_PageIn(2);
        Tb_DrawName();
        Tb_PageIn(0);
    }
    if (sTb->note > 0) {
        s32 a = sTb->note > 16 ? 0x80 : sTb->note * 8;

        Ps_Align(1);
        Ps_TextEx(256.0f, 330.0f, sTb->noteLine, 0.52f, 480.0f, Ps_Rgba(0xFF, 0xF8, 0xB0, a), Ps_Rgba(0xFF, 0xC8, 0x30, a), Ps_Rgba(0x5C, 0x24, 0x08, a));
        Ps_Align(0);
    }
}

/* ----------------------------------------------------------------------------------------------- the frame loop */
/* From the main menu's item until triangle on the hub. Returns 1 when it was left for "Train While Waiting" (the
   room is still open; Duel_Main goes on to the Ultimate Training menu), 0 back to the main menu. */
/* The Duel menu's screen unpacked (the windows' data), while the screens run. */
static void *sTbWinRes;

static void Tb_WinInit(void) {
    void *icons;

    sTbWinRes = Sprite_Unpack(MPACK_AT(gMenuArc5, 1), NULL, NULL); /* (Duel_Main: DuelMenu_Run(1)) */
    icons = MPACK_AT(sTbWinRes, 24);
    Res_RelocateOffsets(&icons, icons, icons);
    IconWin_Init(MPACK_AT(sTbWinRes, 23), icons);
    IconWin_PortHideWords(); /* "Versus Menu" and its emblem: the screens' own title is written there */
    IconWin_Open();
    MsgWin_Init(MPACK_AT(sTbWinRes, 22), MPACK_AT(sTbWinRes, 21), 1, 0);
    MsgWin_SetText(NULL); /* the box only: the hosts' lines are the screens' own (Tb_BoxLine) */
    MsgWin_PortSameTop();
    MsgWin_Open();
}

static void Tb_WinTerm(void) {
    if (sTbWinRes == NULL) {
        return;
    }
    MsgWin_Term();
    IconWin_Term();
    Heap_Free(sTbWinRes);
    sTbWinRes = NULL;
}

s32 PortTbo_Run(void) {
    s32 train;
    static s32 returnTestRuns;
    s32 returnedTest=getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_RETURN_TEST")&&returnTestRuns++;

    sTb = Heap_Alloc(sizeof(TboScreen), 0x20, 0, 2);
    memset(sTb, 0, sizeof(TboScreen));
    sTb->tagMoving=-1;
    sTb->hubCur = HUB_CREATE;
    sTb->hubY = Tb_HubRowY(HUB_CREATE);
    Tb_LoadChoices();
    sTb->tagDelay=Port_Setting("tag_team_delay",6);if(sTb->tagDelay<0||sTb->tagDelay>12)sTb->tagDelay=6;
    if (getenv("BT4_TAG_PREVIEW") != NULL) { sTb->hubCur = HUB_TAG; sTb->hubY = Tb_HubRowY(HUB_TAG); }
    if (Port_TagCapacity() > 0) Tb_SetMode(TB_TAG_ROOM);
    Port_NetNameGet();
    gPortTboSpar = 0;
    if (Port_LobbyInBackground()) { /* back with a room open (from the main menu after training): its search again */
        Port_LobbyBackground(0);
        sTb->host = sTbBgKind;
        sTb->queue = sTbBgQueue;
        Ps_Copy(sTb->code, sTbBgCode, CODE_LEN + 1);
        sTb->waitBack = TB_HUB;
        Tb_SetMode(TB_WAIT);
    }
    Port_EscQuit(0);
    Tb_WinInit();
    if (getenv("BT4_RENDER_TBO") != NULL) {
        const char *which = getenv("BT4_RENDER_TBO");
        if (which[0] == 't') Tb_SetMode(TB_TAG);
        else if (which[0] == 'c' && getenv("BT4_RENDER_TEST")) Tb_SetMode(TB_CREATE);
        else if (which[0] == 'j' && getenv("BT4_RENDER_TEST")) {sTb->tagJoin=1;Tb_SetMode(TB_JOIN);}
        else if (which[0] == 'p' && getenv("BT4_RENDER_TEST")) Tb_SetMode(TB_PROFILE);
        else if (which[0] == 'n' && getenv("BT4_RENDER_TEST")) Tb_SetMode(TB_NAME);
        else if (which[0] == 'q' && getenv("BT4_RENDER_TEST")) Tb_SetMode(TB_QUICK);
        else if (which[0] == 'h') { sTb->hubCur = HUB_TAG; sTb->hubY = Tb_HubRowY(HUB_TAG); }
        else { sTb->host = 1; Tb_SetMode(TB_WAIT); }
    }
    if (!returnedTest && getenv("BT4_TAG_CREATE_TEST") && !getenv("BT4_TAG_VIDEO_TEST")) {
        sTb->tagCPU=1; sTb->tagCur=6;
        sTb->tagSize=getenv("BT4_TAG_TEST_SIZE")?atoi(getenv("BT4_TAG_TEST_SIZE")):5;
        sTb->tagLeft=getenv("BT4_TAG_TEST_LEFT")?atoi(getenv("BT4_TAG_TEST_LEFT")):sTb->tagSize;
        sTb->tagRight=getenv("BT4_TAG_TEST_RIGHT")?atoi(getenv("BT4_TAG_TEST_RIGHT")):sTb->tagSize;
        sTb->tagDifficulty=getenv("BT4_TAG_TEST_DIFFICULTY")?atoi(getenv("BT4_TAG_TEST_DIFFICULTY")):2;
        Tb_SetMode(TB_TAG);
    }
    if (getenv("BT4_TAG_DRAFT_TEST") != NULL && !getenv("BT4_TAG_CREATE_TEST") && !getenv("BT4_TAG_VIDEO_TEST") && !getenv("BT4_TAG_JOIN_UI_TEST")) {
        Port_TagCPUOptions(getenv("BT4_TAG_CPU_TEST") != NULL, getenv("BT4_TAG_TEST_DIFFICULTY")?atoi(getenv("BT4_TAG_TEST_DIFFICULTY")):2);
        if (Port_TagConnect(getenv("BT4_TAG_SERVER"))) {
            const char *size = getenv("BT4_TAG_TEST_SIZE");
            const char *join = getenv("BT4_TAG_DRAFT_JOIN");
            Port_TagCPUOptions(getenv("BT4_TAG_CPU_TEST") != NULL, getenv("BT4_TAG_TEST_DIFFICULTY")?atoi(getenv("BT4_TAG_TEST_DIFFICULTY")):2);
            if (join) Port_TagJoin(join, Port_NetNameGet(), 0x123456789ABCDEF0ULL);
            else Port_TagOpen(size ? atoi(size) : 2, 4, 2, Port_NetNameGet(), 0x123456789ABCDEF0ULL);
            Tb_SetMode(TB_TAG_ROOM);
        }
    }
    if(getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_VIDEO_TEST")){
        sTb->hubCur=HUB_TAG;sTb->hubY=Tb_HubRowY(HUB_TAG);sTb->tagLeft=sTb->tagRight=2;sTb->tagCPU=1;sTb->tagDifficulty=3;sTb->tagCur=6;
        if(getenv("BT4_TAG_LONG_VIDEO")){sTb->tagLeft=getenv("BT4_TAG_TEST_LEFT")?atoi(getenv("BT4_TAG_TEST_LEFT")):5;sTb->tagRight=getenv("BT4_TAG_TEST_RIGHT")?atoi(getenv("BT4_TAG_TEST_RIGHT")):5;sTb->tagAttacks=1;}
    }
    if(!returnedTest&&getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_JOIN_UI_TEST")){sTb->tagJoin=1;sTb->tagCur=7;Tb_SetMode(TB_TAG);}
    ColorFade_StartIn(0, 0, 0, 0x14);
    while (1) {
        s32 live;

        Job_Run();
        Gfx_BeginFrame();
        Pad_Update();
        if (Port_EscTake() && !sTb->typing) {
            gPad[0].gamePressed |= PADG_TRIANGLE;
        }
        Snd_Update();
        ColorFade_Update();
        live = !sTb->leaving && ColorFade_IsInDone();
        if(getenv("BT4_RENDER_TEST")&&getenv("BT4_TBO_TEXT_CAPTURE")&&sTb->frames==90){
            extern void Port_TagUiCapture(int);Port_TagUiCapture(1);
            fprintf(stderr,"tag typography preview: page=%d menu-ready=1\n",sTb->mode);
        }
        if (getenv("BT4_TAG_CREATE_TEST") && sTb->mode==TB_TAG && live) {
            gPad[0].gamePressed |= PADG_CROSS;
        }
        if(getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_CHAT_PREVIEW")&&sTb->mode==TB_TAG_ROOM&&live&&!sTb->tagChatTyping)
            gPad[0].gamePressed=PADG_CIRCLE;
        if(getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_LOBBY_PREVIEW")&&sTb->mode==TB_TAG_ROOM&&live&&Port_TagPhase()==1){
            static s32 previewTick;
            if(++previewTick==60){extern void Port_TagCapture(void);Port_TagCapture();
                fprintf(stderr,"tag text preview: room=1 chat=%d slots=%d\n",sTb->tagChatTyping,Port_TagSlots());}
        }
        if(getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_VIDEO_TEST")&&live){
            static s32 videoMode=-1,videoTick;
            if(videoMode!=sTb->mode){videoMode=sTb->mode;videoTick=0;fprintf(stderr,"tag video: online menu page=%d\n",videoMode);}
            ++videoTick;
            if(sTb->mode==TB_HUB&&videoTick==100)gPad[0].gamePressed=PADG_CROSS;
            if(sTb->mode==TB_TAG&&videoTick>=60&&videoTick%18==0){
                if(sTb->tagCur!=6)gPad[0].gameRepeat=PADG_DOWN;
                else if(videoTick>=216)gPad[0].gamePressed=PADG_CROSS;
            }
            if(sTb->mode==TB_TAG_ROOM&&videoTick==140)gPad[0].gamePressed=PADG_SQUARE;
        }
        if(getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_JOIN_UI_TEST")&&live){
            if(sTb->mode==TB_TAG)gPad[0].gamePressed=PADG_CROSS;
            else if(sTb->mode==TB_JOIN){const char *code=getenv("BT4_TAG_DRAFT_JOIN");s32 j,k;
                if(sTb->codeLen<6&&getenv("BT4_TAG_JOIN_PASTE_TEST")){
                    static s32 pasted;
                    if(!pasted){sTb->keyRow=KEY_ROWS;sTb->keyCol=3;gPad[0].gamePressed=PADG_CROSS;pasted=1;}
                }else if(sTb->codeLen<6){for(j=0;j<KEY_ROWS;++j)for(k=0;k<KEY_COLS;++k)if(code&&sTbKeys[j][k]==code[sTb->codeLen]){sTb->keyRow=j;sTb->keyCol=k;}
                    gPad[0].gamePressed=PADG_CROSS;
                }else{gPad[0].gamePressed=PADG_START;fprintf(stderr,"tag join fixture: original keypad submitted code %s\n",sTb->code);}
            }
        }
        Ps_PromptsClear();
        if (getenv("BT4_RENDER_TBO") == NULL) switch (sTb->mode) {
        case TB_HUB:
            Tb_HubUpdate(live);
            break;
        case TB_TAG:
            Tb_TagUpdate(live); break;
        case TB_TAG_ROOM:
            Tb_TagRoomUpdate(live); break;
        case TB_CREATE:
            Tb_CreateUpdate(live);
            break;
        case TB_JOIN:
            Tb_JoinUpdate(live);
            break;
        case TB_WAIT:
            Tb_WaitUpdate(live);
            break;
        case TB_PROFILE:
            Tb_ProfileUpdate(live);
            break;
        case TB_NAME:
            Tb_NameUpdate(live);
            break;
        case TB_QUICK:
            Tb_QuickUpdate(live);
            break;
        case TB_SPAR:
            Tb_SparUpdate(live);
            break;
        case TB_BLOCK:
            Tb_BlockUpdate(live);
            break;
        }
        Tb_Speak();
        Ps_PageSet();
        Tb_Backdrop();
        IconWin_Draw(); /* the clouds in the corners, the emblem, the line under the title */
        Tb_Draw();
        Tb_DrawText();
        Ps_PromptsDraw();
        ColorFade_Draw();
        File_Stub264D90();
        Gfx_EndFrame(1);
        Dma_Flush();
        sTb->frames++;
        if(returnedTest&&sTb->frames==60){extern void Port_TagUiCapture(int);Port_TagUiCapture(9);}
        if(returnedTest&&sTb->frames==65){fprintf(stderr,"tag return verified: online-hub=%d room-phase=%d\n",sTb->mode==TB_HUB,Port_TagPhase());fflush(NULL);exit(sTb->mode==TB_HUB?0:3);}
        sTb->modeT++;
        if (sTb->note > 0) {
            sTb->note--;
        }
        if (sTb->leaving && ColorFade_IsOutDone()) {
            break;
        }
    }
    sceGsSyncPath(0, 0);
    Port_EscQuit(1);
    Port_TextMode(0);
    Tb_WinTerm();
    if (sTb->train != 2) Port_TagClose();
    if (!sTb->train && (sTb->mode == TB_WAIT || sTb->mode == TB_SPAR)) {
        Port_LobbyCancel();
    }
    train = sTb->train;
    if (train == 2) { Heap_Free(sTb); sTb = NULL; Dma_ResetBuffers(); return 2; }
    if (train) {
        static u8 noItems[0x10]; /* an empty set of Z items */
        s32 me = sTb->spMe;
        s32 foe = sTb->spFoe;

        Heap_Free(sTb);
        sTb = NULL;
        Dma_ResetBuffers();
        Battle_ClearWork();
        BattleSetup_SetRule(0, 6, 8, 0, 1, TB_TOURNAMENT_STAGE, 0); /* training: no time limit, a partner who stands */
        BattleSetup_SetSide(0, 0, 0, 1, 1, 1, 0, NULL);
        BattleSetup_SetSide(1, 2, 1, 1, 1, 1, 0, NULL);
        BattleSetup_SetMember(0, 0, me, 0, 0, -1, 100.0f, noItems);
        BattleSetup_SetMember(1, 0, foe, 0, 0, -1, 100.0f, noItems);
        BattleSetup_Finish();
        gPortTboSpar = 1;
        return 1;
    }
    Heap_Free(sTb);
    sTb = NULL;
    Dma_ResetBuffers();
    return train;
}
/* The sign over the sparring fight (battle/hud_a.c, Hud_Draw, every frame): how long it has searched, what for, and
   how to go back. Nothing when the fight is not Train While Waiting's or the search is over. */
static s32 sTbSignFrames;

void PortTbo_SparSign(void) {
    static char line[64];
    s32 secs;

    if (!gPortTboSpar || !Port_LobbyInBackground()) {
        return;
    }
    sTbSignFrames++;
    secs = (sTbBgFrames + sTbSignFrames) / 60;
    Ps_PageSet();
    Ps_Panel(TBO_KIT_DIALOG, 128.0f, 66.0f, 384.0f, 124.0f, 0x70); /* (as much room under the lines as over them) */
    sprintf(line, "Searching for a rival...  %d:%02d", (int)(secs / 60), (int)(secs % 60));
    Ps_Msg(146.0f, 72.0f, line, 1.0f, 0x80);
    if (sTbBgKind == 2) {
        Ps_Copy(line, "Quick Match: ", 64);
        Ps_Cat(line, sTbBgQueue ? "Teams" : "One on One", 64);
    } else if (sTbBgKind == 1) {
        Ps_Copy(line, "Your room: ", 64);
        Ps_Cat(line, Port_LobbyCode(), 64);
    } else {
        Ps_Copy(line, "Joining room ", 64);
        Ps_Cat(line, sTbBgCode, 64);
    }
    Ps_Msg(146.0f, 93.0f, line, 1.0f, 0x80);
}
#endif
