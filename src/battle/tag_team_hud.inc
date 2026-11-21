/* GPL-3.0-or-later. The Tag Team HUD uses one active-fighter card per local
 * viewport. Other fighters are shown as a short roster and a world target. */
#include "battle/hud_a.h"
static u8 sTagBeans[10];
#ifdef BT4_TAG_TEAM_EXPANDED
#include "battle/tag_team_assist.h"
#include "battle/tag_team_revive.h"
#include "battle/tag_team_fusion.h"
#include "battle/tag_team_hud_math.h"
static void TagHud_Assist(s32 actor,f32 x,f32 y);
#endif

typedef struct TagHudFace { u8 pad[0x50]; HudRes *res; } TagHudFace;
extern void *BtlChar_Get(s32 actor);
extern TagHudFace *BtlChar_GetObj(void *chr);
extern s32 BtlCharApi_GetHp(s32 actor),BtlCharApi_GetHpMax(s32 actor),BtlSide_GetKi(s32 actor);
extern void BtlCharApi_GetBasePos(s32 actor,f32 *out);
extern void Vu0Cur_ResetStack(void),Vu0Cur_LoadIdentity(void),HudGfx_SetEnv(void);
extern void HudSprite_InitTex(HudSprite *,HudRes *,s32,s32);
extern void HudSprite_SetRect(HudSprite *,s32,s32,s32,s32);
extern void HudSprite_DrawAt(HudSprite *,HudRes *,s32,s32,s32);
extern void Port_GsMarker(s32 effect);
extern s32 TagBattle_HudHead(s32 actor,f32 *out),TagBattle_HudProject(f32 *head,f32 *out);
extern void TagBattle_HudRear(f32 *head,f32 *out);
extern s32 BtlSeq_IsFighting(void),BtlSeq_GetTimeLeft(void),Port_TagOfflineBeans(void);
extern f32 sqrtf(f32 x);
static u32 TagHud_Color(s32 r,s32 g,s32 b) { return Ps_Rgba(r,g,b,0x80); }
static void TagHud_Stroke(f32 x0,f32 y0,f32 x1,f32 y1,f32 width,u32 c) {
    f32 dx=x1-x0,dy=y1-y0,n=sqrtf(dx*dx+dy*dy),px,py;
    if(n<0.001f)return;
    px=-dy*width*0.5f/n;py=dx*width*0.5f/n;
    Ps_Quad(x0+px,y0+py,x1+px,y1+py,x0-px,y0-py,x1-px,y1-py,c,c,c,c);
}
static void TagHud_Label(f32 x,f32 y,char *s,f32 scale,f32 width,u32 color) {
    Ps_TextExOuter(x,y,s,scale,width,color,color,TagHud_Color(3,10,17),Ps_Rgba(0,0,0,0x50),0.35f);
}
static void TagHud_Arrow(f32 x,f32 y,f32 dx,f32 dy,f32 size,u32 color) {
    f32 n,px,py;
    dx/=PS_UNIT_X;n=sqrtf(dx*dx+dy*dy);
    if(n<0.001f){dx=0;dy=1;n=1;}
    dx/=n;dy/=n;px=-dy;py=dx;
    Ps_Quad(x+dx*size*PS_UNIT_X,y+dy*size,x+(px*0.65f-dx*0.65f)*size*PS_UNIT_X,y+(py*0.65f-dy*0.65f)*size,
        x+(-px*0.65f-dx*0.65f)*size*PS_UNIT_X,y+(-py*0.65f-dy*0.65f)*size,x+(-px*0.65f-dx*0.65f)*size*PS_UNIT_X,y+(-py*0.65f-dy*0.65f)*size,color,color,color,color);
}
static void TagHud_Portrait(s32 actor,f32 x,f32 y,f32 w,f32 h) {
    /* The HUD indexes independent actor slots, never the temporary side
     * aliases used while another fighter's cinematic is being rendered. */
    TagHudFace *obj=BtlChar_GetObj(BtlChar_Get(actor));
    HudSprite spr;
    HudRes *res;
    if(!obj || !obj->res)return;
    res=obj->res;
    if(res->texCount<1 || !res->tex)return;
    spr.flags=0;
    /* Portraits share the original upload addresses. Force each queued upload,
     * then leave the resource unmarked for the next main-gauge draw. */
    res->tex[0].mark=0;
    HudGfx_SetEnv();Port_GsMarker(0x19); /* page coordinates already account for aspect */
    HudSprite_InitTex(&spr,res,0,0);
    HudSprite_SetRect(&spr,(s32)Ps_X(x),(s32)Ps_X(x+w),(s32)Ps_Y(y),(s32)Ps_Y(y+h));
    if(BtlCharApi_GetHp(actor)<=0){spr.r=0x35;spr.g=0x35;spr.b=0x35;spr.a=0x65;}
    HudSprite_DrawAt(&spr,res,0,0x2C00,0x2CD0);
    res->tex[0].mark=0;
    Port_GsMarker(0x13);
}

