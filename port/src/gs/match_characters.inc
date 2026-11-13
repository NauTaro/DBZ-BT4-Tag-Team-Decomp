/* Selection snapshots, never live transformed actors. Sent once at start and
 * again with a confirmed result. Network I/O stays on the signaling worker. */
namespace {
struct CharacterReport {
    std::string key,project;
    Dj args;
    Uint64 next=0,expires=0;
    unsigned request=0,revision=1,sentRevision=0;
    bool pending=false,acknowledged=false;
};
std::deque<CharacterReport> characterReports;
unsigned characterSerial=1000000,duelRound=0;
bool duelFight=false,duelFinished=false;
std::string duelKey,tagKey,duelGeneration;
unsigned duelSeed=0;
unsigned duelFinishTick=0;int duelFinishBits=0;
bool validCharacters(const Dj &a){
    if(!a.is_array()||a.size()>10)return false;
    for(const auto &id:a)if(!id.is_number_integer()||id.get<int>()<0||id.get<int>()>=512)return false;
    return true;
}
Dj selection(const Dj &a,const Dj &b){
    if(a.size()==1&&b.size()==1)return {{"char_a",a[0]},{"char_b",b[0]}};
    return {{"chars_a",a},{"chars_b",b}};
}
void ensureMatchWatcher(){
    if(!rankedWatcher&&!watchQuit){rankedWatcher=SDL_CreateThread(watchThread,"BT4 match reports",nullptr);if(rankedWatcher)atexit(Port_RankedShutdown);}
}
void queueCharacters(const Dj &args){
    if(!strstr(Bt4Signal_Project(),".workers.dev"))return;
    const auto key=text(args,"match");
    auto it=std::find_if(characterReports.begin(),characterReports.end(),[&](const CharacterReport &r){return r.key==key;});
    if(it!=characterReports.end()){
        if(args.contains("outcome")&&!it->args.contains("outcome")){it->args=args;++it->revision;it->acknowledged=false;it->next=0;it->expires=SDL_GetTicks()+120000;}
        return;
    }
    if(characterReports.size()>=16)characterReports.pop_front();
    CharacterReport r;r.key=key;r.project=Bt4Signal_Project();r.args=args;r.expires=SDL_GetTicks()+120000;
    characterReports.push_back(std::move(r));ensureMatchWatcher();
}
bool characterReply(const Dj &e,Uint64 now){
    for(auto &r:characterReports)if(r.pending&&r.request==number(e,"serial")){
        r.pending=false;r.next=now+3000;
        const auto b=e.find("body");
        if(b!=e.end()&&b->is_object()){
            const auto state=text(*b,"state"),err=text(*b,"error");
            if(state=="recorded"&&r.sentRevision==r.revision){r.acknowledged=true;r.expires=now+7200000;}
            if(state=="characters_disputed"||err=="BAD_CHARACTERS"||err=="NOT_A_MEMBER"||err=="NOT_A_PARTICIPANT")r.expires=now;
        }
        return true;
    }
    return false;
}
void characterStep(Uint64 now){
    for(auto &r:characterReports)if(!r.acknowledged&&now<r.expires&&r.project==Bt4Signal_Project()&&now>=r.next){
        r.request=++characterSerial;r.sentRevision=r.revision;
        const auto raw=r.args.dump();r.pending=Bt4Signal_Command(5,r.request,"match_report",raw.c_str());
        r.next=now+(r.pending?15000:3000);
    }
    characterReports.erase(std::remove_if(characterReports.begin(),characterReports.end(),[&](const CharacterReport &r){return now>=r.expires;}),characterReports.end());
}
void finishCharacters(const std::string &key,int bits){
    if(!(bits&7)||bits&8)return; // a disconnect retains the started record, never invents a winner
    auto it=std::find_if(characterReports.begin(),characterReports.end(),[&](const CharacterReport &r){return r.key==key;});
    if(it==characterReports.end()||it->args.contains("outcome"))return;
    auto args=it->args;const int outcome=(bits&3)==1?0:(bits&3)==2?1:2;
    const auto canonical="BT4-ONLINE-V1|"+key+"|"+std::to_string(outcome);char digest[65];
    UpdSha256_Hex(canonical.data(),canonical.size(),digest);args["outcome"]=outcome;args["digest"]=digest;queueCharacters(args);
}
}

