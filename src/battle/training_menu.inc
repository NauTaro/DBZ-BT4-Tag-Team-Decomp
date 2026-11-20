/* Native training pause pages. Recordings are bounded to four 30-second slots. */
#include "battle/training.h"
#include "battle/btl_input.h"

extern int Port_NetActive(void);
extern s32 BtlSeq_GetState(void);
extern void *BtlChar_Get(s32 actor);
extern s32 *BtlMember_GetActiveGauge(void *);
extern s32 BtlAct_GetCurrent(void *);
extern s32 BtlAct_GetSkillAction(void *,s32);
extern const char *PortOnline_Text(const char *);
extern s32 Port_TrLine(const char *,char *,s32);
extern s32 MenuTranslation_Ensure(void);

static const char *Training_L(const char *original){
    static char translated[8][192];
    static s32 next;
    const char *text=PortOnline_Text(original);
    next=(next+1)&7;
    return Port_TrLine(text,translated[next],sizeof(translated[next]))?translated[next]:text;
}

enum { TRAIN_ROOT=0,TRAIN_RESOURCES,TRAIN_OPPONENT,TRAIN_RECORDING,TRAIN_NETWORK,TRAIN_PAGES };
enum { TRAIN_NATIVE=0,TRAIN_IDLE,TRAIN_GUARD,TRAIN_MELEE,TRAIN_SUPER1,TRAIN_SUPER2,
       TRAIN_SKILL1,TRAIN_SKILL2,TRAIN_COUNTER,TRAIN_POST_GUARD,TRAIN_ESCAPE,TRAIN_ESCAPE2,TRAIN_BEHAVIORS };
enum { TRAIN_OFF=0,TRAIN_RECORD,TRAIN_PLAY };
typedef struct TrainingFrame {u32 buttons;u8 stick[2];} TrainingFrame;
typedef struct TrainingResources {s32 percent[3],freeze[3],pending[3];} TrainingResources;
static struct {
    BtlMenu menus[TRAIN_PAGES];
    BtlMenuItem items[TRAIN_PAGES][8];
    TrainingResources resources[2];
    TrainingFrame recordings[4][900],delay[9];
    s32 lengths[4],actor,behavior,slot,recording,cursor,manual,frames,delayFrames,jitter,delayCursor;
    s32 previousHealth[2],hit[2],guardTicks[2],escapeTicks;
} sTraining;

s32 Training_Active(void){
    extern s32 Port_TagOfflineTraining(void);
    return Battle_GetMode()==6&&!Port_NetActive()&&
           (!TagBattle_Active()||Port_TagOfflineTraining())&&!BattleReplay_IsActive();
}
static void Training_ClearDelay(void){
    s32 i;
    memset(sTraining.delay,0,sizeof(sTraining.delay));
    for(i=0;i<9;++i)sTraining.delay[i].stick[0]=sTraining.delay[i].stick[1]=BTL_STICK_CENTER;
    sTraining.delayCursor=0;
}
static void Training_SwapControl(void){
    sTraining.manual=Training_ControlledActor()==0;
    sTraining.recording=TRAIN_OFF;
    sTraining.escapeTicks=0;
    sTraining.guardTicks[1]=0;
    Training_ClearDelay();
}
static s32 Training_Choose(BtlMenuEvent *ev){
    s32 row=ev->id&15,page=(ev->id-0x7200)/16,k;
    TrainingResources *resources=&sTraining.resources[sTraining.actor];
    if(!ev->selected||!Training_Active())return 1;
    switch(page){
    case TRAIN_RESOURCES:
        if(row==0)sTraining.actor^=1;
        else{
            k=(row-1)/2;
            if(row&1){
                resources->percent[k]=resources->percent[k]<0?0:resources->percent[k]>=100?-1:resources->percent[k]+5;
                resources->pending[k]=resources->percent[k]>=0;
            }else resources->freeze[k]^=1;
        }
        break;
    case TRAIN_OPPONENT:
        if(row==0){
            sTraining.behavior=(sTraining.behavior+1)%TRAIN_BEHAVIORS;
            sTraining.escapeTicks=sTraining.guardTicks[1]=0;
        }else Training_SwapControl();
        break;
    case TRAIN_RECORDING:
        sTraining.recording=TRAIN_OFF;sTraining.cursor=0;
        if(row==0)sTraining.slot=(sTraining.slot+1)%4;
        if(row==1){sTraining.lengths[sTraining.slot]=0;sTraining.recording=TRAIN_RECORD;}
        if(row==2&&sTraining.lengths[sTraining.slot])sTraining.recording=TRAIN_PLAY;
        if(row==4)sTraining.lengths[sTraining.slot]=0;
        Training_ClearDelay();
        break;
    case TRAIN_NETWORK:
        if(row==0)sTraining.delayFrames=(sTraining.delayFrames+1)%9;
        else sTraining.jitter^=1;
        Training_ClearDelay();
        break;
    }
    return 1;
}