static void TagHud_Fill(f32 x,f32 y,f32 width,f32 height,f32 value,u32 color){
    u32 track=Ps_Rgba(72,91,102,0x55);
    if(value>1)value=1;
    Ps_Rect(x,y,x+width,y+height,track,track);
    if(value>0)Ps_Rect(x,y,x+width*value,y+height,color,color);
}

#ifdef BT4_TAG_TEAM_EXPANDED
/* Small controller glyphs belong to the cue, rather than a literal command
 * embedded in the sentence. Stick and shoulder caps share the native outline. */
static void TagHud_Key(f32 x,f32 y,const char *key,s32 stick,u32 color){
#ifdef PORT
    extern int PortInput_XboxGlyphs(int player);
    if (PortInput_XboxGlyphs(0) && key[0] && key[1] && !key[2]) {
        if (key[0]=='L') key=key[1]=='1'?"LB":key[1]=='2'?"LT":key[1]=='3'?"LS":key;
        if (key[0]=='R') key=key[1]=='1'?"RB":key[1]=='2'?"RT":key[1]=='3'?"RS":key;
    }
#endif
    u32 ink=Ps_Rgba(8,19,28,0x78);
    if(stick){
        for(s32 i=0;i<16;++i){
            extern f32 sinf(f32),cosf(f32);
            f32 a=i*.39269908f,b=(i+1)*.39269908f;
            TagHud_Stroke(x+cosf(a)*7*PS_UNIT_X,y+sinf(a)*7,
                x+cosf(b)*7*PS_UNIT_X,y+sinf(b)*7,.8f,color);
        }
        Ps_Rect(x-1,y+8,x+1,y+10,color,color);
    }else{
        Ps_Quad(x-8,y-7,x+7,y-7,x-7,y+7,x+8,y+7,ink,ink,ink,ink);
        TagHud_Stroke(x-8,y-7,x+7,y-7,.7f,color);
        TagHud_Stroke(x-7,y+7,x+8,y+7,.7f,color);
    }
    Ps_Align(1);TagHud_Label(x,Ps_TextTop(y,.32f),(char*)key,.32f,14,color);Ps_Align(0);
}
static void TagHud_Assist(s32 actor,f32 x,f32 y){
    extern const char *PortOnline_Text(const char *);
    s32 partner=-1,ticks=0,total=0,links=0;
    s32 fusion=TagFusion_Hud(actor,&partner,&ticks,&total),state=0,relay=0,key=0;
    const char *label=NULL;
    char detail[64]="";
    u32 gold=TagHud_Color(255,208,94),blue=TagHud_Color(105,230,251),white=TagHud_Color(237,248,253);
    if(!BtlSeq_IsFighting())return;
    if(fusion){
        label="Fusion";
        if(fusion==1||fusion==5)label="Waiting for fusion";
        else if(fusion==2){label="Accept fusion";key=1;}
        else{
            s32 controller=TagFusion_Controller(actor);
            if(BattleSide_GetControl(controller)==BTL_CONTROL_CPU)Ps_Copy(detail,"CPU",sizeof(detail));
            else{
                extern s32 Port_TagActorName(s32,char *,s32);
                if(!TagBattle_Network()||!Port_TagActorName(controller,detail,40))
                    snprintf(detail,sizeof(detail),"P%d",BattleSide_GetPad(TagFusion_ViewActor(actor))+1);
            }
            if(total>0){s32 n=strlen(detail);snprintf(detail+n,sizeof(detail)-n,"   %d s",(ticks+29)/30);}
        }
    }else if((relay=TagRelay_Hud(actor,&partner,&links,&ticks))!=0){
        label=relay==4?"No compatible beam":relay==5?"Not enough Ki":relay==3?"Combined finish":"Rush relay";
        if(relay==1)key=3;
    }else{
        state=TagAssist_Hud(actor,&partner,&ticks);
        if(!state||state==4)return;
        label=state==1?"Joint attack":state==2?"Waiting for ally":"Join attack";
        key=state==1?2:state==3?1:0;
    }
    u32 ink=Ps_Rgba(7,19,29,0x64),clear=Ps_Rgba(7,19,29,0);
    Ps_Quad(x,y,x+146,y,x,y+34,x+138,y+34,ink,clear,ink,clear);
    Ps_Rect(x,y,x+1,y+32,relay?gold:blue,relay?gold:blue);
    if(partner>=0&&partner<TagBattle_Count())TagHud_Portrait(partner,x+5,y+3,21,27);
    else{
        TagHud_Stroke(x+8,y+10,x+20,y+22,1.5f,gold);
        TagHud_Stroke(x+20,y+10,x+8,y+22,1.5f,gold);
    }
    TagHud_Label(x+31,Ps_TextTop(y+8,.56f),(char*)PortOnline_Text(label),.56f,key?78:108,white);
    if(key==1)TagHud_Key(x+133,y+10,"R3",1,blue);
    else if(key==2){TagHud_Key(x+115,y+10,"L2",0,blue);TagHud_Key(x+135,y+10,"R3",1,blue);}
    else if(key==3)Ps_Button(0,x+133,y+10,13);
    if(relay){
        for(s32 i=0;i<3;++i){
            f32 bx=x+31+i*31;
            u32 c=i<links?gold:Ps_Rgba(93,133,147,0x50);
            Ps_Quad(bx,y+18,bx+27,y+18,bx+3,y+24,bx+30,y+24,c,c,c,c);
        }
        f32 ratio=(f32)ticks/(relay>=3?180:90);if(ratio>1)ratio=1;
        Ps_Rect(x+31,y+28,x+124,y+29,Ps_Rgba(32,51,61,0x60),Ps_Rgba(32,51,61,0x60));
        Ps_Rect(x+31,y+28,x+31+93*ratio,y+29,blue,blue);
    }else if(fusion&&detail[0]){
        TagHud_Label(x+31,Ps_TextTop(y+23,.35f),detail,.35f,100,gold);
        if(total>0)TagHud_Fill(x+31,y+30,101,2,(f32)ticks/total,gold);
    }else if(ticks>0)TagHud_Fill(x+31,y+24,101,2,(f32)ticks/90,blue);
}
#endif

