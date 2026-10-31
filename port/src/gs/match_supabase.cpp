/* Adapts existing UDP discovery. Native duel rollback and lobby/tournament
 * authority remain in net.c/net_lobby.c, on the participants' machines.
 */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <string>
#include <array>
#include <map>
#include <mutex>
#include "match_supabase.h"
#include "supabase_signal.h"
#include "voice.h"
#define JSON_NOEXCEPTION
#include "../../third_party/voice/json.hpp"
using Dj=nlohmann::json;
namespace {
std::mutex lock;unsigned serial=20000;
std::string text(const Dj &j,const char *key){auto p=j.find(key);return p!=j.end()&&p->is_string()?p->get<std::string>():std::string();}
unsigned number(const Dj &j,const char *key){auto p=j.find(key);return p!=j.end()&&p->is_number_unsigned()?p->get<unsigned>():0;}
struct Discovery {
    unsigned generation=0,init=0,countsRequest=0,reconcile=0;bool on=false,waiting=false;
    Bt4DiscoveryStatus status={};Dj descriptor,profile,localAddresses;std::string wire,content;
    std::map<unsigned,Dj> addresses;std::map<unsigned,bool> announced;
    Uint64 leaseAt=0,began=0,lastCounts=0;std::array<int,5> counts={};bool countsKnown=false;
} d;
bool rpc(const char *op,const Dj &args,unsigned id=0){return Bt4Signal_Command(0,id?id:++serial,op,args.dump().c_str());}
void error(const std::string &code){
    if(d.status.state==3)return;d.status.state=-1;
    const char *message=code=="ROOM_FULL"?"Someone has already joined that room.":code=="ROOM_NOT_FOUND"?"No room with that code.":
        code=="INCOMPATIBLE_DATA"?"The host's game uses incompatible battle data.":code=="RATE_LIMITED"?"Too many requests. Wait a moment, then try again.":
        "The matchmaking service could not be reached (is the network up?).";
    snprintf(d.status.error,sizeof(d.status.error),"%s",message);
}
void refreshStatus(){
    if(!d.descriptor.is_object())return;unsigned self=number(d.descriptor,"self");bool host=self==1;
    d.status.role=host?1:2;d.status.lobby=text(d.descriptor,"mode")!="duel";
    snprintf(d.status.code,sizeof(d.status.code),"%s",text(d.descriptor,"code").c_str());
    auto members=d.descriptor.find("members");if(members==d.descriptor.end()||!members->is_array())return;
    unsigned target=0;
    if(!host)target=1;
    else if(!d.status.lobby)for(const auto &member:*members)if(number(member,"peer")!=self&&text(member,"role")=="player"){target=number(member,"peer");break;}
    auto remote=d.addresses.find(target);
    if(target&&remote!=d.addresses.end()&&remote->second.is_array()&&!remote->second.empty()){
        d.status.peerCount=0;for(const auto &value:remote->second)if(value.is_string()&&value.get_ref<const std::string&>().size()<64&&d.status.peerCount<4)
            snprintf(d.status.peerAddr[d.status.peerCount++],64,"%s",value.get_ref<const std::string&>().c_str());
        for(const auto &member:*members)if(number(member,"peer")==target)snprintf(d.status.peerName,sizeof(d.status.peerName),"%s",text(member,"name").c_str());
        if(d.status.peerCount)d.status.state=3;
    }else if(host)d.status.state=2;
    if(host){char previous[32][64];memcpy(previous,d.status.watchAddr,sizeof(previous));int old=d.status.watchCount;d.status.watchCount=0;memset(d.status.watchAddr,0,sizeof(d.status.watchAddr));
        for(const auto &member:*members){unsigned peer=number(member,"peer");if(peer==self||(!d.status.lobby&&text(member,"role")!="spectator"))continue;
            auto p=d.addresses.find(peer);if(p==d.addresses.end()||!p->second.is_array())continue;
            for(const auto &address:p->second)if(address.is_string()&&address.get_ref<const std::string&>().size()<64&&d.status.watchCount<32)
                snprintf(d.status.watchAddr[d.status.watchCount++],64,"%s",address.get_ref<const std::string&>().c_str());}
        if(old!=d.status.watchCount||memcmp(previous,d.status.watchAddr,sizeof(previous)))++d.status.watchGen;
    }
}
void metadata(const Dj &body){
    auto generation=text(body,"generation");if(generation.empty())return;
    if(d.descriptor.is_object()&&!text(d.descriptor,"generation").empty()&&generation!=text(d.descriptor,"generation"))return;
    d.descriptor=body;d.waiting=false;auto members=body.find("members");if(members==body.end()||!members->is_array())return;
    PortVoice_P2PSession(1,body.dump().c_str(),nullptr,0);
    unsigned self=number(body,"self");std::string nonce;for(char c:generation)if(c!='-')nonce+=c;
    for(const auto &member:*members){unsigned peer=number(member,"peer");if(!peer||peer==self||d.announced[peer])continue;
        if(rpc("signal",{{"generation",generation},{"to",peer},{"channel","duel"},{"kind","udp"},{"body",{{"addresses",d.localAddresses},{"nonce",nonce}}}}))d.announced[peer]=true;}
    refreshStatus();
}
void pump(){
    char buffer[24577];unsigned work=0;
    while(work++<64&&Bt4Signal_Read(0,buffer,sizeof(buffer))>0){Dj event=Dj::parse(buffer,nullptr,false);if(!event.is_object())continue;auto body=event.find("body");if(body==event.end())continue;
        auto kind=text(event,"event");
        if(kind=="reply"){
            unsigned reply=number(event,"serial");
            if(reply==d.countsRequest&&body->is_object()&&!body->contains("error")){const char *keys[]={"S","T","D10","D15","D20"};for(int i=0;i<5;++i)d.counts[i]=(int)number(*body,keys[i]);d.countsKnown=true;}
            else if((reply==d.init||reply==d.reconcile)&&body->is_object()){
                if(body->contains("error")){error(text(*body,"error"));continue;}
                if(text(*body,"state")=="waiting")d.waiting=true;else metadata(*body);
            }
        }else if(kind=="bt4.ready"&&d.on&&d.descriptor.is_object()){
            d.announced.clear();d.reconcile=++serial;rpc("get",Dj::object(),d.reconcile);
        }else if(kind=="bt4.room"&&d.on&&body->is_object())metadata(*body);
        else if(kind=="bt4.signal"&&d.on&&body->is_object()&&text(*body,"channel")=="duel"&&text(*body,"kind")=="udp"&&text(*body,"generation")==text(d.descriptor,"generation")){
            auto value=body->find("body");unsigned from=number(*body,"from");if(value!=body->end()&&value->is_object()&&value->contains("addresses")){
                d.addresses[from]=(*value)["addresses"];refreshStatus();}
        }
    }
}
}
extern "C" int Bt4Discovery_Begin(unsigned generation,int role,int protocol,uint64_t content,const char *name,const char *code,const char *addresses,const char *queue){
    std::lock_guard<std::mutex> g(lock);if(generation<d.generation||!name||!addresses)return 0;
    Dj local=Dj::parse(addresses,nullptr,false);if(!local.is_array()||local.empty()||local.size()>4)return 0;
    d.generation=generation;d.on=true;d.waiting=false;d.status={};d.status.state=1;d.descriptor=Dj();d.addresses.clear();d.announced.clear();d.localAddresses=local;
    char hash[17];snprintf(hash,sizeof(hash),"%016llx",(unsigned long long)content);d.content=hash;d.wire="bt4-duel-"+std::to_string(protocol);
    d.profile={{"mode",role==5?"lobby":"duel"},{"wire",d.wire},{"content",d.content},{"rules",queue&&*queue?queue:"S"},{"name",std::string(name).substr(0,19)},{"capacity",8}};
    d.init=++serial;d.began=SDL_GetTicks();d.leaseAt=d.began+60000;
    if(!Bt4Signal_Open())return 0;
    // Explicit reset precedes the new admission in the single HTTP queue.
    rpc("leave",Dj::object());
    if(role==1||role==5)return rpc("create",d.profile,d.init);
    if(role==3)return rpc("queue",d.profile,d.init);
    return rpc("join",{{"code",code?code:""},{"wire",d.wire},{"content",d.content},{"name",d.profile["name"]},{"role",role==4?"spectator":"player"}},d.init);
}
extern "C" int Bt4Discovery_Poll(unsigned generation,Bt4DiscoveryStatus *out){
    std::lock_guard<std::mutex> g(lock);if(!out||!d.on||generation!=d.generation)return 0;pump();Uint64 now=SDL_GetTicks();
    if(now>=d.leaseAt){d.leaseAt=now+60000;if(d.waiting){d.init=++serial;rpc("queue",d.profile,d.init);}else if(number(d.descriptor,"self")==1)rpc("lease",Dj::object());}
    if(d.status.state==1&&Bt4Signal_Status()<0&&now-d.began>15000)error("SIGNALING_UNAVAILABLE");
    *out=d.status;return 1;
}
extern "C" void Bt4Discovery_End(unsigned generation){std::lock_guard<std::mutex> g(lock);if(!d.on||generation!=d.generation)return;
    if(Bt4Signal_Status()==1){if(d.waiting)rpc("cancel",Dj::object());else if(d.descriptor.is_object())rpc("leave",{{"generation",text(d.descriptor,"generation")}});}d.on=false;}
extern "C" int Bt4Discovery_Counts(int protocol,uint64_t content,int out[5]){
    std::lock_guard<std::mutex> g(lock);if(!out)return 0;pump();Uint64 now=SDL_GetTicks();
    if(!d.lastCounts||now-d.lastCounts>=30000){d.lastCounts=now;char hash[17];snprintf(hash,sizeof(hash),"%016llx",(unsigned long long)content);d.countsRequest=++serial;
        rpc("counts",{{"wire","bt4-duel-"+std::to_string(protocol)},{"content",hash}},d.countsRequest);}
    for(int i=0;i<5;++i)out[i]=d.counts[i];return d.countsKnown;
}