void Training_Init(void){
    static const s32 counts[TRAIN_PAGES]={5,8,3,6,3};
    s32 page,row,actor,k;
    memset(&sTraining,0,sizeof(sTraining));
    if(Training_Active())MenuTranslation_Ensure();
    for(actor=0;actor<2;++actor)for(k=0;k<3;++k)sTraining.resources[actor].percent[k]=-1;
    Training_ClearDelay();
    for(page=0;page<TRAIN_PAGES;++page){
        BtlMenu *menu=&sTraining.menus[page];
        menu->items=sTraining.items[page];menu->count=counts[page];menu->scale=1.0f;
        menu->padX=14;menu->padY=12;menu->open=-1;
        for(row=0;row<counts[page];++row){
            BtlMenuItem *item=&sTraining.items[page][row];
            item->id=0x7200+page*16+row;item->text[0]=item->text[1]=item->id;
            item->type=BTL_MENU_ITEM_VALUE;item->func=Training_Choose;
        }
        sTraining.items[page][counts[page]-1].type=BTL_MENU_ITEM_BACK;
        sTraining.items[page][counts[page]-1].func=NULL;
    }
    for(row=0;row<4;++row){
        sTraining.items[TRAIN_ROOT][row].sub=&sTraining.menus[row+1];
        sTraining.items[TRAIN_ROOT][row].func=NULL;
    }
}
BtlMenu *Training_Menu(void){return &sTraining.menus[TRAIN_ROOT];}
static const char *Training_BehaviorName(void){
    static const char *names[TRAIN_BEHAVIORS]={"Normal","Stand still","Guard","Melee only","Super attack 1",
        "Super attack 2","Blast 1","Blast 2","Z Counter","Guard after hit","Blast 1 after hit","Blast 2 after hit"};
    return Training_L(names[sTraining.behavior]);
}
u16 *Training_Text(s32 id){
    /* BtlMenu_Draw gathers every row's pointer before drawing any of them. */
    static u16 lines[0x51][160];
    u16 *output;
    char line[256];
    const char *text="";
    s32 page=(id-0x7200)/16,row=id&15,i=0,k;
    TrainingResources *r=&sTraining.resources[sTraining.actor];
    if(id<0x7200||id>0x7250)return NULL;
    output=lines[id-0x7200];
    if(id==0x7250)text=Training_L("Training Settings");
    else if(page<0||page>=TRAIN_PAGES)return NULL;
    else if(row==sTraining.menus[page].count-1)text=Training_L("Back");
    else if(page==TRAIN_ROOT){
        static const char *names[]={"Resources","Opponent","Record and Playback","Input Delay Simulator"};
        text=Training_L(names[row]);
    }else if(page==TRAIN_RESOURCES){
        static const char *names[]={"Health","Ki","Blast Stocks"};
        if(!row)snprintf(line,sizeof(line),"%s  %s",Training_L("Fighter"),Training_L(sTraining.actor?"Opponent":"Player"));
        else{
            k=(row-1)/2;
            if(!(row&1))snprintf(line,sizeof(line),"%s %s  %s",Training_L("Freeze"),Training_L(names[k]),Training_L(r->freeze[k]?"On":"Off"));
            else if(r->percent[k]<0)snprintf(line,sizeof(line),"%s  %s",Training_L(names[k]),Training_L("Normal"));
            else snprintf(line,sizeof(line),"%s  %d%%",Training_L(names[k]),r->percent[k]);
        }
        text=line;
    }else if(page==TRAIN_OPPONENT){
        if(!row)snprintf(line,sizeof(line),"%s  %s",Training_L("Behavior"),Training_BehaviorName());
        else snprintf(line,sizeof(line),"%s  %s",Training_L("Control"),Training_L(sTraining.manual?"Opponent":"Player"));
        text=line;
    }else if(page==TRAIN_RECORDING){
        static const char *names[]={"Slot","Record","Play loop","Stop","Clear slot"};
        if(!row)snprintf(line,sizeof(line),"%s %d  %d/900",Training_L(names[row]),sTraining.slot+1,sTraining.lengths[sTraining.slot]);
        else snprintf(line,sizeof(line),"%s%s",Training_L(names[row]),
            (row==1&&sTraining.recording==TRAIN_RECORD)||(row==2&&sTraining.recording==TRAIN_PLAY)?" *":"");
        text=line;
    }else if(page==TRAIN_NETWORK){
        if(!row)snprintf(line,sizeof(line),"%s  %d",Training_L("Input Delay"),sTraining.delayFrames);
        else snprintf(line,sizeof(line),"%s  %s",Training_L("Jitter"),Training_L(sTraining.jitter?"On":"Off"));
        text=line;
    }
    /* Translation strings use UTF-8, the native menu consumes UTF-16. */
    while(*text&&i<159){
        u32 c=(u8)*text++;
        if(c>=0xC2&&c<0xE0&&((u8)*text&0xC0)==0x80){c=((c&31)<<6)|((u8)*text++&63);}
        else if(c>=0xE0&&c<0xF0&&text[0]&&text[1]&&((u8)text[0]&0xC0)==0x80&&((u8)text[1]&0xC0)==0x80){
            c=((c&15)<<12)|(((u8)text[0]&63)<<6)|((u8)text[1]&63);text+=2;
        }
        output[i++]=(u16)c;
    }
    output[i]=0;return output;
}
s32 Training_ControlledActor(void){
    return Training_Active()?(sTraining.recording==TRAIN_RECORD||sTraining.manual?1:0):-1;
}
void Training_Restart(void){
    s32 actor,k;
    if(!Training_Active())return;
    Training_ClearDelay();sTraining.cursor=sTraining.frames=0;
    memset(sTraining.previousHealth,0,sizeof(sTraining.previousHealth));
    memset(sTraining.hit,0,sizeof(sTraining.hit));
    memset(sTraining.guardTicks,0,sizeof(sTraining.guardTicks));
    sTraining.escapeTicks=0;
    if(sTraining.recording==TRAIN_RECORD)sTraining.lengths[sTraining.slot]=0;
    for(actor=0;actor<2;++actor)for(k=0;k<3;++k)
        sTraining.resources[actor].pending[k]=sTraining.resources[actor].percent[k]>=0;
}
void Training_Tick(void){
    static const s32 values[3]={0,3,5},maxima[3]={1,4,6};
    s32 actor,k;
    if(!Training_Active()||BtlSeq_GetState()!=3||(Battle_GetWork()->flags&BATTLE_FLAG_PAUSE))return;
    ++sTraining.frames;
    if((gPad[0].gameHeld&(PADG_L1|PADG_R1))==(PADG_L1|PADG_R1)&&(gPad[0].gamePressed&PADG_SELECT)){
        Battle_GetWork()->flags|=BATTLE_FLAG_RESTART;
        return;
    }
    if((gPad[0].gameHeld&(PADG_L2|PADG_R2))==(PADG_L2|PADG_R2)&&(gPad[0].gamePressed&PADG_SELECT))
        Training_SwapControl();
    for(actor=0;actor<2;++actor){
        s32 *g=BtlMember_GetActiveGauge(BtlChar_Get(actor));
        TrainingResources *r=&sTraining.resources[actor];
        if(!g||g[1]<=0)continue;
        sTraining.hit[actor]=sTraining.previousHealth[actor]>g[0];
        for(k=0;k<3;++k)if(r->pending[k]||r->freeze[k]){
            s32 value=(s32)((s64)g[maxima[k]]*(r->percent[k]<0?100:r->percent[k])/100);
            if(!k&&value<1)value=1;
            g[values[k]]=value;r->pending[k]=0;
        }
        sTraining.previousHealth[actor]=g[0];
    }
}
void Training_FilterInput(s32 actor,u32 *buttons,u8 stick[2]){
    TrainingFrame frame;
    s32 manual=Training_ControlledActor(),tick=sTraining.frames;
    const s32 recording=sTraining.recording;
    if(manual<0||actor<0||actor>1)return;
    if(actor!=manual&&!(sTraining.recording==TRAIN_PLAY&&actor==1)){
        if(!actor||sTraining.behavior!=TRAIN_NATIVE){
            *buttons=0;stick[0]=stick[1]=BTL_STICK_CENTER;
        }
        if(actor){
            const s32 hit=sTraining.hit[actor];
            switch(sTraining.behavior){
            case TRAIN_GUARD:*buttons=BTLB_GUARD|BTLB_ANY_FACE;break;
            case TRAIN_MELEE:if(!(tick%4))*buttons=BTLB_RUSH;break;
            case TRAIN_SUPER1:if(!(tick%90))*buttons=BTLB_CHARGE|BTLB_BLAST;break;
            case TRAIN_SUPER2:if(!(tick%90))*buttons=BTLB_CHARGE|BTLB_BLAST|BTLB_UP;break;
            case TRAIN_SKILL1:if(!(tick%90))*buttons=BTLB_CHARGE|BTLB_GUARD;break;
            case TRAIN_SKILL2:if(!(tick%90))*buttons=BTLB_CHARGE|BTLB_GUARD|BTLB_UP;break;
            case TRAIN_COUNTER:if(!(tick%4))*buttons=BTLB_UP|BTLB_RUSH;break;
            case TRAIN_POST_GUARD:
                if(hit)sTraining.guardTicks[actor]=30;
                if(sTraining.guardTicks[actor]>0){*buttons=BTLB_GUARD|BTLB_ANY_FACE;--sTraining.guardTicks[actor];}
                break;
            case TRAIN_ESCAPE:
            case TRAIN_ESCAPE2:{
                const s32 slot=sTraining.behavior==TRAIN_ESCAPE2;
                void *fighter=BtlChar_Get(actor);
                const s32 action=BtlAct_GetSkillAction(fighter,slot);
                if(BtlAct_GetCurrent(fighter)==action)sTraining.escapeTicks=0;
                else if(hit&&!sTraining.escapeTicks)sTraining.escapeTicks=12;
                if(sTraining.escapeTicks>0){
                    if(!(sTraining.escapeTicks&1))*buttons=BTLB_CHARGE|BTLB_GUARD|(slot?BTLB_UP:0);
                    --sTraining.escapeTicks;
                }
                break;
            }
            }
        }
    }
    if(*buttons&BTLB_BLAST)*buttons|=BTLB_BLAST2;
    if(actor==1&&sTraining.recording==TRAIN_RECORD){
        TrainingFrame *record=&sTraining.recordings[sTraining.slot][sTraining.cursor++];
        record->buttons=*buttons;memcpy(record->stick,stick,2);
        sTraining.lengths[sTraining.slot]=sTraining.cursor;
        if(sTraining.cursor>=900){sTraining.recording=TRAIN_OFF;sTraining.cursor=0;}
    }else if(actor==1&&sTraining.recording==TRAIN_PLAY){
        const s32 count=sTraining.lengths[sTraining.slot];
        if(count){
            frame=sTraining.recordings[sTraining.slot][sTraining.cursor];
            sTraining.cursor=(sTraining.cursor+1)%count;
            *buttons=frame.buttons;memcpy(stick,frame.stick,2);
        }
    }
    if(actor==manual&&recording!=TRAIN_RECORD){
        s32 delay=sTraining.delayFrames;
        if(sTraining.jitter&&delay>0)delay+=(tick/15%3)-1;
        if(delay>8)delay=8;
        frame.buttons=*buttons;memcpy(frame.stick,stick,2);
        sTraining.delay[sTraining.delayCursor]=frame;
        frame=sTraining.delay[(sTraining.delayCursor+9-delay)%9];
        sTraining.delayCursor=(sTraining.delayCursor+1)%9;
        *buttons=frame.buttons;memcpy(stick,frame.stick,2);
    }
}