static f32 TagHud_Hp(s32 actor){
    s32 maximum=BtlCharApi_GetHpMax(actor);
    return maximum>0?(f32)BtlCharApi_GetHp(actor)/maximum:0;
}

static void TagHud_Card(s32 actor,f32 x,f32 y,char *label){
    char beans[12];
    const s32 showHealth = TagBattle_HealthRule(actor) != NULL;
    const f32 height = showHealth ? 50.0f : 43.0f;
    u32 blue=TagHud_Color(74,229,248),gold=TagHud_Color(255,209,112);
    u32 top=Ps_Rgba(11,27,44,0x75),bottom=Ps_Rgba(7,17,27,0x65);
    f32 health=TagHud_Hp(actor);
    Ps_Quad(x,y,x+128,y,x,y+height,x+119,y+height,top,top,bottom,bottom);
    Ps_Rect(x,y,x+1.5f,y+height,blue,blue);
    TagHud_Portrait(actor,x+5,y+5,27,32);
    TagHud_Label(x+37,y+5,label,.50f,78,TagHud_Color(241,248,253));
    TagHud_Fill(x+37,y+23,76,5,health,health>.25f?blue:TagHud_Color(250,103,94));
    TagHud_Fill(x+37,y+32,64,2.5f,(f32)BtlSide_GetKi(actor)/100000,gold);
    if (showHealth) {
        char hp[24];
        snprintf(hp, sizeof(hp), "%d / %d", BtlCharApi_GetHp(actor), BtlCharApi_GetHpMax(actor));
        Ps_Align(2);
        TagHud_Label(x+113, y+37, hp, .35f, 76, TagHud_Color(199,224,236));
        Ps_Align(0);
    }
    if(Port_TagOfflineBeans()&&!TagBattle_Spectating()){
        snprintf(beans,sizeof(beans),"S %u",(unsigned)sTagBeans[actor]);
        Ps_Align(2);TagHud_Label(x+112,y+7,beans,.36f,24,TagHud_Color(175,238,148));Ps_Align(0);
    }
}

