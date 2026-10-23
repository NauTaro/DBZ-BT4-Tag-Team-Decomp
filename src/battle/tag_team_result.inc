static s32 sTagResultActive,sTagResultCursor,sTagResultFrames,sTagResultWinner,sTagResultSent;
static s32 sTagResultPending,sTagResultStickTimer;
static u32 sTagResultStickLast,sTagResultStickHeld;
extern int Port_TagResultChoice(void),Port_TagHost(void),Port_TagAllEnded(void);
extern int Port_TagPhase(void);
extern void Port_TagClose(void);
extern void Port_TagCommand(int,int,int);
static void TagOnline_Ending(void);
void TagBattle_ResultBegin(void){
    s32 flags=Battle_GetResult()->winner;
    sTagResultWinner=(flags&BTL_RESULT_WIN_P1)?1:(flags&BTL_RESULT_WIN_P2)?2:0;
    sTagResultCursor=0;sTagResultFrames=0;sTagResultSent=0;sTagResultActive=1;
    sTagResultPending=-1;sTagResultStickTimer=0;sTagResultStickLast=sTagResultStickHeld=0;
    if(TagBattle_Network()){
        if(!Port_TagHost()&&getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_RESULT_READY_DELAY")){
            s32 i;extern void Port_TagYield(void);for(i=0;i<250;++i)Port_TagYield();
            fprintf(stderr,"tag result fixture: delayed participant readiness\n");
        }
        Port_TagCommand(21,0,0); /* result barrier, distinct from voice ticket 20 */
    }
    {extern void DemoCam_Reset(void);DemoCam_Reset();}
    if(getenv("BT4_TAG_RESULT_TEST"))printf("tag result: winner=%d opened\n",sTagResultWinner);
}
s32 TagBattle_ResultUpdate(void){
    const char *test=getenv("BT4_TAG_RESULT_TEST");
    s32 network=TagBattle_Network(),host=!network||Port_TagHost();
    s32 confirm;
    ++sTagResultFrames;
    if(getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_LONG_VIDEO")&&sTagResultFrames<300)return 0;
    if(sTagResultFrames<30)return 0;
    if(test && test[0]!='h' && sTagResultFrames%15==0){
        s32 wanted=atoi(test);
        if(sTagResultCursor!=wanted)gPad[0].gameRepeat=PADG_DOWN;
        else gPad[0].gamePressed=PADG_CROSS;
    }
    confirm=(gPad[0].gamePressed&(PADG_CROSS|PADG_START))!=0;
    if(host&&sTagResultPending<0){
        u32 stick=gPad[0].gameLeft[1]>0.5f?PADG_DOWN:gPad[0].gameLeft[1]<-0.5f?PADG_UP:0;
        u32 repeat=Pad_CalcRepeat(stick,stick&~sTagResultStickHeld,&sTagResultStickTimer,&sTagResultStickLast,20,4);
        s32 was=sTagResultCursor;
        sTagResultStickHeld=stick;
        Ps_Move(&sTagResultCursor,3);
        if(was==sTagResultCursor){
            if(repeat&PADG_DOWN)sTagResultCursor=(sTagResultCursor+1)%3;
            else if(repeat&PADG_UP)sTagResultCursor=(sTagResultCursor+2)%3;
        }
    }
    if(network){
        s32 choice=Port_TagResultChoice();
        if(choice<0){
            /* Leaving is a local action, including when a peer never reaches
             * the results barrier. Shared round transitions remain host-owned. */
            if((confirm&&(!host||sTagResultCursor==2))||Port_TagPhase()==0||Port_TagPhase()==6){
                sTagResultCursor=2;TagOnline_Ending();Port_TagClose();
            }else{
                if(host&&confirm&&sTagResultPending<0)sTagResultPending=sTagResultCursor;
                if(!sTagResultSent&&(gPad[0].gamePressed&PADG_TRIANGLE))sTagResultPending=-1;
                if(host&&sTagResultPending>=0&&Port_TagAllEnded()&&!sTagResultSent){
                    Port_TagCommand(18,sTagResultPending,0);sTagResultSent=1;
                }
                return 0;
            }
        }else{sTagResultCursor=choice;TagOnline_Ending();}
    }else if(!confirm)return 0;
    if(sTagResultCursor==0)BattleResult_Set(BTL_RESULT_ABORT,0x8000); /* rematch, without replay bit 0x10000 */
    else {TagBattle_SetReturn(sTagResultCursor);BattleResult_Set(BTL_RESULT_ABORT,0);}
    if(test)printf("tag result: choice=%d confirmed\n",sTagResultCursor);
    sTagResultActive=0;
    return 1;
}
void TagBattle_ResultDraw(void){
    static char *choices[3]={"Rematch","Character Select","Main Menu"};
    s32 i;
    if(!sTagResultActive)return;
    Ps_PageSet();Ps_SetShift(0);Ps_Align(0);
    Ps_Rect(Ps_Left(),0,512-Ps_Left(),448,Ps_Rgba(1,8,17,0x60),Ps_Rgba(1,8,17,0x70));
    Ps_Panel(TBO_KIT_WINDOW,100,106,412,334,0x80);
    Ps_Align(1);Ps_Heading(256,124,sTagResultWinner==1?"TEAM 1 WINS":sTagResultWinner==2?"TEAM 2 WINS":"DRAW",1.15f,280,0x80);
    s32 guest=TagBattle_Network()&&!Port_TagHost();
    Ps_Text(256,169,guest?"HOST CHOOSES THE NEXT ROUND":sTagResultPending>=0?"WAITING FOR PLAYERS...":"TAG TEAM | MATCH COMPLETE",0.48f,280,3);Ps_Align(0);
    for(i=0;i<(guest?1:3);++i){f32 y=guest?237:202+i*35;
        Ps_Pill(i==sTagResultCursor?TBO_KIT_SEL:TBO_KIT_BAR,125,y,387,y+28,0x80);
        Ps_Align(1);Ps_Text(256,Ps_TextTop(y+14,0.65f),guest?"Leave Room":choices[i],0.65f,236,i==sTagResultCursor?1:0);Ps_Align(0);
    }
    Ps_PromptsClear();if(!guest)Ps_Prompt(3,"Choose");Ps_Prompt(0,"Confirm / Start");if(sTagResultPending>=0&&!sTagResultSent)Ps_Prompt(2,"Cancel");Ps_PromptsDraw();
}
