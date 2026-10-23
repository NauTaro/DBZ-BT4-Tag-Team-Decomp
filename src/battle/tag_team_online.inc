/* Fixed simulation frames. Waiting renders the last completed state and polls
 * transport; it does not advance physics, AI, timers or action scripts. */
#include "battle/btl_input.h"
#include "battle/btl_char_ctl.h"
extern int Port_TagPhase(void),Port_TagNetwork(void),Port_TagHost(void),Port_TagSeat(void),Port_TagDelay(void);
extern int Port_TagAllLoaded(void),Port_TagAllArmed(void),Port_TagTakeFrame(u8 out[10][18]);
extern int Port_TagSendInput(u32 frame,const u8 pad[18]);
extern unsigned Port_TagSeed(void);
extern void Port_TagPoll(void),Port_TagCommand(int,int,int),Port_TagClose(void);
extern const char *Port_TagError(void);
extern void *BtlChar_Get(s32 actor);
extern void BtlCam_SelectView(s32 view),BtlCam_ApplyView(s32 scissor);
static u8 sTagOnlinePads[10][18];
static u32 sTagOnlineFrame;
static u32 sTagOnlinePressed;
static s32 sTagOnlineCaptured;
static s32 sTagOnlineEnding;
static s32 sTagOnlineMenu,sTagOnlineMenuCursor;
extern void Port_TagMenuSet(int,int);
static void TagOnline_MenuDraw(void);
static u32 sTagOnlineWaitStarted;
extern unsigned Port_TagMenuMillis(void);
extern void Port_TagCapture(void);
static void TagOnline_Ending(void){sTagOnlineEnding=1;}
extern void Port_TagHash(u32 frame,u32 hash),Port_TagYield(void);
static s32 sTagOnlineSent,sTagOnlineLoaded,sTagOnlineAck,sTagOnlineArm,sTagOnlineStart;
s32 TagBattle_Network(void){return TagBattle_Active()&&Port_TagNetwork();}
u8 *TagBattle_NetPad(s32 actor){return sTagOnlinePads[actor>=0&&actor<10?actor:0];}
static void TagOnline_Reset(void){
    s32 i;extern void Port_Srand(u32),Rand_SeedByArray(u32 *,s32);extern s32 Port_Rand(void);
    u32 key[4];sTagOnlineCaptured=0;sTagOnlinePressed=0;sTagOnlineEnding=sTagOnlineMenu=sTagOnlineMenuCursor=0;sTagOnlineWaitStarted=0;sTagOnlineFrame=sTagOnlineSent=sTagOnlineLoaded=sTagOnlineAck=sTagOnlineArm=sTagOnlineStart=0;
    memset(sTagOnlinePads,0,sizeof(sTagOnlinePads));for(i=0;i<10;++i)sTagOnlinePads[i][4]=sTagOnlinePads[i][5]=127;
    Port_TagMenuSet(0,0);
    if(!TagBattle_Network())return;
    Port_Srand(Port_TagSeed());for(i=0;i<4;++i)key[i]=Port_Rand();Rand_SeedByArray(key,4);
}
static s32 TagOnline_Tick(void){
    s32 phase,wasMenu=sTagOnlineMenu;Port_TagPoll();phase=Port_TagPhase();
    sTagOnlinePressed|=gPad[0].pressed;
    if(sTagResultActive||sTagOnlineEnding){Port_TagMenuSet(0,0);return 1;}
    if(phase==6||phase==0)return -1;
    if(phase==3){
        if(!sTagOnlineLoaded){Port_TagCommand(6,0,0);sTagOnlineLoaded=1;}
        if(Port_TagHost()&&Port_TagAllLoaded()&&!sTagOnlineArm){Port_TagCommand(7,0,0);sTagOnlineArm=1;}
        return 0;
    }
    if(phase==4){
        if(!sTagOnlineAck){Port_TagCommand(8,0,0);sTagOnlineAck=1;}
        if(Port_TagHost()&&Port_TagAllArmed()&&!sTagOnlineStart){Port_TagCommand(9,0,0);sTagOnlineStart=1;}
        return 0;
    }
    if(phase!=5)return 0;
    if(getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_PAUSE_TEST")&&Port_TagHost()&&sTagOnlineFrame>=650){
        static s32 step;extern void Port_TagUiCapture(int);
        ++step;gPad[0].gamePressed=gPad[0].gameRepeat=0;
        if(step==1||step==70)gPad[0].gamePressed=PADG_START;
        if(step==30)gPad[0].gameRepeat=PADG_DOWN;
        if(step==50)gPad[0].gamePressed=PADG_TRIANGLE;
        if(step==90)gPad[0].gamePressed=PADG_CROSS;
        if(step==12)Port_TagUiCapture(1);
        if(step==35)Port_TagUiCapture(2);
        if(step==60)Port_TagUiCapture(3);
        if(step==100){Port_TagUiCapture(4);fprintf(stderr,"tag pause fixture: START DOWN TRIANGLE START CROSS completed\n");}
    }
    if(gPad[0].gamePressed&PADG_START){sTagOnlineMenu=!sTagOnlineMenu;if(sTagOnlineMenu)sTagOnlineMenuCursor=0;}
    if(sTagOnlineMenu&&(gPad[0].gamePressed&PADG_TRIANGLE))sTagOnlineMenu=0;
    if(sTagOnlineMenu){Ps_Move(&sTagOnlineMenuCursor,2);if(gPad[0].gamePressed&PADG_CROSS){
        if(sTagOnlineMenuCursor){Port_TagClose();return -1;}sTagOnlineMenu=0;}}
    Port_TagMenuSet(sTagOnlineMenu,sTagOnlineMenuCursor);
    if(!sTagOnlineSent){
        u8 packet[18]={0};s32 actor=Port_TagSeat();Pad *pad=&gPad[0];
        BtlInputChr *chr=BtlChar_Get(actor);u32 buttons=BtlInput_RemapButtons(pad->held|sTagOnlinePressed,chr->input.mask);
        /* Input is mapped on its owner's machine; another player's key config
         * cannot reinterpret it. Commands/double taps use shared fighter state. */
        memcpy(packet,&buttons,4);packet[4]=BtlInput_StickToByte(pad->left[0]);packet[5]=BtlInput_StickToByte(pad->left[1]);
        packet[6]=(sTagOnlinePressed&PAD_L3)!=0;packet[7]=pad->status==0||pad->status==1?pad->status:2;
        if(sTagOnlineMenu||wasMenu){memset(packet,0,sizeof(packet));packet[4]=packet[5]=127;}
        if(!sTagOnlineMenu&&!wasMenu&&getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_ONLINE_TEST")){
            u32 n=sTagOnlineFrame,held=0;packet[4]=packet[5]=127;
            if(n%240<70)held=BTLB_DASH|BTLB_UP;
            else if(n%240<140)held=BTLB_RUSH|BTLB_ANY_FACE|BTLB_SQUARE;
            else if(n%240<190)held=BTLB_CHARGE;
            else held=BTLB_BLAST|BTLB_BLAST2|BTLB_TRIANGLE;
            memcpy(packet,&held,4);packet[6]=n%150==0;packet[7]=0;
        }
        if(!Port_TagSendInput(sTagOnlineFrame+Port_TagDelay(),packet))return -1;
        sTagOnlinePressed=0;
        sTagOnlineSent=1;
    }
    if(getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_WAIT_TEST")){
        static s32 waitTick;if(++waitTick%4){if(!sTagOnlineWaitStarted)sTagOnlineWaitStarted=Port_TagMenuMillis();return 0;}
    }
    if(!Port_TagTakeFrame(sTagOnlinePads)){if(!sTagOnlineWaitStarted)sTagOnlineWaitStarted=Port_TagMenuMillis();return 0;}
    ++sTagOnlineFrame;sTagOnlineSent=0;

    /* Opt-in reproduction of the first Hit rush in a real network room.
     * Apply on a shared simulation tick, never from a client's local vblank. */
    if(getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_HIT_REPRO")){
        extern s32 BtlSeq_GetState(void);
        extern s32 *BtlSeq_GetClock(void);
        extern s32 TagBattle_Count(void),TagBattle_SetTarget(s32,s32);
        extern s32 BtlStage_FindZoneNear(s32,Vec4 *);
        extern s32 *BtlMember_GetActiveGauge(void *);
        extern s32 BtlAct_GetTechniqueAction(void *,s32),BtlAct_GetCurrent(void *);
        extern void BtlAct_Request(void *,s32);
        static s32 fired,entered,recovered;
        s32 i,actor=getenv("BT4_TAG_HIT_ACTOR")?atoi(getenv("BT4_TAG_HIT_ACTOR")):0;
        s32 victim=actor^1,slot=atoi(getenv("BT4_TAG_HIT_REPRO"));
        for(i=0;i<TagBattle_Count();++i){
            *(s32 *)((u8 *)BtlChar_Get(i)+0x1278)=0;
            memset(sTagOnlinePads[i],0,18);sTagOnlinePads[i][4]=sTagOnlinePads[i][5]=127;
        }
        if(!fired&&BtlSeq_GetState()==3&&BtlSeq_GetClock()[0]>=30){
            for(i=0;i<TagBattle_Count();++i){
                f32 pos[4]={350.f,-150.f,350.f+i*40.f,1.f},rot[4]={0.f,0.f,0.f,0.f};
                void *chr=BtlChar_Get(i);
                BtlAct_Request(chr,0xB);
                if(i==actor){pos[0]=0.f;pos[2]=-40.f;}
                if(i==victim){pos[0]=0.f;pos[2]=0.f;rot[1]=3.14159265f;}
                BtlChar_Place(chr,(Vec4 *)pos,(Vec4 *)rot,BtlStage_FindZoneNear(-1,(Vec4 *)pos));
            }
            TagBattle_SetTarget(actor,victim);TagBattle_SetTarget(victim,actor);
            {s32 *g=BtlMember_GetActiveGauge(BtlChar_Get(actor));g[3]=g[4];}
            BtlAct_Request(BtlChar_Get(actor),BtlAct_GetTechniqueAction(BtlChar_Get(actor),slot));
            fired=1;fprintf(stderr,"hit online repro: first-request frame=%u actor=%d victim=%d slot=%d\n",sTagOnlineFrame,actor,victim,slot);
        }
        if(fired){
            s32 action=BtlAct_GetCurrent(BtlChar_Get(actor));
            if(action>=0x106&&action<=0x13B)entered=1;
            if(entered&&action<0xFD){
                if(++recovered==120)fprintf(stderr,"hit online repro: recovered frame=%u action=%x\n",sTagOnlineFrame,action);
            }else recovered=0;
        }
    }

    if(sTagOnlineFrame==650 && getenv("BT4_RENDER_TEST") && getenv("BT4_TAG_NET_CUT_TEST")){
        extern s32 *BtlMember_GetActiveGauge(void *);
        extern s32 BtlAct_GetTechniqueAction(void *,s32);
        extern void BtlAct_Request(void *,s32);
        void *chr=BtlChar_Get(0);s32 *gauge=BtlMember_GetActiveGauge(chr);
        gauge[3]=gauge[4];BtlAct_Request(chr,BtlAct_GetTechniqueAction(chr,3));
        fprintf(stderr,"tag net cut fixture: native technique requested at shared frame=650\n");
    }
    sTagOnlineWaitStarted=0;
    {extern void Port_TagWaitClear(void);Port_TagWaitClear();}
    return 1;
}
/* Waiting must not rebuild PS2 draw lists: the effect uploads, palettes and
 * double-buffered DMA work are generated once per simulation frame. Reuse the
 * backend's existing held-frame path, which also processes window events. */