static void TagHud_Allies(s32 own,s32 count,f32 x,f32 y,s32 mirror){
    s32 actor,slot=0;
    u32 blue=TagHud_Color(74,229,248),bg=Ps_Rgba(9,22,35,0x65);
    for(actor=0;actor<count;++actor){
        f32 xx;
        if(actor==own||!TagBattle_Allied(actor,own))continue;
        xx=x+slot++*(mirror?-27:27);
        Ps_Rect(xx,y,xx+24,y+29,bg,bg);
        TagHud_Portrait(actor,xx+2,y+2,20,21);
        TagHud_Fill(xx+2,y+25,20,2,TagHud_Hp(actor),blue);
    }
}

static void TagHud_Clock(s32 view,s32 count,s32 split){
    char time[16];
    s32 actor,pips=0,seconds=BtlSeq_GetTimeLeft();
    f32 x=split?256:512-Ps_Left()-35;
    u32 bg=Ps_Rgba(8,19,32,0x70),gold=TagHud_Color(255,211,90),white=TagHud_Color(236,247,251);
    if(seconds<0)Ps_Copy(time,"--:--",sizeof(time));
    else snprintf(time,sizeof(time),"%02d:%02d",seconds/60,seconds%60);
    Ps_Quad(x-23,11,x+23,11,x-20,35,x+20,35,bg,bg,bg,bg);
    Ps_Align(1);TagHud_Label(x,13,time,.68f,44,white);Ps_Align(0);
    if(split)return;
    for(actor=0;actor<count;++actor){
        f32 xx;
        u32 color;
        if(TagBattle_Allied(actor,view))continue;
        color=BtlCharApi_GetHp(actor)>0?gold:TagHud_Color(51,66,81);
        xx=x-16+pips++*7;
        Ps_Rect(xx,39,xx+4,41,color,color);
    }
}

static void TagHud_Target(s32 viewer,s32 target){
    static const f32 ring[32][2]={{1,0},{.9808f,.1951f},{.9239f,.3827f},{.8315f,.5556f},
        {.7071f,.7071f},{.5556f,.8315f},{.3827f,.9239f},{.1951f,.9808f},{0,1},
        {-.1951f,.9808f},{-.3827f,.9239f},{-.5556f,.8315f},{-.7071f,.7071f},
        {-.8315f,.5556f},{-.9239f,.3827f},{-.9808f,.1951f},{-1,0},
        {-.9808f,-.1951f},{-.9239f,-.3827f},{-.8315f,-.5556f},{-.7071f,-.7071f},
        {-.5556f,-.8315f},{-.3827f,-.9239f},{-.1951f,-.9808f},{0,-1},
        {.1951f,-.9808f},{.3827f,-.9239f},{.5556f,-.8315f},{.7071f,-.7071f},
        {.8315f,-.5556f},{.9239f,-.3827f},{.9808f,-.1951f}};
    f32 head[4],body[4],screen[3],tip[3],x,y,radius,left=Ps_Left(),right=512-left;
    u32 gold=TagHud_Color(255,214,100),shadow=Ps_Rgba(16,23,33,0x75);
    s32 i,front,ally=0;
#ifdef BT4_TAG_TEAM_EXPANDED
    extern s32 TagBattle_Focus(s32);
    s32 focus=TagBattle_Focus(viewer);
    if(focus==-2)return;
    if(focus>=0){target=focus;ally=1;gold=TagHud_Color(113,228,255);}
#else
    (void)viewer;
#endif
    if(target<0||(!ally&&BtlCharApi_GetHp(target)<=0)||!TagBattle_HudHead(target,head))return;
    BtlCharApi_GetBasePos(BattleSide_GetObjId(target),body);
    for(i=0;i<3;++i)body[i]+=(head[i]-body[i])*.56f;
    front=TagBattle_HudProject(body,screen);
    x=front?left+screen[0]*(512-2*left)/512:256;
    y=front?screen[1]:224;
    if(!front||x<left||x>right||y<0||y>448){
        f32 dx=x-256,dy=y-224,ax,ay,k;
        if(!front){f32 rear[2];TagBattle_HudRear(head,rear);dx=rear[0]*PS_UNIT_X;dy=rear[1];}
        ax=dx<0?-dx:dx;ay=dy<0?-dy:dy;
        k=ax/(right-270);if(ay/154>k)k=ay/154;
        if(k>0.001f)TagHud_Arrow(256+dx/k,224+dy/k,dx,dy,6,gold);
        return;
    }
    radius=TagBattle_HudProject(head,tip)?y-tip[1]+6:16;
    if(radius<10)radius=10;
    if(radius>32)radius=32;
    for(i=0;i<32;++i){
        s32 next=(i+1)%32;
        TagHud_Stroke(x+ring[i][0]*radius*PS_UNIT_X,y+ring[i][1]*radius,
            x+ring[next][0]*radius*PS_UNIT_X,y+ring[next][1]*radius,1.45f,gold);
        TagHud_Stroke(x+ring[i][0]*(radius+3)*PS_UNIT_X,y+ring[i][1]*(radius+3),
            x+ring[next][0]*(radius+3)*PS_UNIT_X,y+ring[next][1]*(radius+3),.55f,gold);
    }
    /* The bar stays above the reticle and uses the model height instead
     * of an animated bone, so flinches and attacks do not kick it around. */
    {
        extern f32 BtlCharApi_GetHeight(s32);
        f32 anchor[4], projected[3];
        s32 obj=BattleSide_GetObjId(target);
        BtlCharApi_GetBasePos(obj,anchor);
        anchor[1]-=BtlCharApi_GetHeight(obj);
        y-=radius+9;
        if(TagBattle_HudProject(anchor,projected)){
            x=left+projected[0]*(512-2*left)/512;
            y=projected[1]-9;
        }
        if(y<23)y=23;
        if(x<left+65)x=left+65;
        if(x>right-65)x=right-65;
    }
    Ps_Rect(x-30,y-1,x+30,y+5,shadow,shadow);
    TagHud_Fill(x-29,y,58,3,TagHud_Hp(target),TagHud_Color(116,235,83));
#ifdef BT4_TAG_TEAM_EXPANDED
    {
        extern s32 Port_TagActorName(s32,char *,s32);
        char name[80];
        if(Port_TagActorName(target,name,sizeof(name))){
            Ps_Align(1);TagHud_Label(x,y-15,name,.48f,116,TagHud_Color(245,248,255));Ps_Align(0);
        }
    }
#endif
}

