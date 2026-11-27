/* Adapts existing UDP discovery. Native duel rollback and lobby/tournament
 * authority remain in net.c/net_lobby.c, on the participants' machines.
 */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>
#include <ctime>
#include <algorithm>
#include <string>
#include <array>
#include <map>
#include <deque>
#include <set>
#include <mutex>
#include <atomic>
#include <vector>
#include "match_supabase.h"
#include "profile_cards.h"
#include "tag_team_p2p.h"
#include "net_lobby.h"
#include "../../include/tag_team.h"
#include "supabase_signal.h"
#include "voice.h"
#include "update_sign.h"
extern "C" void Port_RankedShutdown(void);
#define JSON_NOEXCEPTION
#include "../../third_party/voice/json.hpp"
using Dj=nlohmann::json;
#include "profile_image_codec.inc"
namespace {
std::mutex lock;unsigned serial=20000;
std::string text(const Dj &j,const char *key){auto p=j.find(key);return p!=j.end()&&p->is_string()?p->get<std::string>():std::string();}
unsigned number(const Dj &j,const char *key){auto p=j.find(key);return p!=j.end()&&p->is_number_unsigned()?p->get<unsigned>():0;}
bool jsonFlag(const Dj &j,const char *key){auto p=j.find(key);return p!=j.end()&&p->is_boolean()&&p->get<bool>();}
Bt4ProfileCard profileCard(const Dj &card){
    Bt4ProfileCard result={int(std::min(7u,number(card,"name_color"))),int(std::min(7u,number(card,"background"))),
                          jsonFlag(card,"premium")?int(std::min(6u,number(card,"decoration"))):0,jsonFlag(card,"premium"),
                          jsonFlag(card,"checkout_available"),{},{},{},{}};
    const auto rgb=card.find("name_rgb");
    if(rgb!=card.end()&&rgb->is_number_unsigned()&&rgb->get<unsigned>()<=0xffffff){
        result.nameRgb=rgb->get<unsigned>();
        result.hasNameRgb=1;
    }
    const auto bannerRgb=card.find("banner_rgb");
    if(bannerRgb!=card.end()&&bannerRgb->is_number_unsigned()&&bannerRgb->get<unsigned>()<=0xffffff){
        result.bannerRgb=bannerRgb->get<unsigned>();
        result.hasBannerRgb=1;
    }
    const auto media=card.find("media");
    if(media!=card.end()&&media->is_object()){
        auto copy=[&](char (&out)[37],const char *kind){
            const std::string id=text(*media,kind);
            if(id.size()==36&&id.find_first_not_of("0123456789abcdef-")==std::string::npos)
                memcpy(out,id.c_str(),37);
        };
        copy(result.avatar,"avatar");
        if(result.premium){copy(result.motion,"motion");copy(result.banner,"banner");copy(result.bannerMotion,"banner_motion");}
    }
    return result;
}
struct Discovery {
    unsigned generation=0,init=0,countsRequest=0,tagCountsRequest=0,reconcile=0;bool on=false,waiting=false,refreshRoom=false;
    Bt4DiscoveryStatus status={};Dj descriptor,profile,localAddresses;std::string wire,content;
    std::map<unsigned,Dj> addresses;std::map<unsigned,bool> announced;
    std::map<unsigned,unsigned> announcements;
    std::map<unsigned,Uint64> retryAt;
    std::deque<Dj> earlySignals;
    Uint64 leaseAt=0,began=0,lastCounts=0,reconcileAt=0;std::array<int,5> counts={};bool countsKnown=false;
    Uint64 admissionRetryAt=0;unsigned admissionRetries=0;
    const char *admissionOp=nullptr;Dj admissionArgs;
    enum TurnState { NoPeer, Requesting, Ready, Allocating, Finished } turnState=NoPeer;
    unsigned turnRequest=0;Uint64 turnDeadline=0;
    Bt4RoomTurn turn={};
    Uint64 lastTagCounts=0;std::array<int,3> tagCounts={};bool tagCountsKnown=false;
    bool ranked=false,sawFight=false,reportPending=false;
    unsigned outcomeTick=0,reportSerial=0;int outcomeBits=0,reportState=0;
    int resultRating=0,resultDelta=0;
    Uint64 reportAt=0;Dj report,characters;
    unsigned rockCandidate=0,rockCandidateTick=0;
    bool rockSubmitted=false,quick=false,rockRoundEnded=false;
} d;
bool rankedNext=false;
std::atomic<bool> rankedActive{false};
std::atomic<bool> rankedRocksAvailable{false};
std::atomic<unsigned> rankedRockSeed{0};
extern "C" void Port_UiDragonBallFound(const char *,unsigned);
// Keep an authenticated liveness proof while fighting, then briefly after a
// remote disconnect. No simulation, GPU or blocking network work runs here.
struct RankedWatch {
    std::string match,project;
    Uint64 next=0,until=0;
    unsigned request=0;
    bool concede=false,peerGone=false,pending=false,notify=false,sentConcede=false,heartbeatAcknowledged=false;
    bool settled=false,review=false;
    int rating=0,delta=0;
    Dj characters;
};
std::vector<RankedWatch> rankedWatches;
struct RankedRockProof {
    std::string match,project;
    unsigned event=0,request=0;
    bool ranked=false;
    Uint64 next=0,until=0;
};
std::deque<RankedRockProof> rankedRockProofs;
struct RankedSettlement {int rating,delta;bool review;};
std::deque<RankedSettlement> rankedSettlements;
std::atomic<bool> settlementReady{false};
Uint64 nextSettlement;
SDL_Thread *rankedWatcher=nullptr;
std::atomic<bool> watchQuit{false};
unsigned watchSerial=900000;
void watchNotify(const RankedWatch &w){
    if(!w.notify||!w.settled)return;
    if(rankedSettlements.size()==4)rankedSettlements.pop_front();
    rankedSettlements.push_back({w.rating,w.delta,w.review});
    settlementReady=true;
}
bool characterReply(const Dj &,Uint64);
void characterStep(Uint64);
void ensureMatchWatcher();

bool queueRockProof(const std::string &match,unsigned event,bool ranked) {
    if (match.empty() || !event || rankedRockProofs.size()>=4) return false;
    const auto now=SDL_GetTicks();
    rankedRockProofs.push_back({match,Bt4Signal_Project(),event,0,ranked,now,now+120000});
    ensureMatchWatcher();
    return true;
}
void watchStep(Uint64 now){
    char raw[2048];
    while(Bt4Signal_Read(5,raw,sizeof(raw))>0){
        Dj e=Dj::parse(raw,nullptr,false);if(!e.is_object())continue;
        if(characterReply(e,now))continue;
        const unsigned reply = number(e,"serial");
        auto proof = std::find_if(rankedRockProofs.begin(),rankedRockProofs.end(),
                                  [reply](const RankedRockProof &p){return reply && p.request==reply;});
        if(proof!=rankedRockProofs.end()){
            proof->request=0;
            proof->next=now+3000;
            auto body=e.find("body");
            if(body!=e.end()&&body->is_object()){
                const auto state=text(*body,"state"),failure=text(*body,"error");
                if(state=="applied"){
                    const unsigned stars=number(*body,"stars");
                    const auto finder=text(*body,"finder");
                    const bool current=d.on&&proof->match==text(d.descriptor,"generation")&&rankedRockSeed!=0;
                    if(current&&stars>=1&&stars<=7&&!finder.empty()&&finder.size()<=19)
                        Port_UiDragonBallFound(finder.c_str(),stars);
                }
                if(state=="applied"||state=="disputed"||state=="unavailable"||state=="expired"||
                   failure=="NOT_A_PARTICIPANT"||failure=="BAD_ROCK_EVENT")proof->until=1;
            }
            continue;
        }
        for(auto &w:rankedWatches)if(w.pending&&w.request==number(e,"serial")){
            w.pending=false;w.next=now+5000;
            auto b=e.find("body");if(b==e.end()||!b->is_object())continue;
            auto state=text(*b,"state");
            if(state=="waiting"&&!w.sentConcede){
                w.heartbeatAcknowledged=true;
                if(w.concede)w.next=now;
            }
            if(state=="applied"||state=="already_applied"){
                auto rating=b->find("rating"),delta=b->find("delta");
                if(rating!=b->end()&&rating->is_number_integer()&&delta!=b->end()&&delta->is_number_integer()){
                    w.settled=true;
                    w.rating=rating->get<int>();
                    w.delta=delta->get<int>();
                    w.review=jsonFlag(*b,"under_review");
                    // The server can settle a concession before UDP sees the peer leave.
                    if(!w.until)continue;
                    watchNotify(w);
                }
            }
            if(state=="applied"||state=="already_applied"||state=="not_started"||state=="expired"||
               text(*b,"error")=="NOT_A_PARTICIPANT")w.until=1;
        }
    }
    for(auto &w:rankedWatches){
        if(w.settled||(w.until&&now>=w.until))continue;
        if(w.project!=Bt4Signal_Project())continue; // a new route cannot settle an old route's match
        if(now>=w.next){
            w.request=++watchSerial;
            Dj args={{"match",w.match}};
            if(w.characters.is_object())args.update(w.characters);
            w.sentConcede=w.concede&&w.heartbeatAcknowledged;
            if(w.sentConcede)args["reason"]="local_quit";
            else args["peer_gone"]=w.peerGone;
            auto body=args.dump();
            w.pending=Bt4Signal_Command(5,w.request,w.sentConcede?"ranked_forfeit":"ranked_heartbeat",body.c_str());
            w.next=now+(w.pending?25000:5000);
        }
    }
    for(auto &proof:rankedRockProofs){
        if(now>=proof.until||now<proof.next||proof.project!=Bt4Signal_Project())continue;
        const unsigned request=++watchSerial;
        const std::string args=Dj{{"match",proof.match},{"event",proof.event}}.dump();
        if(Bt4Signal_Command(5,request,proof.ranked?"ranked_rock":"quick_rock",args.c_str())){
            proof.request=request;
            proof.next=now+20000;
        }else proof.next=now+3000;
    }
    rankedRockProofs.erase(std::remove_if(rankedRockProofs.begin(),rankedRockProofs.end(),
        [now](const RankedRockProof &p){return now>=p.until;}),rankedRockProofs.end());
    characterStep(now);
    rankedWatches.erase(std::remove_if(rankedWatches.begin(),rankedWatches.end(),[&](const RankedWatch &w){return w.until&&now>=w.until;}),rankedWatches.end());
}
int SDLCALL watchThread(void*){
    SDL_SetCurrentThreadPriority(SDL_THREAD_PRIORITY_LOW);
    while(!watchQuit){SDL_Delay(250);std::lock_guard<std::mutex> guard(lock);watchStep(SDL_GetTicks());}
    return 0;
}
void watchFight(){
    auto match=text(d.descriptor,"generation");if(match.empty())return;
    auto it=std::find_if(rankedWatches.begin(),rankedWatches.end(),[&](const RankedWatch &w){return w.match==match;});
    if(it==rankedWatches.end()){
        if(rankedWatches.size()>=4)return;
        rankedWatches.push_back({match,Bt4Signal_Project()});
        rankedWatches.back().characters=d.characters;
    }else if(it->until)return; // a completed/left match is not revived by a later presentation frame
    if(!rankedWatcher&&!watchQuit){
        rankedWatcher=SDL_CreateThread(watchThread,"BT4 Ranked liveness",nullptr);
        if(rankedWatcher)atexit(Port_RankedShutdown);
    }
}
void watchFinish(bool concede,bool disconnected=false,bool peerGone=false){
    auto match=text(d.descriptor,"generation");
    for(auto &w:rankedWatches)if(w.match==match&&(!w.until||(disconnected&&!w.settled))){
        if(!w.until)w.until=SDL_GetTicks()+120000;
        w.concede=w.concede||(concede&&!d.reportSerial);
        // A missing opponent may prevent their normal result report as well.
        // Keep proving our presence even when our own report is already waiting.
        w.peerGone=w.peerGone||peerGone;
        w.notify=w.notify||(disconnected&&d.reportState!=2&&d.reportState!=3);
        if(w.settled){watchNotify(w);w.until=1;continue;}
        if(!w.pending)w.next=0;
    }
}
bool rpc(const char *op,const Dj &args,unsigned id=0){
    if(d.ranked&&!strcmp(op,"queue"))op="ranked_queue";
    return Bt4Signal_Command(0,id?id:++serial,op,args.dump().c_str());
}
bool requestAdmission(){
    d.init=++serial;
    if(rpc(d.admissionOp,d.admissionArgs,d.init))return true;
    d.init=0;return false;
}
void error(const std::string &code){
    if(d.status.state==3)return;d.status.state=-1;
    fprintf(stderr,"bt4: discovery: admission failed (%s)\n",code.c_str());
    const char *message=code=="ROOM_FULL"?"Someone has already joined that room.":code=="ROOM_NOT_FOUND"?"No room with that code.":
        code=="INCOMPATIBLE_DATA"?"The host's game uses incompatible battle data.":code=="RATE_LIMITED"?"Too many requests. Wait a moment, then try again.":
        code=="ROOM_BUSY"?"That room has started and is no longer accepting players.":code=="ALREADY_IN_ROOM"?"Leave the current online room before joining another.":
        code=="BAD_CODE"?"Enter the six-character room code again.":
        code=="UPDATE_REQUIRED"?"This is an old or incompatible version. Please update the complete BT4 launcher.":
        code=="AUTH_REQUIRED"?"Online sign-in failed. Check your connection and try again.":
        code=="NICKNAME_REQUIRED"?"Choose a nickname before playing online.":
        code=="ROOM_ROUTE_EXPIRED"?"This room code belongs to an older online route. Ask the host for a new code.":
        code=="ROUTE_CHANGED"||code=="ROUTE_REFRESH_REQUIRED"||code=="ROUTE_RECOVERING"?"The online room service is changing over. Wait a moment and try again.":
        code=="BAD_REQUEST"||code=="BAD_PROFILE"||code=="BAD_CAPACITY"?"The room settings are invalid. Review them and try again.":
        code=="INTERNAL_ERROR"?"The online service had a problem. Please try again later.":
        code=="RANKED_NOT_ENABLED"||code=="RANKED_UNAVAILABLE"?"Ranked is temporarily unavailable. Please try again later.":
        "The matchmaking service could not be reached (is the network up?).";
    snprintf(d.status.error,sizeof(d.status.error),"%s",message);
}
void refreshStatus(){
    if(!d.descriptor.is_object())return;unsigned self=number(d.descriptor,"self");bool host=self==1;
    /* Some deployed descriptors omit mode. Fall back only in that case:
     * a join request starts as duel, but may discover a native lobby. */
    std::string mode=text(d.descriptor,"mode");
    if(mode.empty())mode=text(d.profile,"mode");
    d.status.role=host?1:2;d.status.lobby=mode!="duel";
    snprintf(d.status.code,sizeof(d.status.code),"%s",text(d.descriptor,"code").c_str());
    auto members=d.descriptor.find("members");if(members==d.descriptor.end()||!members->is_array())return;
    unsigned target=0;
    if(!host)target=1;
    else if(!d.status.lobby)for(const auto &member:*members){
        std::string role=text(member,"role");
        if(number(member,"peer")!=self&&(role=="player"||role.empty())){target=number(member,"peer");break;}
    }
    d.status.peerCount=0;memset(d.status.peerAddr,0,sizeof(d.status.peerAddr));
    d.status.peerName[0]=0;
    auto remote=d.addresses.find(target);
    if(d.turnState!=Discovery::Requesting&&d.turnState!=Discovery::Ready&&d.turnState!=Discovery::Allocating&&
       target&&remote!=d.addresses.end()&&remote->second.is_array()&&!remote->second.empty()){
        d.status.peerCount=0;for(const auto &value:remote->second)if(value.is_string()&&value.get_ref<const std::string&>().size()<64&&d.status.peerCount<4)
            snprintf(d.status.peerAddr[d.status.peerCount++],64,"%s",value.get_ref<const std::string&>().c_str());
        for(const auto &member:*members)if(number(member,"peer")==target)snprintf(d.status.peerName,sizeof(d.status.peerName),"%s",text(member,"name").c_str());
        if(d.status.peerCount)d.status.state=3;
    }else if(host)d.status.state=2;
    if(host){char previous[MATCH_WATCH_ADDRS][64];memcpy(previous,d.status.watchAddr,sizeof(previous));int old=d.status.watchCount;d.status.watchCount=0;memset(d.status.watchAddr,0,sizeof(d.status.watchAddr));
        for(const auto &member:*members){unsigned peer=number(member,"peer");if(peer==self||(!d.status.lobby&&text(member,"role")!="spectator"))continue;
            auto p=d.addresses.find(peer);if(p==d.addresses.end()||!p->second.is_array())continue;
            std::string seen;
            for(const auto &address:p->second)if(address.is_string()&&address.get_ref<const std::string&>().size()<64&&d.status.watchCount<MATCH_WATCH_ADDRS){
                const auto &value=address.get_ref<const std::string&>();
                snprintf(d.status.watchAddr[d.status.watchCount++],64,"%s",value.c_str());
                if(seen.empty()&&value.rfind("relay=",0)!=0&&value.find(':')!=std::string::npos)seen="seen="+value.substr(0,value.rfind(':'));
            }
            // Native lobby pending-member tracking needs this alongside candidates.
            if(!seen.empty()&&d.status.watchCount<MATCH_WATCH_ADDRS)snprintf(d.status.watchAddr[d.status.watchCount++],64,"%s",seen.c_str());}
        if(old!=d.status.watchCount||memcmp(previous,d.status.watchAddr,sizeof(previous)))++d.status.watchGen;
    }
}
bool takeSignal(const Dj &body){
    if(text(body,"generation")!=text(d.descriptor,"generation"))return false;
    unsigned from=number(body,"from");auto members=d.descriptor.find("members");
    if(!from||members==d.descriptor.end()||!members->is_array())return false;
    bool member=false;for(const auto &m:*members)if(number(m,"peer")==from)member=true;
    auto value=body.find("body");
    if(!member||value==body.end()||!value->is_object())return false;
    auto addresses=value->find("addresses");
    if(addresses==value->end()||!addresses->is_array()||addresses->empty()||addresses->size()>4)return false;
    for(const auto &a:*addresses)if(!a.is_string()||a.get_ref<const std::string&>().size()>=64)return false;
    auto decoded = *addresses;
    for(auto &item:decoded){
        const auto &address=item.get_ref<const std::string&>();
        // [IPv4]:port distinguishes a relay using the existing SQL wire grammar.
        const auto end=address.find("]:");
        if(!address.empty()&&address.front()=='['&&end!=std::string::npos&&
           address.find(':')==end+1&&address.find('.')!=std::string::npos){
            item="relay="+address.substr(1,end-1)+address.substr(end+1);
        }
    }
    d.addresses[from]=std::move(decoded);return true;
}
void metadata(const Dj &body){
    auto generation=text(body,"generation");if(generation.empty())return;
    if(d.descriptor.is_object()&&!text(d.descriptor,"generation").empty()&&generation!=text(d.descriptor,"generation"))return;
    d.descriptor=body;d.waiting=false;auto members=body.find("members");if(members==body.end()||!members->is_array())return;
    if((d.ranked||d.quick)&&!d.rockRoundEnded){
        const unsigned seed=number(body,d.ranked?"ranked_rock_seed":"quick_rock_seed");
        if(seed && seed<=8388608)rankedRockSeed=seed;
    }
    // Endpoint announcements are also retained in the authenticated descriptor.
    // A broadcast accepted before the recipient subscribed is not delivery.
    auto endpoints=body.find("udp");
    if(endpoints!=body.end()&&endpoints->is_array())for(const auto &entry:*endpoints){
        if(!entry.is_object())continue;
        Dj signal={{"generation",generation},{"from",number(entry,"peer")},{"body",{{"addresses",entry.value("addresses",Dj::array())}}}};
        if(number(entry,"peer")!=number(body,"self"))takeSignal(signal);
    }
    PortVoice_P2PSession(1,body.dump().c_str(),nullptr,0);
    unsigned self=number(body,"self");std::string nonce;for(char c:generation)if(c!='-')nonce+=c;
    if(d.turnState==Discovery::NoPeer&&self&&members->size()>1){
        d.turnRequest=++serial;
        auto args=Dj{{"generation",generation}}.dump();
        if(Bt4Signal_Command(0,d.turnRequest,"turn_credentials",args.c_str())){
            d.turnState=Discovery::Requesting;
            d.turnDeadline=SDL_GetTicks()+2500;
        }else{
            d.turnRequest=0;
            d.turnState=Discovery::Finished;
        }
    }
    std::set<unsigned> present;for(const auto &member:*members)present.insert(number(member,"peer"));
    for(auto it=d.retryAt.begin();it!=d.retryAt.end();)if(!present.count(it->first)){
        d.announced.erase(it->first);d.addresses.erase(it->first);it=d.retryAt.erase(it);
    }else ++it;
    for(auto it=d.announcements.begin();it!=d.announcements.end();)if(!present.count(it->second))it=d.announcements.erase(it);else ++it;
    for(const auto &member:*members){unsigned peer=number(member,"peer");if(d.turnState!=Discovery::Finished||!peer||peer==self||d.announced[peer]||SDL_GetTicks()<d.retryAt[peer])continue;
        bool pending=false;for(const auto &request:d.announcements)if(request.second==peer)pending=true;
        if(pending)continue;
        unsigned id=++serial;
        if(rpc("signal",{{"generation",generation},{"to",peer},{"channel","duel"},{"kind","udp"},{"body",{{"addresses",d.localAddresses},{"nonce",nonce}}}},id))d.announcements[id]=peer;
        d.retryAt[peer]=SDL_GetTicks()+2000;
    }
    for(auto it=d.earlySignals.begin();it!=d.earlySignals.end();)
        if(text(*it,"generation")!=generation||takeSignal(*it))it=d.earlySignals.erase(it);else ++it;
    refreshStatus();
}
void pump(){
    char buffer[24577];unsigned work=0;
    while(work++<64&&Bt4Signal_Read(0,buffer,sizeof(buffer))>0){Dj event=Dj::parse(buffer,nullptr,false);if(!event.is_object())continue;auto body=event.find("body");if(body==event.end())continue;
        auto kind=text(event,"event");
        if(kind=="reply"){
            unsigned reply=number(event,"serial");
            if(d.turnRequest&&reply==d.turnRequest){
                d.turnRequest=0;
                auto host=text(*body,"hostname"),user=text(*body,"username"),password=text(*body,"credential");
                unsigned port=number(*body,"udp_port"),expires=number(*body,"expires_at");
                if(text(*body,"state")=="ready"&&port>0&&port<=65535&&expires>(uint64_t)std::time(nullptr)+120&&
                   host.size()>=2&&host.size()<=253&&host.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.-")==std::string::npos&&
                   !user.empty()&&user.size()<=128&&!password.empty()&&password.size()<=128&&
                   user.find('\0')==std::string::npos&&password.find('\0')==std::string::npos){
                    snprintf(d.turn.server,sizeof(d.turn.server),"%s:%u",host.c_str(),port);
                    SDL_strlcpy(d.turn.username,user.c_str(),sizeof(d.turn.username));
                    SDL_strlcpy(d.turn.password,password.c_str(),sizeof(d.turn.password));
                    d.turnState=Discovery::Ready;
                }else{
                    d.turnState=Discovery::Finished;
                    if(d.descriptor.is_object()){auto current=d.descriptor;metadata(current);}
                }
                continue;
            }
            if(d.reportSerial&&reply==d.reportSerial){
                d.reportPending=false;d.reportAt=SDL_GetTicks()+3000;
                std::string state=text(*body,"state"),failure=text(*body,"error");
                if(state=="applied"||state=="already_applied"){
                    d.reportState=jsonFlag(*body,"under_review")?3:2;d.resultRating=(int)std::min(100000u,number(*body,"rating"));
                    auto delta=body->find("delta");d.resultDelta=delta!=body->end()&&delta->is_number_integer()
                        ?(int)std::max<int64_t>(-100000,std::min<int64_t>(100000,delta->get<int64_t>())):0;
                }
                else if(state=="disputed"||(!failure.empty()&&failure!="SIGNALING_UNAVAILABLE"&&failure!="RATE_LIMITED"))d.reportState=-1;
                continue;
            }
            auto announcement=d.announcements.find(reply);
            if(announcement!=d.announcements.end()){
                d.announced[announcement->second]=body->is_object()&&text(*body,"state")=="sent";
                auto failure=text(*body,"error");
                if(!failure.empty()){
                    if(failure=="RATE_LIMITED")d.retryAt[announcement->second]=SDL_GetTicks()+15000;
                    fprintf(stderr,"bt4: discovery: endpoint announcement failed (%s)\n",failure.c_str());
                }
                d.announcements.erase(announcement);
            }else if(reply==d.countsRequest&&body->is_object()&&!body->contains("error")){const char *keys[]={"S","T","D10","D15","D20"};for(int i=0;i<5;++i)d.counts[i]=(int)number(*body,keys[i]);d.countsKnown=true;}
            else if(reply==d.tagCountsRequest&&body->is_object()&&!body->contains("error")){const char *keys[]={"F6","T2","T5"};for(int i=0;i<3;++i)d.tagCounts[i]=(int)number(*body,keys[i]);d.tagCountsKnown=true;}
            else if(reply&&(reply==d.init||reply==d.reconcile)&&body->is_object()){
                auto failure=text(*body,"error");
                if(d.reconcile&&reply==d.reconcile){
                    d.reconcile=0;
                    if(failure=="RATE_LIMITED"||failure=="SIGNALING_UNAVAILABLE"){
                        d.refreshRoom=true;
                        d.reconcileAt=SDL_GetTicks()+(failure=="RATE_LIMITED"?15000:8000);
                        fprintf(stderr,"bt4: discovery: room refresh delayed (%s)\n",failure.c_str());
                        continue;
                    }
                }else{
                    d.init=0;
                    if(failure=="RATE_LIMITED"&&d.status.state==1&&!d.descriptor.is_object()&&d.admissionRetries<2){
                        d.admissionRetryAt=SDL_GetTicks()+(d.admissionRetries++?30000:15000);
                        fprintf(stderr,"bt4: discovery: admission delayed (RATE_LIMITED)\n");
                        continue;
                    }
                }
                if(!failure.empty()){error(failure);continue;}
                if(text(*body,"state")=="waiting")d.waiting=true;else metadata(*body);
            }
        }else if(kind=="bt4.ready"&&d.on&&d.descriptor.is_object()){
            d.announced.clear();d.announcements.clear();d.retryAt.clear();d.refreshRoom=true;
        }else if(kind=="bt4.room"&&d.on&&body->is_object()&&(d.descriptor.is_object()||d.waiting)){
            // A delayed notification from the room just left must not establish
            // the identity of a new create/join before its admission reply.
            metadata(*body);
        }
        else if(kind=="bt4.signal"&&d.on&&body->is_object()&&text(*body,"channel")=="duel"&&text(*body,"kind")=="udp"){
            if(takeSignal(*body))refreshStatus();
            else if(d.earlySignals.size()<16)d.earlySignals.push_back(*body);
        }
    }
}
}
static bool nextListed=false,nextTournament=false;
extern "C" void Bt4Discovery_CreatePrivacy(int listed,int tournament){
    std::lock_guard<std::mutex> guard(lock);
    nextListed=listed!=0;
    nextTournament=tournament!=0;
}
extern "C" int Bt4Discovery_Begin(unsigned generation,int role,int protocol,uint64_t content,const char *name,const char *code,const char *addresses,const char *queue){
    std::lock_guard<std::mutex> g(lock);if(generation<d.generation||!name||!addresses)return 0;
    Dj local=Dj::parse(addresses,nullptr,false);if(!local.is_array()||local.empty()||local.size()>4)return 0;
    d.generation=generation;d.on=true;d.waiting=false;d.status={};d.status.state=1;d.descriptor=Dj();d.addresses.clear();d.announced.clear();d.localAddresses=local;
    d.announcements.clear();d.retryAt.clear();d.earlySignals.clear();d.reconcile=0;d.reconcileAt=0;d.refreshRoom=false;
    d.ranked=rankedNext&&role==3;rankedNext=false;d.sawFight=false;d.reportPending=false;
    rankedActive=d.ranked;
    rankedRockSeed=0;d.rockCandidate=d.rockCandidateTick=0;d.rockSubmitted=d.rockRoundEnded=false;
    d.quick=role==3&&!d.ranked;
    d.outcomeTick=d.reportSerial=0;d.outcomeBits=d.reportState=0;d.reportAt=0;d.report=Dj();d.characters=Dj();
    d.resultRating=d.resultDelta=0;
    char hash[17];snprintf(hash,sizeof(hash),"%016llx",(unsigned long long)content);d.content=hash;d.wire="bt4-duel-"+std::to_string(protocol);
    if(d.ranked){
        d.wire+=":result2";
        if(rankedRocksAvailable)d.wire+=":rock1";
    }
    d.profile={{"mode",role==5?"lobby":"duel"},{"wire",d.wire},{"content",d.content},{"rules",queue&&*queue?queue:"S"},{"name",std::string(name).substr(0,19)},{"capacity",role==5?LOBBY_SLOTS:LOBBY_CLASSIC_SLOTS}};
    if(role==5){
        d.profile["listed"]=nextListed;
        if(nextTournament)d.profile["rules"]="public-tournament";
        nextListed=nextTournament=false;
    }
    if(d.ranked){d.profile["ruleset"]="solo";d.profile["rules"]="S";d.profile["abandonment_version"]=2;}
    if(d.quick)d.profile["rock_version"]=1;
    d.began=SDL_GetTicks();d.leaseAt=d.began+60000;
    if(!Bt4Signal_Open()){d.on=false;rankedActive=false;return 0;}
    // Explicit reset precedes the new admission in the single HTTP queue.
    if(!rpc("leave",Dj::object())){d.on=false;rankedActive=false;return 0;}
    d.admissionRetryAt=0;d.admissionRetries=0;
    d.turnState=Discovery::NoPeer;d.turnRequest=0;d.turn={};
    d.admissionOp=role==1||role==5?"create":role==3?"queue":"join";
    d.admissionArgs=d.profile;
    if(role!=1&&role!=5&&role!=3){
        d.admissionArgs={{"code",code?code:""},{"wire",d.wire},{"content",d.content},{"name",d.profile["name"]},{"role",role==4?"spectator":"player"}};
    }
    return requestAdmission();
}
extern "C" int Bt4Discovery_Poll(unsigned generation,Bt4DiscoveryStatus *out){
    std::lock_guard<std::mutex> g(lock);if(!out||!d.on||generation!=d.generation)return 0;pump();Uint64 now=SDL_GetTicks();
    if(d.turnState==Discovery::Requesting&&now>=d.turnDeadline){
        d.turnRequest=0;d.turnState=Discovery::Finished;
        if(d.descriptor.is_object()){auto current=d.descriptor;metadata(current);}
    }
    if(d.admissionRetryAt&&now>=d.admissionRetryAt){
        if(d.status.state!=1||requestAdmission())d.admissionRetryAt=0;
        else d.admissionRetryAt=now+1000;
    }
    // The server allows 30 reads per minute. Reconnects share the same budget.
    if(d.descriptor.is_object()&&d.status.state>0&&(d.refreshRoom||d.status.state<3)&&!d.reconcile&&now>=d.reconcileAt){
        d.reconcile=++serial;d.reconcileAt=now+3500;
        if(rpc("get",Dj::object(),d.reconcile))d.refreshRoom=false;
        else d.reconcile=0;
    }
    if(now>=d.leaseAt){d.leaseAt=now+60000;if(d.waiting&&!d.init&&!d.admissionRetryAt)requestAdmission();else if(number(d.descriptor,"self")==1)rpc("lease",Dj::object());}
    // First-run admission may perform anonymous auth and then the room RPC;
    // both have bounded 20 s HTTP receive timeouts. Do not cancel the UI while
    // that request is still within its legitimate deadline.
    if(d.status.state==1&&!d.admissionRetryAt&&Bt4Signal_Status()<0&&now-d.began>45000)error("SIGNALING_UNAVAILABLE");
    // A queued RPC is not an acknowledgement; retry failed announcements at a bounded rate.
    bool retry=false;for(const auto &entry:d.retryAt)if(!d.announced[entry.first]&&now>=entry.second){
        bool pending=false;for(const auto &request:d.announcements)if(request.second==entry.first)pending=true;
        if(!pending)retry=true;
    }
    if(retry&&d.descriptor.is_object())metadata(d.descriptor);
    *out=d.status;return 1;
}
extern "C" int Bt4Discovery_TakeTurn(unsigned generation,Bt4RoomTurn *out){
    std::lock_guard<std::mutex> g(lock);
    if(!d.on||generation!=d.generation||d.turnState!=Discovery::Ready)return 0;
    *out=d.turn;d.turn={};d.turnState=Discovery::Allocating;
    return 1;
}
extern "C" void Bt4Discovery_TurnReady(unsigned generation,const char *address){
    std::lock_guard<std::mutex> g(lock);
    if(!d.on||generation!=d.generation||d.turnState!=Discovery::Allocating)return;
    if(address&&d.localAddresses.size()<4)d.localAddresses.push_back(address);
    d.turnState=Discovery::Finished;
    if(d.descriptor.is_object()){auto current=d.descriptor;metadata(current);}
}
extern "C" void Bt4Discovery_End(unsigned generation){std::lock_guard<std::mutex> g(lock);if(!d.on||generation!=d.generation)return;
    // Realtime is only the discovery channel. Cancellation and leave are HTTP
    // operations, so always queue them even if the inbox is disconnected.
    if(d.waiting)rpc("cancel",Dj::object());
    if(d.ranked&&d.sawFight){auto m=text(d.descriptor,"generation");for(auto &w:rankedWatches)if(w.match==m&&!w.until){w.until=SDL_GetTicks()+120000;w.next=0;}}
    Dj leave=Dj::object();auto generationId=text(d.descriptor,"generation");if(!generationId.empty())leave["generation"]=generationId;
    rpc("leave",leave);d.earlySignals.clear();d.on=false;d.turn={};d.turnRequest=0;rankedActive=false;rankedRockSeed=0;}
