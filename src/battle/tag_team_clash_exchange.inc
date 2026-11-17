/* Each aerial clash owns its QTE/path data. All storage belongs to the
   deterministic game image and is restored with the normal rollback snapshot. */
static BtlClashC sTagExchange[10];
static s32 sTagExchangeOwner[10],sTagExchangePeer[10],sTagExchangeState[10],sTagExchangeTicks[10];
extern s32 TagBattle_CanInteract(s32,s32),TagBattle_ReservePair(s32,s32);
extern void TagBattle_ReleasePair(s32),BtlAct_Request(BtlFlagChr *,s32);

static s32 TagClash_IsVanish(BtlFlagChr *chr){
    s32 action=BtlAct_GetCurrent(chr);return action==0xFB||action==0xFC;
}
static void TagClash_ResetExchanges(void){
    for(s32 i=0;i<10;++i){
        sTagExchangeOwner[i]=sTagExchangePeer[i]=-1;
        sTagExchangeState[i]=sTagExchangeTicks[i]=0;
    }
}
void *TagClash_GetExchange(s32 actor){
    if(TagBattle_Active()&&actor>=0&&actor<TagBattle_Count()){
        s32 owner=sTagExchangeOwner[actor];
        if(owner>=0&&owner<10)return &sTagExchange[owner];
    }
    return &gBtlChars->clashC;
}
s32 TagClash_ExchangeSide(s32 actor){
    if(TagBattle_Active()&&actor>=0&&actor<TagBattle_Count()&&sTagExchangeOwner[actor]>=0)
        return actor==sTagExchangeOwner[actor]?0:1;
    return actor==0?0:1;
}
s32 TagClash_BeginVanish(s32 a,s32 b){
    if(!TagBattle_Active())return 1;
    s32 count=TagBattle_Count(),owner=a<b?a:b,peer=a<b?b:a;
    if(a<0||b<0||a>=count||b>=count||a==b||TagBattle_Allied(a,b)||!TagBattle_CanInteract(a,b))return 0;
    if(sTagExchangeOwner[a]>=0||sTagExchangeOwner[b]>=0)
        return sTagExchangeOwner[a]==owner&&sTagExchangeOwner[b]==owner&&sTagExchangePeer[owner]==peer;
    if(!TagBattle_ReservePair(a,b))return 0;
    sTagExchangeOwner[a]=sTagExchangeOwner[b]=owner;sTagExchangePeer[owner]=peer;
    sTagExchangeState[owner]=9;sTagExchangeTicks[owner]=0;
    return 1;
}
static void TagClash_ReleaseExchange(s32 owner,s32 abort){
    s32 peer=sTagExchangePeer[owner];
    if(peer<0||peer>=TagBattle_Count())return;
    for(s32 n=0;n<2;++n){
        s32 actor=n?peer:owner;BtlFlagChr *chr=BtlChar_Get(actor);
        if(abort){
            /* Do not manufacture a winner, damage or a new hit reaction. */
            BtlChar_ClearFlag(chr,0xB);BtlChar_ClearFlag(chr,0xE);
            BtlChar_ClearFlagRange(chr,0xC4,0xC8);
            if(TagClash_IsVanish(chr))BtlAct_Request(chr,0xB);
        }
        ChrCam_EndCut(chr);sTagExchangeOwner[actor]=-1;
    }
    TagBattle_ReleasePair(owner);
    sTagExchangePeer[owner]=-1;sTagExchangeState[owner]=sTagExchangeTicks[owner]=0;
}
static void TagClash_UpdateExchanges(void){
    s32 count=TagBattle_Count();
    /* Also cover a forced clash action admitted outside the contact helper. */
    for(s32 i=0;i<count;++i)if(sTagExchangeOwner[i]<0&&TagClash_IsVanish(BtlChar_Get(i))){
        s32 peer=TagBattle_Target(i);
        if(peer>=0&&peer<count&&TagClash_IsVanish(BtlChar_Get(peer))&&TagBattle_Target(peer)==i){
            if(TagClash_BeginVanish(i,peer))continue;
        }
        BtlFlagChr *chr=BtlChar_Get(i);
        BtlChar_ClearFlag(chr,0xB);BtlChar_ClearFlag(chr,0xE);
        BtlChar_ClearFlagRange(chr,0xC4,0xC8);BtlAct_Request(chr,0xB);ChrCam_EndCut(chr);
    }
    for(s32 owner=0;owner<count;++owner){
        s32 peer=sTagExchangePeer[owner],state=sTagExchangeState[owner];
        if(!state||peer<=owner||peer>=count)continue;
        BtlFlagChr *a=BtlChar_Get(owner),*b=BtlChar_Get(peer);
        s32 both=TagClash_IsVanish(a)&&TagClash_IsVanish(b);
        if(state==12){
            if(!TagClash_IsVanish(a)&&!TagClash_IsVanish(b)){TagClash_ReleaseExchange(owner,0);continue;}
        }else if(!both){
            /* The admission flags may precede the action pass by one frame. */
            if(state==9&&sTagExchangeTicks[owner]<6&&
               BtlChar_TestFlag(a,0x62)&&BtlChar_TestFlag(b,0x62)){
                ++sTagExchangeTicks[owner];continue;
            }
            TagClash_ReleaseExchange(owner,1);continue;
        }
        /* Bound a broken sequence without using wall clock time or affecting
           independent fights. Normal seven-exchange QTEs finish much earlier. */
        if(++sTagExchangeTicks[owner]>1200){TagClash_ReleaseExchange(owner,1);continue;}
        if(state==12)continue;
        state=BtlClash_UpdatePairC(state,&sTagExchange[owner],a,b);
        sTagExchangeState[owner]=state?state:12;
    }
}