#ifdef BT4_TAG_TEAM_EXPANDED
static void TagHud_NativeGauge(s32 actor,f32 x,f32 y,f32 scale,s32 small,s32 mirror){
    extern void HudGauge_TagDraw(s32,f32,f32,f32,f32,s32,s32);
    char hp[32];
    HudGauge_TagDraw(actor,Ps_X(x),Ps_Y(y),scale*(Ps_X(1)-Ps_X(0))*PS_UNIT_X,
        scale*(Ps_Y(1)-Ps_Y(0)),small,mirror);
    Port_GsMarker(0x72);
    if(!small&&BtlCharApi_GetHpMax(actor)>70000){
        snprintf(hp,sizeof(hp),"%d / %d",BtlCharApi_GetHp(actor),BtlCharApi_GetHpMax(actor));
        if(mirror)Ps_Align(2);
        TagHud_Label(x+(mirror?-58:58)*scale,y+68*scale,hp,small?.31f:.42f,140*scale,TagHud_Color(238,248,251));
        snprintf(hp,sizeof(hp),"%d",(BtlCharApi_GetHp(actor)+9999)/10000);
        TagHud_Label(x+(mirror?-63:63)*scale,y,hp,small?.31f:.42f,30,TagHud_Color(238,248,251));
        Ps_Align(0);
    }
    if(BtlCharApi_GetHp(actor)<=0){if(mirror)Ps_Align(2);TagHud_Label(x+(mirror?-5:5),y+10,"KO",.58f,30,TagHud_Color(255,110,84));Ps_Align(0);}
    Port_GsMarker(0x73);Port_GsMarker(0x13);
}
static void TagHud_NativeAllies(s32 view,s32 count,f32 left,s32 mirror){
    s32 actor,n=0,index=0;
    for(actor=0;actor<count;++actor)if(actor!=view&&!TagFusion_Absent(actor)&&TagBattle_Allied(actor,view))++n;
    for(actor=0;actor<count;++actor)if(actor!=view&&!TagFusion_Absent(actor)&&TagBattle_Allied(actor,view)){
        TagHud_NativeGauge(actor,mirror?512-Ps_Left()-8:left+8,400-(n-1-index++)*39,.48f,1,mirror);
    }
}
static void TagHud_FloatingAllies(s32 pane,s32 cameraActor,s32 count){
    extern s32 TagBattle_HudAllyProject(s32,s32,f32 *),Port_TagOfflineMode(void);
    extern void Port_TagAllyHudFeed(s32,s32,s32,s32,s32);
    s32 teamActor=Port_TagOfflineMode()==2?cameraActor:TagBattle_ViewActor();
    s32 actor,shown=0;
    f32 placed[8],left=Ps_Left(),right=512-left;
    const f32 paneLeft=pane?256:left,paneRight=pane?right:256;
    for(actor=0;actor<count;++actor){
        f32 screen[3],point[2],x;
        if(actor==teamActor || actor==cameraActor || !TagBattle_Allied(actor,teamActor) ||
           TagFusion_Absent(actor) || BtlCharApi_GetHp(actor)<=0)continue;
        if(!TagBattle_HudAllyProject(pane,actor,screen))continue;
        x=left+screen[0]*(512-2*left)/512;
        if(!ThAllyPlate(x,screen[1],paneLeft,paneRight,placed,shown,point))continue;
        placed[shown*2]=point[0];placed[shown*2+1]=point[1];++shown;
        Port_TagAllyHudFeed(pane,cameraActor,actor,(s32)((point[0]-left)*100000/(512-2*left)),(s32)(point[1]*100000/448));
        Port_GsMarker(0x50000000u|((u32)actor<<24)|((u32)pane<<23)|
            ((u32)((point[0]-left)*4095/(512-2*left))<<11)|(u32)(point[1]*2047/448));
        TagHud_NativeGauge(actor,point[0]-28,point[1],.28f,1,0);
    }
}
static void TagHud_Radar(s32 view,s32 count,f32 right){
    extern s32 TagBattle_HudCamera(f32 *);
    f32 own[4],p[4],basis[4],cx=right-39,cy=43,rad=29;
    s32 i,k;
    u32 grid=Ps_Rgba(109,205,122,0x28),rim=TagHud_Color(221,244,218);
    BtlCharApi_GetBasePos(BattleSide_GetObjId(view),own);
    if(!TagBattle_HudCamera(basis))return;
    for(i=0;i<32;++i){
        extern f32 sinf(f32),cosf(f32);
        f32 a=i*6.2831853f/32,b=(i+1)*6.2831853f/32;
        f32 ax=cosf(a),ay=sinf(a),bx=cosf(b),by=sinf(b);
        Ps_Quad(cx,cy,cx+ax*rad*PS_UNIT_X,cy+ay*rad,cx+bx*rad*PS_UNIT_X,cy+by*rad,
            cx+bx*rad*PS_UNIT_X,cy+by*rad,Ps_Rgba(4,37,17,0x65),Ps_Rgba(4,37,17,0x65),Ps_Rgba(4,37,17,0x65),Ps_Rgba(4,37,17,0x65));
        TagHud_Stroke(cx+ax*rad*PS_UNIT_X,cy+ay*rad,cx+bx*rad*PS_UNIT_X,cy+by*rad,.8f,rim);
    }
    for(i=-2;i<=2;++i){
        f32 off=i*9,span=sqrtf(rad*rad-off*off);
        TagHud_Stroke(cx-span*PS_UNIT_X,cy+off,cx+span*PS_UNIT_X,cy+off,.4f,grid);
        TagHud_Stroke(cx+off*PS_UNIT_X,cy-span,cx+off*PS_UNIT_X,cy+span,.4f,grid);
    }
    s32 marked=TagBattle_Target(view);
#ifdef BT4_TAG_TEAM_EXPANDED
    extern s32 TagBattle_Focus(s32);
    const s32 focus=TagBattle_Focus(view);
    if(focus!=-1)marked=focus;
#endif
    for(i=0;i<count;++i){
        f32 dx=0,dy=0,len;
        u32 col;
        if(BtlCharApi_GetHp(i)<=0)continue;
        BtlCharApi_GetBasePos(BattleSide_GetObjId(i),p);
        dx=(p[0]-own[0])*basis[0]+(p[2]-own[2])*basis[1];
        dy=-((p[0]-own[0])*basis[2]+(p[2]-own[2])*basis[3]);
        dx*=.045f;dy*=.045f;len=sqrtf(dx*dx+dy*dy);
        if(len>rad-3){dx*=(rad-3)/len;dy*=(rad-3)/len;}
        col=i==view?TagHud_Color(245,249,224):TagBattle_Allied(i,view)?TagHud_Color(69,211,246):TagHud_Color(249,104,78);
        if(i==marked)col=TagBattle_Allied(i,view)?TagHud_Color(170,244,255):TagHud_Color(255,223,80);
        TagHud_Arrow(cx+dx*PS_UNIT_X,cy+dy,0,-1,i==view?2.7f:2.1f,col);
    }
}
static void TagHud_DevLayout(s32 view,s32 target,s32 count,s32 split){
    f32 left=Ps_Left(),right=512-left;
    if(split){
        extern const char *PortOnline_Text(const char *);
        for (s32 player = 0; player < 2; ++player) {
            const s32 actor = TagBattle_SplitViewActor(player);
            const f32 edge = player ? 256 : left;
            TagHud_NativeGauge(actor,player?512-left-8:edge+8,8,.60f,0,player!=0);
            if(BtlSeq_IsFighting())TagHud_FloatingAllies(player,actor,count);
            if (!BtlSeq_IsFighting()) continue;
            if (TagBattle_SplitSpectating(player)) {
                char label[100];
                s32 progress, total, helpers;
                const char *key = actor == player ? "Right stick: rotate camera" : "Watching an ally";
                Ps_Copy(label, (char *)PortOnline_Text(key), sizeof(label));
                TagHud_Label(edge + 8, 104, label, .43f, 190, TagHud_Color(238, 248, 251));
                TagHud_Key(edge+15,123,"L1",0,TagHud_Color(217,244,255));
                TagHud_Key(edge+35,123,"R1",0,TagHud_Color(217,244,255));
                Ps_Copy(label, (char *)PortOnline_Text("Switch fighter"), sizeof(label));
                TagHud_Label(edge+49,118,label,.40f,145,TagHud_Color(217,244,255));
                if (TagRevive_Hud(player, &progress, &total, &helpers) && total > 0) {
                    Ps_Copy(label, (char *)PortOnline_Text("Revive"), sizeof(label));
                    TagHud_Label(edge + 8, 134, label, .40f, 76, TagHud_Color(195, 255, 211));
                    TagHud_Fill(edge + 85, 139, 64, 3, (f32)progress / total, TagHud_Color(84, 237, 139));
                }
            } else {
                TagHud_Assist(player, edge + 8, 244);
            }
        }
        if(gHud->flags&HUD_SHOW_TIMER){
            char time[16];s32 seconds=BtlSeq_GetTimeLeft();
            if(seconds<0)Ps_Copy(time,"\xE2\x88\x9E",sizeof(time));else snprintf(time,sizeof(time),"%d",seconds);
            Port_GsMarker(0x74);
            Ps_Align(1);TagHud_Label(256,8,time,.92f,60,TagHud_Color(217,244,255));Ps_Align(0);
            Port_GsMarker(0x75);
        }
        return;
    }
    TagHud_NativeGauge(view,left+8,8,.86f,0,0);
    TagHud_NativeAllies(view,count,left,0);
    TagHud_Radar(view,count,right);
    if(gHud->flags&HUD_SHOW_TIMER){
        char time[16];s32 seconds=BtlSeq_GetTimeLeft();
        if(seconds<0)Ps_Copy(time,"\xE2\x88\x9E",sizeof(time));else snprintf(time,sizeof(time),"%d",seconds);
        Ps_Align(2);TagHud_Label(right-75,9,time,1.03f,55,TagHud_Color(217,244,255));Ps_Align(0);
    }
    if(BtlSeq_IsFighting())TagHud_Target(view,target);
    if(!TagBattle_Spectating() || TagFusion_Absent(TagBattle_LocalActor()))TagHud_Assist(view,left+8,244);
}
/* Project the cylinder from the fallen actor's world coordinates in the active
 * native camera. Its size follows the fighter, not the output resolution. */