extern "C" void Port_ProfileCardForPlayer(const char *name,Bt4ProfileCard *out){
    *out={};
    std::lock_guard<std::mutex> guard(lock);
    if(!d.on||!name||!*name)return;
    const auto members=d.descriptor.find("members");
    if(members==d.descriptor.end()||!members->is_array())return;
    for(const auto &member:*members){
        if(text(member,"name")!=name||text(member,"role")!="player")continue;
        const auto card=member.find("card");
        if(card!=member.end()&&card->is_object())*out=profileCard(*card);
        const auto wins=member.find("wins"), losses=member.find("losses");
        if(d.ranked&&wins!=member.end()&&losses!=member.end()&&wins->is_number_unsigned()&&losses->is_number_unsigned()){
            out->rankedWins=int(std::min(1000000u,wins->get<unsigned>()));
            out->rankedLosses=int(std::min(1000000u,losses->get<unsigned>()));
            out->hasRankedRecord=1;
        }
        break;
    }
}
extern "C" int Port_ProfileRankedRating(const char *name){
    if(!name||!*name)return -1;
    std::lock_guard<std::mutex> guard(lock);
    if(!d.on||!d.ranked)return -1;
    const auto members=d.descriptor.find("members");
    if(members==d.descriptor.end()||!members->is_array())return -1;
    for(const auto &member:*members){
        if(text(member,"name")!=name||text(member,"role")!="player")continue;
        const auto rating=member.find("rating");
        if(rating!=member.end()&&rating->is_number_unsigned())
            return int(std::min(100000u,rating->get<unsigned>()));
    }
    return -1;
}
extern "C" void Port_ProfileCardForTagPlayer(const char *name,Bt4ProfileCard *out){
    *out={};
    if(!name||!*name)return;
    // The RTC worker only changes the descriptor when Cloudflare sends a roster.
    // Recheck at a bounded rate and keep JSON out of the render loop.
    static std::string known;
    static std::map<std::string,Bt4ProfileCard> cards;
    static Uint64 next=0;
    const Uint64 now=SDL_GetTicks();
    if(now>=next){
        next=now+250;
        char data[8192]={};
        if(!TagPeer_Descriptor(data,sizeof(data)))data[0]=0;
        if(known!=data){
            known=data;
            cards.clear();
            const auto parsed=Dj::parse(known,nullptr,false);
            const auto members=parsed.find("members");
            if(members!=parsed.end()&&members->is_array())for(const auto &member:*members){
                const auto card=member.find("card");
                if(card!=member.end()&&card->is_object())cards.emplace(text(member,"name"),profileCard(*card));
            }
        }
    }
    const auto profile=cards.find(name);
    if(profile!=cards.end())*out=profile->second;
}
extern "C" int Bt4Discovery_Counts(int protocol,uint64_t content,int out[5]){
    std::lock_guard<std::mutex> g(lock);if(!out)return 0;pump();Uint64 now=SDL_GetTicks();
    if(!d.lastCounts||now-d.lastCounts>=30000){d.lastCounts=now;char hash[17];snprintf(hash,sizeof(hash),"%016llx",(unsigned long long)content);d.countsRequest=++serial;
        rpc("counts",{{"wire","bt4-duel-"+std::to_string(protocol)},{"content",hash}},d.countsRequest);}
    for(int i=0;i<5;++i)out[i]=d.counts[i];return d.countsKnown;
}
extern "C" int Bt4Discovery_TagCounts(uint64_t content,int out[3]){
    std::lock_guard<std::mutex> g(lock);if(!out)return 0;pump();Uint64 now=SDL_GetTicks();
    if(!d.lastTagCounts||now-d.lastTagCounts>=30000){d.lastTagCounts=now;char hash[17];snprintf(hash,sizeof(hash),"%016llx",(unsigned long long)content);d.tagCountsRequest=++serial;
        rpc("counts",{{"wire","bt4-p2p-2"},{"content",hash}},d.tagCountsRequest);}
    for(int i=0;i<3;++i)out[i]=d.tagCounts[i];return d.tagCountsKnown;
}
extern "C" void Port_RankedSearch(int selected){std::lock_guard<std::mutex> g(lock);rankedNext=selected!=0;}
extern "C" int Port_RankedActive(void){return rankedActive.load();}
extern "C" int Port_RankedRivalPoints(const char *name){
    if(!name||!*name)return -1;
    std::lock_guard<std::mutex> guard(lock);
    if(!d.on||!d.ranked)return -1;
    const auto members=d.descriptor.find("members");
    if(members==d.descriptor.end()||!members->is_array())return -1;
    for(const auto &member:*members){
        if(text(member,"name")!=name||text(member,"role")!="player")continue;
        const auto rating=member.find("rating");
        if(rating!=member.end()&&rating->is_number_unsigned())
            return int(std::min(100000u,rating->get<unsigned>()));
    }
    return -1;
}
extern "C" unsigned Port_RankedRockSeed(void){return rankedRockSeed.load();}
extern "C" void Port_RankedObserveRock(int state,unsigned event,unsigned tick,unsigned confirmed){
    if(!rankedRockSeed)return;
    std::lock_guard<std::mutex> guard(lock);
    if(!d.on||(!d.ranked&&!d.quick))return;
    if(state>=4&&state<=6)d.rockRoundEnded=true;
    if(d.rockRoundEnded&&state<4){rankedRockSeed=0;return;}
    if(state<3||state>6||d.rockSubmitted)return;
    if(event!=d.rockCandidate){d.rockCandidate=event;d.rockCandidateTick=tick;}
    if(!event||confirmed<d.rockCandidateTick)return;
    d.rockSubmitted=queueRockProof(text(d.descriptor,"generation"),event,d.ranked);
}
extern "C" void Port_RankedShutdown(void){
    watchQuit=true;
    if(rankedWatcher){SDL_WaitThread(rankedWatcher,nullptr);rankedWatcher=nullptr;}
    std::lock_guard<std::mutex> guard(lock);rankedWatches.clear();rankedRockProofs.clear();
}
extern "C" void Port_RankedDisconnected(int peerGone){
    std::lock_guard<std::mutex> guard(lock);
    if(d.ranked&&d.sawFight)watchFinish(false,true,peerGone!=0);
}
extern "C" int Port_RankedTakeSettlement(int *rating,int *delta){
    if(!settlementReady)return 0;
    std::lock_guard<std::mutex> guard(lock);
    Uint64 now=SDL_GetTicks();
    if(rankedSettlements.empty()||now<nextSettlement)return 0;
    *rating=rankedSettlements.front().rating;
    *delta=rankedSettlements.front().delta;
    int result=rankedSettlements.front().review?2:1;
    rankedSettlements.pop_front();settlementReady=!rankedSettlements.empty();
    nextSettlement=now+2000;
    return result;
}
extern "C" int Port_RankedResult(int *rating,int *delta){
    std::lock_guard<std::mutex> g(lock);
    if(rating)*rating=d.resultRating;if(delta)*delta=d.resultDelta;return d.reportState;
}
extern "C" void Port_RankedObserve(int battleState,int bits,unsigned tick,unsigned confirmed,int resimulating,int localConcession){
    std::lock_guard<std::mutex> g(lock);
    if(!d.on||!d.ranked||resimulating||!d.descriptor.is_object())return;
    // Discovery polling ends after the handshake. Consume result acknowledgments
    // here while the confirmed fight/result ticks continue.
    pump();
    Uint64 now=SDL_GetTicks();
    if(d.reportSerial){
        if(d.reportState==1&&!d.reportPending&&now>=d.reportAt){
            d.reportSerial=++serial;d.reportPending=rpc("ranked_report",d.report,d.reportSerial);
            d.reportAt=now+3000;
        }
        return;
    }
    if(battleState==3){if(!d.sawFight)watchFight();d.sawFight=true;d.outcomeBits=0;return;}
    if(!d.sawFight||battleState<4||battleState>6||!(bits&15)){d.outcomeBits=0;return;}
    bits=bits&8?8:bits&7;
    if(d.outcomeBits!=bits){d.outcomeBits=bits;d.outcomeTick=tick;return;}
    if(bits==8){
        if(localConcession>=0&&confirmed>=d.outcomeTick+2)watchFinish(localConcession!=0,true,localConcession==0);
        return;
    }
    // A tentative finish must outlive the rollback window and confirmed input.
    if(tick-d.outcomeTick<20||confirmed<d.outcomeTick+2)return;
    int winner=(bits&3)==1?0:(bits&3)==2?1:2;
    char canonical[256],digest[65];
    snprintf(canonical,sizeof(canonical),"BT4-RANKED-V1|%s|%s|%s|%d",text(d.descriptor,"generation").c_str(),
             d.content.c_str(),text(d.profile,"ruleset").c_str(),winner);
    UpdSha256_Hex(canonical,strlen(canonical),digest);
    d.report={{"match",text(d.descriptor,"generation")},{"outcome",winner},{"digest",digest}};
    if(d.characters.is_object())d.report.update(d.characters);
    d.reportState=1;d.reportSerial=++serial;d.reportAt=now+3000;
    watchFinish(false);
    d.reportPending=rpc("ranked_report",d.report,d.reportSerial);
}

