/* Dev only. Reuse BT4's own gauge sprites. Bounded to ten independent actors;
 * shared texture resources remain owned by the normal HUD. No game RNG draws. */
static HudBWork *sTagGauge[10];

void HudGauge_TagTerm(void) {
    extern void HudGauge_Term(void);
    HudBWork *normal=gHudGauge;
    s32 i;
    for(i=0;i<10;++i)if(sTagGauge[i]){
        gHudGauge=sTagGauge[i];HudGauge_Term();sTagGauge[i]=NULL;
    }
    gHudGauge=normal;
}

void HudGauge_TagDraw(s32 actor,f32 x,f32 y,f32 sx,f32 sy,s32 small,s32 mirror) {
    extern void *BtlChar_Get(s32);
    extern HudBObj *BtlChar_GetObj(void *);
    extern s32 BtlCharApi_GetHp(s32),BtlCharApi_GetHpMax(s32),BtlSide_GetKi(s32),BtlSide_GetBlast(s32),BtlSide_GetMaxPower(s32);
    extern void HudNode_Update(HudBGroup *),HudNode_Draw(HudBGroup *,s32),HudGfx_ClearAlpha(void),Hud_ClearTexMarks(void);
    extern void Vu0Cur_Push(void),Vu0Cur_Pop(void),Vu0Cur_MulMtx(f32 *);
    HudBWork *normal=gHudGauge,*work;
    void *chr;
    HudBObj *obj;
    HudBGroup *root;
    f32 matrix[16]={0};
    s32 fresh,side=0,hp;
    if(actor<0||actor>=10)return;
    chr=BtlChar_Get(actor);if(!chr)return;obj=BtlChar_GetObj(chr);
    fresh=sTagGauge[actor]==NULL;
    if(fresh){
        HudGauge_Init(&root,normal->res);
        sTagGauge[actor]=gHudGauge;
        /* Every property is read from this actor. Cosmetic sparks must not
         * advance the simulation's RNG once for every extra teammate. */
        gHudGauge->grp[10].update=NULL;
        gHudGauge->grp[10].draw=NULL;
        gHudGauge->grp[10].sprCount=0;
    }else gHudGauge=sTagGauge[actor];
    work=gHudGauge;sTagHudActor=actor;sTagHudMirror=mirror!=0;
    HudGauge_SelectSide(0);
    hp=BtlCharApi_GetHp(actor);
    if(hp<0)hp=0;
    HudGauge_SetHp(side,hp);
    HudGauge_SetKi(side,BtlSide_GetKi(actor));
    HudGauge_SetBlast(side,BtlSide_GetBlast(actor));
    HudGauge_SetMaxPower(side,BtlSide_GetMaxPower(actor));
    HudGauge_SetFighter(side,obj);
    if(!small){
        extern s32 Battle_IsSplitScreen(void);
        s32 source=Battle_IsSplitScreen()&&actor==1?1:0;
        HudGauge_SetStatIcons(0,normal->statMask[source],normal->statPos[source]);
        HudGauge_SetKiReserve(0,normal->kiReserve[source]);
    }
    if(fresh){work->hpShown[0]=hp;work->kiShown[0]=work->ki[0];}
    if(hp==0){work->hpShown[0]=0;work->ki[0]=work->kiShown[0]=0;}
    HudNode_Update(work->grp);
    HudNode_Show(&work->grp[11],obj&&obj->faceRes);
    HudNode_Show(&work->grp[2],BtlCharApi_GetHpMax(actor)<=70000);
    /* Keep health, portrait and reserves in the mini gauge. Ki remains a
     * compact native row; status icons and blast numbers belong to the player. */
    HudNode_Show(&work->grp[8],!small);
    HudNode_Show(&work->grp[9],!small);
    /* The native node mirror uses the entire 512-pixel screen. This compact
     * card owns its transform: mirror around its supplied outer edge instead.
     * Native face/digit UV mirrors still keep their lettering readable. */
    work->grp->flags&=~2;
    matrix[0]=mirror?-sx:sx;matrix[5]=sy;matrix[10]=matrix[15]=1;
    matrix[12]=x;matrix[13]=y;
    Vu0Cur_Push();Vu0Cur_MulMtx(matrix);
    /* Face uploads reuse native VRAM addresses. Re-admit the shared sheet
     * after each face rather than using the preceding actor's palette. */
    Hud_ClearTexMarks();
    HudGfx_ClearAlpha();Port_GsMarker(0x72);Port_GsMarker(0x19);
    HudNode_Draw(work->grp,0);
    Port_GsMarker(0x73);Port_GsMarker(0x13);Vu0Cur_Pop();
    sTagHudActor=-1;sTagHudMirror=0;gHudGauge=normal;
}