static void TagHud_Revival(s32 view,s32 count){
    extern f32 Mathf_Sin(f32),Mathf_Cos(f32),BtlCharApi_GetHeight(s32);
    extern const char *PortOnline_Text(const char *);
    s32 actor,progress,total,helpers,k;
    for(actor=0;actor<count;++actor){
        f32 p[4],head[4],screen[4],height,radius;u32 color;
        if(!TagBattle_Allied(view,actor)||!TagRevive_Hud(actor,&progress,&total,&helpers))continue;
        BtlCharApi_GetBasePos(BattleSide_GetObjId(actor),p);
        height=BtlCharApi_GetHeight(BattleSide_GetObjId(actor))*.55f;
        if(height<6)height=6;
        if(height>30)height=30;
        radius=TagRevive_Radius();color=Ps_Rgba(81,245,137,TagRevive_Opacity()*128/100/2);
        for(k=0;k<24;++k){
            f32 a=k*(6.2831853f/24),b=(k+1)*(6.2831853f/24);
            f32 world[4][4],point[4][4];s32 j,visible=1;
            for(j=0;j<4;++j){
                f32 angle=(j&1)?b:a;
                memcpy(world[j],p,sizeof(p));
                world[j][0]+=Mathf_Cos(angle)*radius;world[j][2]+=Mathf_Sin(angle)*radius;
                if(j>=2)world[j][1]-=height;
                if(!TagBattle_HudProject(world[j],point[j]))visible=0;
                point[j][0]=Ps_Left()+point[j][0]*(512-2*Ps_Left())/512;
                if(point[j][0]<Ps_Left()-1024||point[j][0]>1536-Ps_Left()||point[j][1]<-1024||point[j][1]>1472)visible=0;
            }
            if(visible){
                u32 rim=Ps_Rgba(105,252,159,TagRevive_Opacity()*128/100);
                Ps_Quad(point[0][0],point[0][1],point[1][0],point[1][1],
                    point[2][0],point[2][1],point[3][0],point[3][1],color,color,color,color);
                TagHud_Stroke(point[2][0],point[2][1],point[3][0],point[3][1],.75f,rim);
            }
        }
        if(TagBattle_HudHead(actor,head)&&TagBattle_HudProject(head,screen)){
            char label[48];f32 ratio=total>0?(f32)progress/total:0,x,y;
            s32 rate=helpers?3+helpers:4,seconds=(total-progress+30*rate-1)/(30*rate);
            x=Ps_Left()+screen[0]*(512-2*Ps_Left())/512;
            y=screen[1]-18.0f;
            if(x<Ps_Left()+48||x>464-Ps_Left()||y<58||y>430)continue;
            u32 green=TagHud_Color(84,237,139),ink=Ps_Rgba(6,23,17,0x68);
            snprintf(label,sizeof(label),"%s  %ds",PortOnline_Text("Revive"),seconds);
            Ps_Quad(x-47,y-14,x+44,y-14,x-44,y+6,x+41,y+6,ink,ink,ink,ink);
            Ps_Rect(x-40,y-9,x-38,y-1,green,green);Ps_Rect(x-43,y-6,x-35,y-4,green,green);
            Ps_Align(1);TagHud_Label(x+4,y-12,label,.38f,70,TagHud_Color(195,255,211));Ps_Align(0);
            TagHud_Fill(x-30,y+1,66,2.5f,ratio,green);
        }
    }
}
#endif