/* Hidden native regression, entered only by the existing render-test harness. */
static s32 TagClash_RunVanishRegression(void){
    s32 i,ok=1,count=TagBattle_Count();
    if(count!=10)return 0;
    TagClash_Reset();TagBattle_ResetPairs();
    for(i=0;i<count;++i){
        BtlFlagChr *chr=BtlChar_Get(i);
        *(s32 *)((u8 *)chr+0x948)=0xB;
        BtlChar_ClearFlagRange(chr,0xC4,0xC8);
    }
    TagBattle_SetTarget(0,5);TagBattle_SetTarget(5,0);
    TagBattle_SetTarget(6,7);TagBattle_SetTarget(7,6);
    ok &= TagClash_BeginVanish(0,5)&&TagClash_BeginVanish(6,7);
    for(i=0;i<count;++i)if(i==0||i==5||i==6||i==7)
        *(s32 *)((u8 *)BtlChar_Get(i)+0x948)=0xFB;
    TagClash_UpdateExchanges();
    ok &= TagClash_GetExchange(0)==TagClash_GetExchange(5);
    ok &= TagClash_GetExchange(0)!=TagClash_GetExchange(6);
    ok &= TagClash_ExchangeSide(0)==0&&TagClash_ExchangeSide(5)==1&&TagClash_ExchangeSide(6)==0;
    for(i=0;i<count;++i)if(i==0||i==5||i==6||i==7){
        BtlFlagChr *chr=BtlChar_Get(i);*(s32 *)((u8 *)chr+0x948)=0xFC;
        BtlChar_SetFlag(chr,0xC4);chr->unkE58=i==0||i==7?2:-2;
        chr->clashCountB=i==0||i==7?4:1;
    }
    sTagExchange[0].count=sTagExchange[6].count=6;
    TagClash_UpdateExchanges();
    ok &= BtlChar_TestFlag(BtlChar_Get(0),0xC7)&&BtlChar_TestFlag(BtlChar_Get(5),0xC8);
    ok &= BtlChar_TestFlag(BtlChar_Get(7),0xC7)&&BtlChar_TestFlag(BtlChar_Get(6),0xC8);
    ok &= !TagBattle_CanInteract(2,0)&&!TagBattle_ReservePair(2,5);
    ok &= sTagExchange[0].count==7&&sTagExchange[6].count==7;
    for(i=0;i<count;++i)if(i!=0&&i!=5&&i!=6&&i!=7)
        ok &= !BtlChar_TestFlag(BtlChar_Get(i),0xC7)&&!BtlChar_TestFlag(BtlChar_Get(i),0xC8);

    /* Interrupt before the result: restore the stranded partner, without
       changing the outsider's action or awarding a win. */
    sTagExchangeState[0]=10;BtlChar_Get(5)->unkE58=-1;
    BtlChar_SetHeldFlag(BtlChar_Get(5),0xE);BtlChar_SetFlag(BtlChar_Get(5),0xB);
    *(s32 *)((u8 *)BtlChar_Get(0)+0x948)=0xB4;
    TagClash_UpdateExchanges();
    ok &= sTagExchangeOwner[5]<0&&TagBattle_Pair(5)<0;
    ok &= !BtlChar_TestFlag(BtlChar_Get(5),0xB)&&!BtlChar_TestFlag(BtlChar_Get(5),0xE);
    ok &= *(s32 *)((u8 *)BtlChar_Get(5)+0x94C)==0xB;
    ok &= !BtlChar_TestFlag(BtlChar_Get(5),0xC7)&&!BtlChar_TestFlag(BtlChar_Get(5),0xC8);
    ok &= BtlAct_GetCurrent(BtlChar_Get(2))==0xB&&sTagExchangeOwner[7]==6;
    sTagExchangeTicks[6]=1200;TagClash_UpdateExchanges();
    ok &= sTagExchangeOwner[6]<0&&sTagExchangeOwner[7]<0&&TagBattle_Pair(6)<0;
    TagClash_Reset();TagBattle_ResetPairs();
    printf("tag aerial clash regression: separate contexts, nonadjacent seats, outsider protection, interruption and timeout %s\n",ok?"PASS":"FAIL");
    return ok;
}