/* UI presence is not a room and does not hold a Realtime subscription. Only
 * authenticated aggregate counts return to the menu, never identities/IPs. */
extern "C" int Port_OnlinePresence(int active){
    static bool wasActive=false,known=false;
    static unsigned request=0,nextSerial=400000;
    static Uint64 next=0;
    static int count=0;
    char buffer[1024];
    while(Bt4Signal_Read(3,buffer,sizeof(buffer))>0){
        Dj event=Dj::parse(buffer,nullptr,false);
        if(!event.is_object()||number(event,"serial")!=request)continue;
        auto body=event.find("body");
        if(body!=event.end()&&body->is_object()&&body->contains("online")&&(*body)["online"].is_number_unsigned()){
            count=(int)std::min(100000u,number(*body,"online"));known=true;
        }
    }
    Uint64 now=SDL_GetTicks();
    if(active&&(!wasActive||now>=next)){
        request=++nextSerial;
        Bt4Signal_Command(3,request,"presence","{\"active\":true}");
        next=now+300000;
    }else if(!active&&wasActive){
        request=++nextSerial;
        Bt4Signal_Command(3,request,"presence","{\"active\":false}");
    }
    wasActive=active!=0;
    return known?count:-1;
}

namespace {
std::mutex rankedLock;
Bt4RankedView rankedView={};
std::array<Bt4RankedEntry,100> rankedEntries={};int rankedTotal=0;
unsigned rankedStatusRequest=0,rankedTopRequest=0,nicknameRequest=0,rankedSerial=800000;
Uint64 nextRankedStatus=0,nextRankedTop=0,rankedStatusDeadline=0,rankedTopDeadline=0,nicknameDeadline=0;
Uint64 nextNicknameCheck=0;
int nicknameState=BT4_NICKNAME_LOADING;
std::string nickname,profileProject;
unsigned cardRequest=0;
Uint64 cardDeadline=0;
int cardState=0;
bool cardAvailable=false;
std::string cardError;
Bt4ProfileCard ownCard={};
unsigned checkoutRequest=0;
Uint64 checkoutDeadline=0;
int checkoutState=0;
std::string checkoutUrl;
struct MediaRequest {std::string id; unsigned request=0; Uint64 until=0; Dj body;};
std::deque<MediaRequest> profileMedia;
std::mutex profileDraftLock;
ProfileImageSource profileDraft;
constexpr Uint64 ProfileTimeout=45000,TopInterval=600000;
void rankedPage(){
    rankedView.pages=std::max(1,(rankedTotal+4)/5);
    rankedView.page=std::max(0,std::min(rankedView.page,rankedView.pages-1));
    rankedView.count=std::min(5,rankedTotal-rankedView.page*5);
    memset(rankedView.top,0,sizeof(rankedView.top));
    for(int i=0;i<rankedView.count;++i)rankedView.top[i]=rankedEntries[rankedView.page*5+i];
}
Bt4PublicDirectory directory={};
unsigned directoryRequest=0;
Uint64 directoryDeadline=0,nextDirectory=0;
void profileRoute(){
    const char *project=Bt4Signal_Project();
    if(profileProject==project)return;
    profileProject=project;
    rankedStatusRequest=rankedTopRequest=nicknameRequest=directoryRequest=0;
    directory={};nextDirectory=0;
    cardRequest=0;cardState=0;cardAvailable=false;cardError.clear();ownCard={};profileMedia.clear();
    checkoutRequest=0;checkoutState=0;checkoutUrl.clear();
    nextRankedStatus=nextRankedTop=0;
    rankedView.state=0;rankedView.rating=rankedView.wins=rankedView.losses=0;
    rankedView.streakScoring=rankedView.streakWins=rankedView.streakPoints=0;
    rankedView.monthlyReset=rankedView.dragonBalls=rankedView.collectionPosition=0;
    rankedView.forfeitPenalty=0;
    rankedView.nextReset[0]=0;
    rankedRocksAvailable=false;
    rankedView.season[0]=0;
    nicknameState=BT4_NICKNAME_LOADING;nickname.clear();
    nextNicknameCheck=0;
}
unsigned profileRequest(const char *op,const char *args){
    unsigned id=++rankedSerial;
    return Bt4Signal_Command(4,id,op,args)?id:0;
}
void profilePump(Uint64 now){
    char buffer[24577];
    while(int read=Bt4Signal_Read(4,buffer,sizeof(buffer))){
        if(read<0)continue;
        Dj event=Dj::parse(buffer,nullptr,false);unsigned id=number(event,"serial");
        auto project=text(event,"project");
        if(!id||(!project.empty()&&project!=profileProject))continue;
        auto body=event.find("body");if(body==event.end()||!body->is_object())continue;
        if(id==directoryRequest){
            directoryRequest=0;
            directory={};
            auto rows=body->find("rooms");
            directory.state=rows!=body->end()&&rows->is_array()?1:-1;
            if(directory.state==1)for(const auto &row:*rows){
                if(!row.is_object()||directory.count>=BT4_PUBLIC_ROOM_LIMIT)continue;
                auto code=text(row,"code"),name=text(row,"name");
                unsigned capacity=number(row,"capacity"),occupancy=number(row,"occupancy"),kind=number(row,"kind");
                if(code.size()!=6||name.empty()||capacity<2||capacity>40||occupancy==0||occupancy>=capacity||kind>2)continue;
                auto &entry=directory.rooms[directory.count++];
                snprintf(entry.code,sizeof(entry.code),"%s",code.c_str());
                snprintf(entry.name,sizeof(entry.name),"%s",name.c_str());
                entry.capacity=int(capacity);entry.occupancy=int(occupancy);entry.kind=int(kind);
            }
            continue;
        }
        if(id==checkoutRequest){
            checkoutRequest=0;
            const auto url=text(*body,"url");
            const std::string kofi=std::string(Bt4Signal_Project())+"/v5/profile/kofi/BT4-";
            const bool kofiUrl=kofi.compare(0,8,"https://")==0 && url.size()==kofi.size()+24 &&
                url.compare(0,kofi.size(),kofi)==0 && url.find_first_not_of("0123456789ABCDEF",kofi.size())==std::string::npos;
            if((url.compare(0,28,"https://checkout.stripe.com/")==0&&url.size()<2048)||kofiUrl){
                checkoutUrl=url;checkoutState=2;
            }else checkoutState=-1;
            continue;
        }
        auto media=std::find_if(profileMedia.begin(),profileMedia.end(),[&](const MediaRequest &entry){return entry.request==id;});
        if(media!=profileMedia.end()){
            media->request=0;
            if(text(*body,"id")==media->id)media->body=std::move(*body);
            continue;
        }
        if(id==cardRequest){
            cardRequest=0;
            const auto premium=body->find("premium");
            if(premium!=body->end()&&premium->is_boolean()){
                ownCard=profileCard(*body);cardAvailable=true;cardError.clear();cardState=BT4_CARD_READY;
            }else{
                cardError=text(*body,"error");
                cardState=cardAvailable?BT4_CARD_SAVE_FAILED:BT4_CARD_FAILED;
            }
            continue;
        }
        if(id==nicknameRequest){
            nicknameRequest=0;
            auto state=text(*body,"state"),error=text(*body,"error");
            if(state=="ready"){
                nickname=text(*body,"name");
                nicknameState=!nickname.empty()&&nickname.size()<=19?BT4_NICKNAME_READY:BT4_NICKNAME_UNAVAILABLE;
            }else if(state=="required")nicknameState=BT4_NICKNAME_REQUIRED;
            else if(state=="optional")nicknameState=BT4_NICKNAME_OPTIONAL;
            else if(state=="disabled"||(error=="BAD_REQUEST"&&nicknameState==BT4_NICKNAME_LOADING))nicknameState=BT4_NICKNAME_DISABLED;
            else if(error=="NICKNAME_TAKEN")nicknameState=BT4_NICKNAME_TAKEN;
            else if(error=="INVALID_NICKNAME")nicknameState=BT4_NICKNAME_INVALID;
            else nicknameState=BT4_NICKNAME_UNAVAILABLE;
            nextNicknameCheck=now+(nicknameState==BT4_NICKNAME_READY?300000:30000);
            continue;
        }
        if(id==rankedStatusRequest){
            rankedStatusRequest=0;
            auto enabled=body->find("enabled");
            rankedView.state=enabled!=body->end()&&enabled->is_boolean()&&enabled->get<bool>()?1:-1;
            const std::string season=text(*body,"season");
            if(!season.empty()&&rankedView.season[0]&&season!=rankedView.season){
                rankedTotal=0;rankedEntries={};rankedView.page=0;rankedView.topState=0;
                rankedTopRequest=0;nextRankedTop=0;rankedPage();
            }
            snprintf(rankedView.season,sizeof(rankedView.season),"%s",season.c_str());
            rankedView.rating=(int)std::min(100000u,number(*body,"rating"));
            rankedView.wins=(int)std::min(100000u,number(*body,"wins"));
            rankedView.losses=(int)std::min(100000u,number(*body,"losses"));
            rankedView.streakScoring=text(*body,"scoring")=="streak";
            rankedView.streakWins=(int)std::min(100000u,number(*body,"streak_wins"));
            rankedView.streakPoints=(int)std::min(100000u,number(*body,"streak_points"));
            rankedView.forfeitPenalty=(int)std::min(100u,number(*body,"forfeit_penalty"));
            auto weekly=body->find("monthly_reset");
            rankedView.monthlyReset=weekly!=body->end()&&weekly->is_boolean()&&weekly->get<bool>();
            rankedRocksAvailable=jsonFlag(*body,"ranked_rocks");
            rankedView.dragonBalls=(int)std::min(7u,number(*body,"dragon_balls"));
            rankedView.collectionPosition=(int)std::min(100000u,number(*body,"collection_position"));
            snprintf(rankedView.nextReset,sizeof(rankedView.nextReset),"%s",text(*body,"next_reset_at").c_str());
        }else if(id==rankedTopRequest){
            rankedTopRequest=0;
            const std::string season=text(*body,"season");
            if(!season.empty()&&rankedView.season[0]&&season!=rankedView.season){nextRankedStatus=nextRankedTop=0;continue;}
            auto entries=body->find("entries");
            if(entries==body->end()||!entries->is_array()){
                if(rankedView.topState==0)rankedView.topState=-1;
                nextRankedTop=now+15000;
                continue;
            }
            rankedTotal=0;rankedView.topState=1;
            snprintf(rankedView.updated,sizeof(rankedView.updated),"%s",text(*body,"updated_at").c_str());
            for(const auto &row:*entries){
                if(!row.is_object()||rankedTotal>=100)continue;
                auto &r=rankedEntries[rankedTotal++];
                snprintf(r.name,sizeof(r.name),"%s",text(row,"name").c_str());
                r.rating=(int)std::min(100000u,number(row,"rating"));
                r.wins=(int)std::min(100000u,number(row,"wins"));
                r.losses=(int)std::min(100000u,number(row,"losses"));
                r.dragonBalls=(int)std::min(7u,number(row,"dragon_balls"));
            }
            rankedPage();
        }
    }
    if(directoryRequest&&now>=directoryDeadline){directoryRequest=0;directory={};directory.state=-1;}
    if(rankedStatusRequest&&now>=rankedStatusDeadline){rankedStatusRequest=0;rankedView.state=-1;}
    if(rankedTopRequest&&now>=rankedTopDeadline){
        rankedTopRequest=0;nextRankedTop=now+15000;
        if(rankedView.topState==0)rankedView.topState=-1;
    }
    if(nicknameRequest&&now>=nicknameDeadline){nicknameRequest=0;nicknameState=BT4_NICKNAME_UNAVAILABLE;nextNicknameCheck=now+15000;}
    if(cardRequest&&now>=cardDeadline){
        cardRequest=0;cardError="TIMEOUT";cardState=cardAvailable?BT4_CARD_SAVE_FAILED:BT4_CARD_FAILED;
    }
    if(checkoutRequest&&now>=checkoutDeadline){checkoutRequest=0;checkoutState=-1;}
}
}
extern "C" int Port_ProfileCheckout(int action,char *url,unsigned capacity){
    std::lock_guard<std::mutex> guard(rankedLock);
    profileRoute();profilePump(SDL_GetTicks());
    if(action<0){checkoutRequest=0;checkoutState=0;checkoutUrl.clear();}
    if(action>0&&!checkoutRequest){
        checkoutRequest=profileRequest("profile_checkout","{}");
        checkoutDeadline=SDL_GetTicks()+15000;checkoutState=checkoutRequest?1:-1;
    }
    const int state=checkoutState;
    if(state==2&&url&&capacity){
        SDL_strlcpy(url,checkoutUrl.c_str(),capacity);checkoutUrl.clear();checkoutState=0;
    }
    return state;
}
extern "C" int Port_ProfileCardView(Bt4ProfileCard *out){
    std::lock_guard<std::mutex> guard(rankedLock);
    profileRoute();profilePump(SDL_GetTicks());*out=ownCard;return cardState;
}
extern "C" int Port_ProfileCardError(char *out,int capacity){
    std::lock_guard<std::mutex> guard(rankedLock);
    if(capacity<=0)return 0;
    SDL_strlcpy(out,cardError.c_str(),size_t(capacity));
    return !cardError.empty();
}
extern "C" void Port_ProfileCardRequest(const Bt4ProfileCard *card){
    std::lock_guard<std::mutex> guard(rankedLock);
    profileRoute();
    if(cardRequest)return;
    Dj args=Dj::object();
    if(card){
        args["card"]={{"name_color",card->nameColor},{"background",card->background},{"decoration",card->decoration}};
        if(card->hasNameRgb)args["card"]["name_rgb"]=card->nameRgb;
        if(card->hasBannerRgb)args["card"]["banner_rgb"]=card->bannerRgb;
    }
    cardRequest=profileRequest(card?"profile_card_save":"profile_card_get",args.dump().c_str());
    cardDeadline=SDL_GetTicks()+20000;
    cardError=cardRequest?"":"SIGNALING_UNAVAILABLE";
    cardState=cardRequest?(cardAvailable?BT4_CARD_SAVING:BT4_CARD_LOADING):
        (cardAvailable?BT4_CARD_SAVE_FAILED:BT4_CARD_FAILED);
}
static int profileMediaSubmit(const Dj &args){
    if(args.is_null()||args.empty())return 0;
    std::lock_guard<std::mutex> guard(rankedLock);
    profileRoute();
    if(cardRequest)return 0;
    cardRequest=profileRequest("profile_media_save",args.dump().c_str());
    cardDeadline=SDL_GetTicks()+20000;
    cardError=cardRequest?"":"SIGNALING_UNAVAILABLE";
    cardState=cardRequest?(cardAvailable?BT4_CARD_SAVING:BT4_CARD_LOADING):
        (cardAvailable?BT4_CARD_SAVE_FAILED:BT4_CARD_FAILED);
    return cardRequest!=0;
}
extern "C" int Port_ProfileMediaSave(const char *kind,const char *path){
    return profileMediaSubmit(path?profileImage(path,kind):Dj{{"kind",kind},{"remove",true}});
}
extern "C" int Port_ProfileDraftFile(const char *kind,const char *path){
    int error=-1;
    auto image=loadProfileImage(path,kind,&error);
    if(image.pixels.empty())return error;
    std::lock_guard<std::mutex> guard(profileDraftLock);
    profileDraft=std::move(image);
    return 1;
}
extern "C" int Port_ProfileDraftRoster(int character){
    ProfileImageSource image;
    image.kind="avatar";image.width=256;image.height=512;image.delays={200};
    image.pixels.resize(256*512*4);
    if(!Port_ProfileRosterPortrait(character,image.pixels.data()))return 0;
    std::lock_guard<std::mutex> guard(profileDraftLock);
    profileDraft=std::move(image);
    return 1;
}
extern "C" int Port_ProfileDraftPreview(float x,float y,float zoom,unsigned char *rgba,int *width,int *height,int *count,int delays[6]){
    std::lock_guard<std::mutex> guard(profileDraftLock);
    if(profileDraft.pixels.empty())return 0;
    const bool banner=profileDraft.kind=="banner"||profileDraft.kind=="banner_motion";
    const int frames=int(profileDraft.delays.size());
    *count=std::min(frames,6);
    *width=(*count>1)?128:BT4_PROFILE_AVATAR_MAX;
    *height=banner?*width/4:*width;
    const size_t stride=size_t(*width)*(*height)*4;
    for(int i=0;i<*count;++i){
        const int from=i*frames/(*count),until=(i+1)*frames/(*count);
        profileCrop(profileDraft,rgba+i*stride,*width,*height,4,x,y,zoom,from);
        delays[i]=profileFrameDuration(profileDraft,from,until);
    }
    return 1;
}
extern "C" int Port_ProfileDraftUpload(float x,float y,float zoom){
    Dj args;
    {
        std::lock_guard<std::mutex> guard(profileDraftLock);
        args=encodeProfileImage(profileDraft,x,y,zoom);
    }
    return profileMediaSubmit(args);
}
extern "C" void Port_ProfileDraftReset(){
    std::lock_guard<std::mutex> guard(profileDraftLock);
    profileDraft={};
}
extern "C" int Port_ProfileMediaRead(const char *id,unsigned char *pixels,int *width,int *height,int *count,int delays[6]){
    if(!id||!*id)return 0;
    if(getenv("BT4_RENDER_TEST")&&(!strcmp(id,"preview-local")||!strcmp(id,"preview-rival"))){
        const char *path=getenv(!strcmp(id,"preview-local")?"BT4_RANKED_PREVIEW_LOCAL":"BT4_RANKED_PREVIEW_RIVAL");
        if(!path)return 0;
        const Dj body=profileImage(path,"avatar");
        if(body.is_null())return 0;
        *count=1;delays[0]=100;
        if(body.contains("detail"))
            return profileDecodeAvatar(body["detail"].get_ref<const std::string &>(),pixels,width,height);
        profileImageDimensions("avatar",*width,*height);
        return profilePixels(body["frames"][0].get_ref<const std::string &>(),pixels,size_t(*width)*(*height)*4);
    }
    std::lock_guard<std::mutex> guard(rankedLock);
    const Uint64 now=SDL_GetTicks();
    profileRoute();profilePump(now);
    auto entry=std::find_if(profileMedia.begin(),profileMedia.end(),[&](const MediaRequest &media){return media.id==id;});
    if(entry==profileMedia.end()){
        if(profileMedia.size()>=48)profileMedia.pop_front();
        profileMedia.push_back({id,0,0,{}});
        entry=std::prev(profileMedia.end());
    }
    if(now>=entry->until&&entry->body.is_null()){
        entry->request=profileRequest("profile_media_get",Dj{{"id",id},{"hd",true},{"detail",true}}.dump().c_str());
        entry->until=now+15000;
    }
    if(entry->body.is_null())return 0;
    const auto &body=entry->body;
    const std::string kind=text(body,"kind");
    const auto detail=body.find("detail");
    if(kind=="avatar"&&detail!=body.end()&&detail->is_string()&&
       profileDecodeAvatar(detail->get_ref<const std::string &>(),pixels,width,height)){
        *count=1;delays[0]=100;
        return 1;
    }
    int w,h;
    profileImageDimensions(kind,w,h);
    const auto frames=body.find("frames"),timing=body.find("delays");
    if(frames==body.end()||!frames->is_array()||frames->empty()||frames->size()>((kind=="motion"||kind=="banner_motion")?6u:1u)||
       timing==body.end()||!timing->is_array()||timing->size()!=frames->size())return 0;
    if(kind=="avatar"&&(*frames)[0].is_string()&&(*frames)[0].get_ref<const std::string &>().size()==2732)
        w=h=32;
    const size_t frameBytes=size_t(w)*h*4;
    for(size_t i=0;i<frames->size();++i){
        if(!(*frames)[i].is_string()||!(*timing)[i].is_number_unsigned()||
            !profilePixels((*frames)[i].get<std::string>(),pixels+i*frameBytes,frameBytes))return 0;
        delays[i]=int(std::clamp((*timing)[i].get<unsigned>(),100u,1000u));
    }
    *width=w;*height=h;*count=int(frames->size());return 1;
}
extern "C" void Bt4Discovery_Directory(int protocol,uint64_t content,uint64_t tagContent,Bt4PublicDirectory *out){
    std::lock_guard<std::mutex> guard(rankedLock);
    Uint64 now=SDL_GetTicks();
    profileRoute();
    profilePump(now);
    if(!directoryRequest&&now>=nextDirectory){
        char key[17],tag[17];
        snprintf(key,sizeof(key),"%016llx",(unsigned long long)content);
        snprintf(tag,sizeof(tag),"%016llx",(unsigned long long)tagContent);
        Dj args={{"wire","bt4-duel-"+std::to_string(protocol)},{"content",key},{"tag_content",tag}};
        directoryRequest=profileRequest("public_rooms",args.dump().c_str());
        nextDirectory=now+12000;directoryDeadline=now+ProfileTimeout;
        if(!directoryRequest){directory={};directory.state=-1;}
    }
    *out=directory;
}
extern "C" void Port_RankedPage(int direction){
    std::lock_guard<std::mutex> guard(rankedLock);
    if(direction)rankedView.page+=direction<0?-1:1;
    else rankedView.page=0;
    rankedPage();
}
extern "C" void Port_RankedRefresh(int refreshStatus){
    std::lock_guard<std::mutex> guard(rankedLock);
    Uint64 now=SDL_GetTicks();
    profileRoute();profilePump(now);
    const char *args="{\"ruleset\":\"solo\"}";
    if(refreshStatus&&!rankedStatusRequest&&now>=nextRankedStatus){
        rankedStatusRequest=profileRequest("ranked_status",args);
        nextRankedStatus=now+15000;rankedStatusDeadline=now+ProfileTimeout;
        if(!rankedStatusRequest)rankedView.state=-1;
    }
    if(!rankedTopRequest&&now>=nextRankedTop){
        rankedTopRequest=profileRequest("ranked_top",args);
        nextRankedTop=now+(rankedTopRequest?TopInterval:15000);rankedTopDeadline=now+ProfileTimeout;
        if(!rankedTopRequest&&rankedView.topState==0)rankedView.topState=-1;
    }
}
extern "C" void Port_RankedView(Bt4RankedView *out){
    if(!out)return;
    std::lock_guard<std::mutex> guard(rankedLock);
    profilePump(SDL_GetTicks());
    *out=rankedView;
}
extern "C" int Port_RankedTopTen(Bt4RankedEntry out[10]){
    std::lock_guard<std::mutex> guard(rankedLock);
    if(rankedView.topState!=1)return rankedView.topState<0?-1:-2;
    int count=std::min(10,rankedTotal);
    std::copy_n(rankedEntries.begin(),count,out);
    return count;
}
extern "C" void Port_OnlineNicknameBegin(void){
    std::lock_guard<std::mutex> guard(rankedLock);
    profileRoute();
    if(nicknameState==BT4_NICKNAME_SAVING)return;
    nicknameRequest=0;nicknameState=BT4_NICKNAME_LOADING;
}
extern "C" int Port_OnlineNicknameView(char name[20]){
    std::lock_guard<std::mutex> guard(rankedLock);
    Uint64 now=SDL_GetTicks();
    profileRoute();profilePump(now);
    bool recheck=(nicknameState==BT4_NICKNAME_OPTIONAL||nicknameState==BT4_NICKNAME_REQUIRED||nicknameState==BT4_NICKNAME_UNAVAILABLE)&&now>=nextNicknameCheck;
    if(!nicknameRequest&&(nicknameState==BT4_NICKNAME_LOADING||recheck)){
        nicknameRequest=profileRequest("nickname_get","{}");nicknameDeadline=now+ProfileTimeout;
        if(!nicknameRequest){nicknameState=BT4_NICKNAME_UNAVAILABLE;nextNicknameCheck=now+15000;}
    }
    if(name)snprintf(name,20,"%s",nickname.c_str());
    return nicknameState;
}
extern "C" int Port_OnlineNicknameSubmit(const char *name){
    std::lock_guard<std::mutex> guard(rankedLock);
    if(nicknameRequest||!name||!*name)return 0;
    std::string args=Dj{{"name",name}}.dump(-1,' ',false,Dj::error_handler_t::replace);
    nicknameRequest=profileRequest("nickname_set",args.c_str());nicknameDeadline=SDL_GetTicks()+ProfileTimeout;
    nicknameState=nicknameRequest?BT4_NICKNAME_SAVING:BT4_NICKNAME_UNAVAILABLE;
    return nicknameRequest!=0;
}

#include "match_characters.inc"