extern "C" int Port_BattleRoster(int side,int *ids,int capacity);
extern "C" void Port_OnlineDuelObserve(int state,int bits,unsigned tick,unsigned confirmed,int resim,unsigned seed,const char *nameA,const char *nameB){
    if(resim)return;
    std::lock_guard<std::mutex> guard(lock);
    if(!d.on||!d.descriptor.is_object())return;
    const auto generation=text(d.descriptor,"generation");
    if(generation!=duelGeneration||seed!=duelSeed){duelGeneration=generation;duelSeed=seed;duelRound=0;duelFight=duelFinished=false;duelFinishBits=0;}
    if(state<0||(state<3&&duelFinished)){duelFight=false;duelFinished=false;duelFinishBits=0;}
    if(state==3&&!duelFight){
        int ids[7];Dj a=Dj::array(),b=Dj::array();
        for(int side=0;side<2;++side){const int n=Port_BattleRoster(side,ids,7);for(int i=0;i<n;++i)(side?b:a).push_back(ids[i]);}
        if(a.empty()||b.empty()||!validCharacters(a)||!validCharacters(b))return;
        duelFight=true;
        const auto fields=selection(a,b);
        if(d.ranked){d.characters=fields;return;}
        duelKey=text(d.descriptor,"generation")+":"+std::to_string(seed)+":"+std::to_string(++duelRound);
        auto args=fields;args["match"]=duelKey;args["generation"]=text(d.descriptor,"generation");args["mode"]="duel";
        args["names_a"]=Dj::array({nameA?nameA:""});args["names_b"]=Dj::array({nameB?nameB:""});queueCharacters(args);
    }
    if(d.ranked||!duelFight||state<4||state>6)return;
    if(duelFinishBits!=bits){duelFinishBits=bits;duelFinishTick=tick;return;}
    if(tick-duelFinishTick>=20&&confirmed>=duelFinishTick+2){finishCharacters(duelKey,bits);duelFinished=true;}
}

extern "C" void Port_OnlineTagObserve(const TtRoom *room,const char *descriptor,int state,int bits){
    if(!room||!descriptor||state<3||state>6)return;
    auto desc=Dj::parse(descriptor,nullptr,false);if(!desc.is_object()||text(desc,"generation").empty())return;
    std::lock_guard<std::mutex> guard(lock);
    const auto key=text(desc,"generation")+":tag:"+std::to_string(room->epoch)+":"+std::to_string(room->seed);
    if(state==3&&tagKey!=key){
        auto args=Dj::object();Dj a=Dj::array(),b=Dj::array(),pa=Dj::array(),pb=Dj::array();
        for(int i=0;i<TT_MAX_PLAYERS;++i){const auto &p=room->players[i];if(!p.member)continue;
            const bool side=!room->free_for_all&&p.team!=0;(side?b:a).push_back(p.chara);(side?pb:pa).push_back(p.cpu?0:p.member);
        }
        if(a.empty()||(!room->free_for_all&&b.empty())||!validCharacters(a)||!validCharacters(b))return;
        args["chars_a"]=a;args["chars_b"]=b;args["peers_a"]=pa;args["peers_b"]=pb;args["free_for_all"]=room->free_for_all!=0;
        args["mode"]="tag";args["match"]=key;args["generation"]=text(desc,"generation");tagKey=key;queueCharacters(args);
    }
    if(state>=4&&!room->free_for_all)finishCharacters(key,bits);
}