/* Unlike the state regression, this fixture lets the production action,
   animation, collision and visibility passes run between observations. */
s32 TagClash_SceneStep(s32 interrupt){
    static s32 started,ticks,entered,attempted,cancelled;
    s32 count=TagBattle_Count(),peer=count-1;
    BtlFlagChr *a=BtlChar_Get(0),*b=BtlChar_Get(peer);
    if(!started){
        TagClash_Reset();TagBattle_ResetPairs();
        TagBattle_SetTarget(0,peer);TagBattle_SetTarget(peer,0);
        if(!TagClash_BeginVanish(0,peer))return 3;
        BtlChar_SetHeldFlag(a,6);BtlChar_SetHeldFlag(b,6);
        BtlAct_Request(a,0xFB);BtlAct_Request(b,0xFB);
        started=1;
        printf("tag aerial scene: started pair=0/%d interrupt=%d\n",peer,interrupt);
        return -1;
    }
    ++ticks;
    if(BtlAct_GetCurrent(a)==0xFC&&BtlAct_GetCurrent(b)==0xFC){
        entered=1;
        if(!attempted){
            /* Real throw entry rejects the third actor before reading a move. */
            s32 before=TagBattle_Target(0);
            BtlColl_StartThrow(BtlChar_Get(2),b,2,0);
            if(TagBattle_Target(0)!=before||TagBattle_Pair(2)>=0||TagBattle_Pair(0)!=peer)return 3;
            attempted=1;
        }
        if(interrupt&&!cancelled){BtlAct_Request(a,0xB);cancelled=1;}
    }
    if(entered&&sTagExchangeOwner[0]<0&&sTagExchangeOwner[peer]<0&&
       !TagClash_IsVanish(a)&&!TagClash_IsVanish(b)){
        s32 visible=!BtlChar_TestFlag(a,0xB)&&!BtlChar_TestFlag(b,0xB);
        s32 mobile=!BtlChar_TestFlag(a,0xE)&&!BtlChar_TestFlag(b,0xE);
        s32 finished=interrupt?cancelled:sTagExchange[0].count==7;
        printf("tag aerial scene: %s ticks=%d pair=0/%d visible=%d mobile=%d outsider=%d exchanges=%d\n",
            visible&&mobile&&attempted&&finished?"PASS":"FAIL",ticks,peer,visible,mobile,attempted,sTagExchange[0].count);
        return visible&&mobile&&attempted&&finished?0:3;
    }
    if(ticks%120==0)printf("tag aerial scene: tick=%d actions=%x/%x state=%d count=%d flags=%d/%d\n",
        ticks,BtlAct_GetCurrent(a),BtlAct_GetCurrent(b),sTagExchangeState[0],sTagExchange[0].count,
        BtlChar_TestFlag(a,0xC4),BtlChar_TestFlag(b,0xC4));
    if(ticks>900){printf("tag aerial scene: FAIL stalled\n");return 3;}
    return -1;
}