static void TagOnline_WaitDraw(void){
    extern void Port_TagWait(int phase,int menu,int cursor,int show);
    extern void EftVram_GetCursor(s32 *,s32 *);
    extern s32 BtlAiMgr_GetFrame(void);
    s32 image,clut,afterImage,afterClut,ai=BtlAiMgr_GetFrame();
    static s32 checked;
    EftVram_GetCursor(&image,&clut);
    Port_TagWait(Port_TagPhase(),sTagOnlineMenu,sTagOnlineMenuCursor,
        Port_TagPhase()<5 || Port_TagMenuMillis()-sTagOnlineWaitStarted>=750);
    if(getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_RENDER_AUDIT")){
        EftVram_GetCursor(&afterImage,&afterClut);
        if(image!=afterImage||clut!=afterClut||ai!=BtlAiMgr_GetFrame()){
            fprintf(stderr,"tag held-frame audit FAILED: wait modified effect cursors or AI\n");exit(3);
        }
        if(++checked%100==0)fprintf(stderr,"tag held-frame audit: waits=%d no-scene-redraw=1 cursors-stable=1 ai-stable=1\n",checked);
    }
}
static void TagOnline_MenuDraw(void){
    Port_TagMenuSet(TagBattle_Network()&&sTagOnlineMenu&&!sTagResultActive,sTagOnlineMenuCursor);
}
static void TagOnline_Trace(void){
    if(getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_RENDER_SEQUENCE")&&sTagOnlineFrame>=600&&sTagOnlineFrame<660)Port_TagCapture();
    if(!sTagOnlineCaptured&&sTagOnlineFrame>=1050&&getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_ONLINE_CAPTURE")){
        s32 i,safe=1;extern s32 ChrCam_IsCutActive(void *),DemoCam_IsActive(void);
        for(i=0;i<TagBattle_Count();++i)if(ChrCam_IsCutActive(BtlChar_Get(i)))safe=0;
        if(DemoCam_IsActive())safe=0;
        if(safe||sTagOnlineFrame>=1800){Port_TagCapture();sTagOnlineCaptured=1;}
    }
    if(sTagResultActive||sTagOnlineEnding||sTagOnlineFrame%30)return;
    {u32 hash=2166136261u; s32 i,k;extern s32 BtlAct_GetCurrent(void *);
        for(i=0;i<TagBattle_Count();++i){f32 pos[4];u32 words[7];
            BtlCharApi_GetPos(i,pos);memcpy(words,pos,12);words[3]=BtlCharApi_GetHp(i);words[4]=BtlSide_GetKi(i);
            words[5]=BtlAct_GetCurrent(BtlChar_Get(i));words[6]=TagBattle_Target(i);
            for(k=0;k<7;++k){hash^=words[k];hash*=16777619u;}}
        Port_TagHash(sTagOnlineFrame,hash);
        if(!getenv("BT4_RENDER_TEST")||!getenv("BT4_TAG_ONLINE_TEST"))return;
        fprintf(stderr,"tag online frame=%u hash=%08X seat=%d hp0=%d hp1=%d\n",sTagOnlineFrame,hash,Port_TagSeat(),BtlCharApi_GetHp(0),BtlCharApi_GetHp(1));fflush(stderr);
        if(sTagOnlineFrame>=2100){fprintf(stderr,"tag online test completed\n");fflush(NULL);exit(0);}
    }
}