void TagBattle_DrawRoster(void){
    s32 view,target,count,split;
    f32 left;
#ifdef PORT
    extern void Port_TagAllyHudReset(void);Port_TagAllyHudReset();
    Port_GsMarker(0x60000000u);
#endif
    if(!TagBattle_Active()||!gHud||!(gHud->flags&HUD_SHOW_GAUGES)||
       (Battle_GetWork()->flags&BATTLE_FLAG_LOADING))return;
    split=Battle_IsSplitScreen();
    view=split?0:TagBattle_ViewActor();
#ifdef BT4_TAG_TEAM_EXPANDED
    view=TagFusion_ViewActor(view);
#endif
    target=TagBattle_Target(view);count=TagBattle_Count();
    Ps_PageSet();Ps_SetShift(0);Ps_Align(0);Port_GsMarker(0x13);
    Vu0Cur_ResetStack();Vu0Cur_LoadIdentity();
    left=Ps_Left();
#ifdef BT4_TAG_TEAM_EXPANDED
    TagHud_DevLayout(view,target,count,split);
    if(!split&&BtlSeq_IsFighting())TagHud_Revival(view,count);
    return;
#endif
    TagHud_Card(view,left+10,12,split?"Player 1":TagBattle_Spectating()?"Spectating":"Fighter");
    TagHud_Allies(view,count,left+10,406,0);
#ifdef BT4_TAG_TEAM_EXPANDED
    if(!TagBattle_Spectating())TagHud_Assist(view,left+10,66);
#endif
    if(split){
        TagHud_Card(1,512-left-138,12,"Player 2");
        TagHud_Allies(1,count,512-left-34,406,1);
    }else if(BtlSeq_IsFighting()){
        TagHud_Target(view,target);
        if(TagBattle_Spectating())
            TagHud_Label(left+10,386,"L1 / R1  SWITCH FIGHTER",.43f,170,TagHud_Color(236,247,251));
    }
    if(gHud->flags&HUD_SHOW_TIMER)TagHud_Clock(view,count,split);
}
