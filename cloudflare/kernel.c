/* Same validated C room state machine as the native client. No game assets. */
#include "../port/include/tag_team_wire.h"
#include <string.h>
#include <stdarg.h>
void *memset(void *p,int c,unsigned long n){unsigned char *d=p;while(n--)*d++=(unsigned char)c;return p;}
void *memcpy(void *p,const void *s,unsigned long n){unsigned char*d=p;const unsigned char*b=s;while(n--)*d++=*b++;return p;}
int memcmp(const void *a,const void *b,unsigned long n){const unsigned char*x=a,*y=b;while(n--){if(*x!=*y)return *x-*y;++x;++y;}return 0;}
int snprintf(char *dst,unsigned long cap,const char *fmt,...){
    va_list a;unsigned n=0;va_start(a,fmt);
    while(*fmt){char buf[24];const char*s=buf;
        if(*fmt!='%'){buf[0]=*fmt++;buf[1]=0;}
        else{int pad=0;++fmt;if(*fmt=='0'){++fmt;pad=*fmt++-'0';}
            if(*fmt=='s'){s=va_arg(a,const char*);++fmt;}
            else{unsigned v=va_arg(a,unsigned),j=0,k;do{buf[j++]=(char)('0'+v%10);v/=10;}while(v);while((int)j<pad)buf[j++]='0';
                for(k=0;k<j/2;++k){char t=buf[k];buf[k]=buf[j-1-k];buf[j-1-k]=t;}buf[j]=0;++fmt;}}
        while(*s){if(n+1<cap)dst[n]=*s;++n;++s;}}
    if(cap)dst[n<cap?n:cap-1]=0;va_end(a);return n;
}
static TtRoom room;static TtInputs frames;
static unsigned char input[TT_WIRE_BYTES],output[TT_WIRE_BYTES],neutral[TT_PAD_BYTES];
static unsigned inactive_mask,last_late_mask;static int paced_input;
#include "sync_kernel.inc"
unsigned char *Kernel_Input(void){return input;}
unsigned char *Kernel_Output(void){return output;}
int Kernel_Phase(void){return room.phase;}
int Kernel_Humans(void){return Tt_Humans(&room);}
int Kernel_Seat(unsigned member){return Tt_Seat(&room,member);}
unsigned Kernel_Epoch(void){return room.epoch;}
unsigned Kernel_Host(void){return room.host;}
void Kernel_Seed(unsigned seed){room.seed=seed;}
void Kernel_Leave(unsigned member){
    int i,seat=Tt_Seat(&room,member);
    if(room.phase!=TT_FIGHT||seat<0){Tt_Leave(&room,member);return;}
    /* Keep the shared fighter and feed it neutral input until this round ends.
     * A vanished connection must not abort the remaining players' battle. */
    inactive_mask|=1u<<seat;room.players[seat].ended=1;++room.revision;
    if(member==room.host){
        for(i=0;i<Tt_Count(&room);++i)if(!room.players[i].cpu&&!(inactive_mask&(1u<<i))){room.host=room.players[i].member;break;}
        if(i==Tt_Count(&room))room.phase=TT_ABORTED;
    }
}
void Kernel_Pacing(int on){paced_input=on!=0;}
unsigned Kernel_PendingFrame(void){return frames.next;}
unsigned Kernel_DeadlineFrame(void){
    int i;if(room.phase!=TT_FIGHT)return 0;last_late_mask=0;
    for(i=0;i<Tt_Count(&room);++i)if(!(frames.ring[frames.next%TT_INPUT_HISTORY].mask&(1u<<i))||frames.ring[frames.next%TT_INPUT_HISTORY].frame!=frames.next){
        Tt_InputPut(&frames,room.epoch,i,frames.next,neutral);
        if(!room.players[i].cpu)last_late_mask|=1u<<i;
    }
    return last_late_mask;
}
void Kernel_Snapshot(unsigned member,int result,const char *code){Ttw_Snapshot(output,&room,member,result,code);}
static unsigned long long read64(unsigned char*p){return Ttw_Read(p)|((unsigned long long)Ttw_Read(p+4)<<32);}
int Kernel_Command(unsigned member){
    int cmd=Ttw_Read(input+4),a=Ttw_Read(input+12),b=Ttw_Read(input+16),rc=TT_BAD_PHASE;
    unsigned epoch=Ttw_Read(input+8);char name[20];
    if(Ttw_Read(input)!=TT_PROTOCOL)return TT_MISMATCH;
    memcpy(name,input+32,19);name[19]=0;
    switch(cmd){
    case TTW_OPEN:
        if(room.phase)return TT_BAD_PHASE;
        if(a<2||a>5||b<0||b>4||Ttw_Read(input+20)>12||Ttw_Read(input+52)>1||Ttw_Read(input+56)>4)return TT_BAD_CHOICE;
        Tt_Open(&room,member,name,a,read64(input+24));room.time=b;room.delay=Ttw_Read(input+20);
        if(Ttw_Read(input+52))Tt_FillCPU(&room,member,room.epoch,Ttw_Read(input+56));return TT_OK;
    case TTW_JOIN:return room.phase==TT_CLOSED?TT_BAD_MEMBER:Tt_Join(&room,member,name,read64(input+24));
    case TTW_PING:return TT_OK;
    case TTW_SELECT:return Tt_BeginSelect(&room,member,epoch);
    case TTW_CANCEL_SELECT:return Tt_Seat(&room,member)<0?TT_BAD_MEMBER:Tt_CancelSelect(&room,room.host,epoch);
    case TTW_CHOOSE:return Tt_ChooseSeat(&room,member,epoch,Ttw_Read(input+20),a,b);
    case TTW_LOAD:
        rc=Tt_BeginLoad(&room,member,epoch,read64(input+24));if(!rc){Tt_InputsReset(&frames,room.epoch,Tt_Count(&room));sync_reset();}return rc;
    case TTW_LOADED:return Tt_Loaded(&room,member,epoch,read64(input+24));
    case TTW_ARM:return Tt_Arm(&room,member,epoch);
    case TTW_ACK:return Tt_ArmAck(&room,member,epoch);
    case TTW_START:
        rc=Tt_Start(&room,member,epoch);
        if(!rc){int f,i;for(f=0;f<room.delay;++f)for(i=0;i<Tt_Count(&room);++i)Tt_InputPut(&frames,room.epoch,i,f,neutral);}
        return rc;
    case TTW_TEAMS:return Tt_SetTeams(&room,member,epoch,a,b);
    case TTW_SLOT:return Tt_SetSlot(&room,member,epoch,a,b,Ttw_Read(input+20));
    case TTW_ARENA:return Tt_SetArena(&room,member,epoch,a,b,Ttw_Read(input+20));
    case 19:return Tt_MovePlayer(&room,member,epoch,a,b);
    case TTW_INPUT:
        if(room.phase!=TT_FIGHT)return TT_BAD_PHASE;
        rc=Tt_InputPut(&frames,epoch,Tt_Seat(&room,member),a,input+16);
        if(paced_input&&epoch==room.epoch&&(unsigned)a<frames.next)return TT_OK;
        if(!rc){int i;for(i=0;i<Tt_Count(&room);++i)if(room.players[i].cpu||(inactive_mask&(1u<<i)))Tt_InputPut(&frames,epoch,i,a,neutral);}
        return rc;
    case TTW_HASH:return sync_sample(member,epoch,(unsigned)a,(unsigned)b);
    case 18: /* The host chooses the next round after the result screen. */
        if(room.phase!=TT_FIGHT||epoch!=room.epoch)return TT_BAD_PHASE;
        if(member!=room.host)return TT_NOT_OWNER;
        if(a<0||a>2)return TT_BAD_CHOICE;
        {int i;for(i=0;i<Tt_Count(&room);++i)if(!room.players[i].ended)return TT_NOT_READY;}
        {int i;room.result_choice=a;++room.epoch;++room.revision;
            for(i=0;i<Tt_Count(&room);++i)if(inactive_mask&(1u<<i)){
                room.players[i].member=TT_CPU_MEMBER|(unsigned)(i+1);room.players[i].cpu=1;room.players[i].difficulty=2;
            }
            inactive_mask=0;
            for(i=0;i<Tt_Count(&room);++i){room.players[i].ended=room.players[i].loaded=room.players[i].armed=0;if(a==1){room.players[i].confirmed=0;room.players[i].chara=-1;}}
            sync_reset();
            room.phase=a==0?TT_LOADING:a==1?TT_SELECT:TT_ABORTED;
            if(a==0)room.load_digest=Tt_LoadDigest(&room);
            Tt_InputsReset(&frames,room.epoch,Tt_Count(&room));return TT_OK;}
    case TTW_RESULT_ENDED:
        if(room.phase!=TT_FIGHT||epoch!=room.epoch)return TT_BAD_PHASE;
        {int i,seat=Tt_Seat(&room,member);if(seat<0)return TT_BAD_MEMBER;
            room.players[seat].ended=1;
            if(member==room.host)for(i=0;i<Tt_Count(&room);++i)if(room.players[i].cpu)room.players[i].ended=1;
            ++room.revision;return TT_OK;}
    }
    return TT_BAD_CHOICE;
}
int Kernel_TakeFrame(void){
    unsigned char pads[TT_MAX_PLAYERS][TT_PAD_BYTES];
    if(Tt_InputTake(&frames,pads)!=TT_OK)return 0;
    Ttw_Request(output,TTW_FRAME,room.epoch);Ttw_Write(output+12,frames.next-1);memcpy(output+16,pads,sizeof(pads));
    Ttw_Write(output+492,paced_input?0x32535442u:0);Ttw_Write(output+496,last_late_mask);return 1;
}
