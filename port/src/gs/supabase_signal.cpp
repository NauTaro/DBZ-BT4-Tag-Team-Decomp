/* Supabase Auth + PostgREST + private Realtime inbox. No game/audio relay API.
 * Network work is owned by one background thread. Credentials are never logged.
 */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <deque>
#include <array>
#include <mutex>
#include <atomic>
#include <memory>
#include <algorithm>
#ifndef _WIN32
#include <sys/file.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif
#include "supabase_signal.h"
#include "voice_platform.inc"
#define JSON_NOEXCEPTION
#include "../../third_party/voice/json.hpp"
#ifdef _WIN32
#include "voice_transport.inc"
#else
#include "voice_transport_curl.inc"
#endif
using Sj=nlohmann::json;
namespace {
std::mutex lifecycle;
constexpr const char *project="https://mhgksaiitqoijnatpurf.supabase.co";
constexpr const char *publishable="sb_publishable_T2WjPRgHLMAVgRC6juKLqg_BoKiSgww";
std::string str(const Sj &j,const char *key){auto i=j.find(key);return i!=j.end()&&i->is_string()?i->get<std::string>():std::string();}
struct Job{int consumer;unsigned serial;std::string op;Sj args;};
struct Identity{std::string access,refresh,user;Uint64 renewAt=0;} identity;
std::string identityFile;
#ifdef _WIN32
HANDLE identityLock=INVALID_HANDLE_VALUE;
static std::wstring pathWide(const std::string &path){int size=MultiByteToWideChar(CP_UTF8,0,path.c_str(),-1,nullptr,0);if(size<=0)return {};std::wstring wide(size,0);MultiByteToWideChar(CP_UTF8,0,path.c_str(),-1,&wide[0],size);wide.resize(size-1);return wide;}
static void unlockIdentity(){if(identityLock!=INVALID_HANDLE_VALUE)CloseHandle(identityLock);identityLock=INVALID_HANDLE_VALUE;}
#else
int identityLock=-1;
static void unlockIdentity(){if(identityLock>=0)close(identityLock);identityLock=-1;}
#endif
static bool identitySlot(){
    if(!identityFile.empty())return true;std::string folder;
    const char *override=SDL_getenv("BT4_SIGNAL_SESSION_DIR");
    if(override&&*override)folder=override;
    else {char *pref=SDL_GetPrefPath("NauTaro","BT4decomp");if(!pref)return false;folder=std::string(pref)+"network";SDL_free(pref);}
    if(!SDL_CreateDirectory(folder.c_str()))return false;
    for(int slot=0;slot<16;++slot){std::string file=folder+"/mhgksaiitqoijnatpurf-"+std::to_string(slot);
#ifdef _WIN32
        auto wide=pathWide(file+".lock");identityLock=CreateFileW(wide.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(identityLock==INVALID_HANDLE_VALUE)continue;
#else
        identityLock=open((file+".lock").c_str(),O_RDWR|O_CREAT,0600);if(identityLock<0)continue;
        if(flock(identityLock,LOCK_EX|LOCK_NB)){close(identityLock);identityLock=-1;continue;}
#endif
        identityFile=file+".json";atexit(unlockIdentity);
        if(auto input=SDL_IOFromFile(identityFile.c_str(),"rb")){Sint64 size=SDL_GetIOSize(input);if(size>0&&size<4096){std::string data((size_t)size,0);if(SDL_ReadIO(input,&data[0],data.size())==data.size()){
            auto saved=Sj::parse(data,nullptr,false);if(saved.is_object()){auto token=str(saved,"refresh_token");if(token.size()<=1024)identity.refresh=token;}}}SDL_CloseIO(input);}
        return true;
    }return false;
}
static void saveIdentity(){
    if(identityFile.empty())return;std::string temp=identityFile+".tmp",data=Sj{{"refresh_token",identity.refresh}}.dump();
    auto out=SDL_IOFromFile(temp.c_str(),"wb");if(!out)return;bool good=SDL_WriteIO(out,data.data(),data.size())==data.size();good=SDL_CloseIO(out)&&good;if(!good)return;
#ifdef _WIN32
    auto a=pathWide(temp),b=pathWide(identityFile);MoveFileExW(a.c_str(),b.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
#else
    chmod(temp.c_str(),0600);rename(temp.c_str(),identityFile.c_str());
#endif
}
struct State {
    std::mutex lock;SDL_Thread *thread=nullptr;SDL_Semaphore *wake=nullptr;
    std::atomic<bool> quit{false};std::atomic<int> status{0};
    unsigned consumers=0;
    std::deque<Job> jobs;std::array<std::deque<std::string>,3> events;
} state;
static void wake(void*){if(state.wake)SDL_SignalSemaphore(state.wake);}
static bool emit(int consumer,const Sj &j){
    if(consumer<0||consumer>2)return false;
    std::string body=j.dump();if(body.size()>24576)return false;
    std::lock_guard<std::mutex> g(state.lock);
    if(!(state.consumers&(1u<<consumer)))return true;
    if(state.events[consumer].size()>=128){state.status=-1;return false;}
    state.events[consumer].push_back(std::move(body));return true;
}
static bool http(const std::string &path,const Sj &body,bool authenticated,Sj &answer){
    const std::string data=body.dump();std::string result;long code=0;
    if(data.size()>20000)return false;
#ifdef _WIN32
    HINTERNET session=WinHttpOpen(L"BT4decomp signaling",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
    if(!session)return false;WinHttpSetTimeouts(session,4000,4000,4000,8000);
    HINTERNET connection=WinHttpConnect(session,L"mhgksaiitqoijnatpurf.supabase.co",443,0);
    std::wstring wide(path.begin(),path.end());
    HINTERNET request=connection?WinHttpOpenRequest(connection,L"POST",wide.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE):nullptr;
    std::string headers="Content-Type: application/json\r\napikey: "+std::string(publishable)+"\r\n";
    if(authenticated)headers+="Authorization: Bearer "+identity.access+"\r\n";
    std::wstring wh(headers.begin(),headers.end());
    bool ok=request&&WinHttpSendRequest(request,wh.c_str(),(DWORD)wh.size(),(void*)data.data(),(DWORD)data.size(),(DWORD)data.size(),0)&&WinHttpReceiveResponse(request,nullptr);
    if(ok){DWORD status=0,size=sizeof(status),got;ok=WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX);code=status;
        char bytes[4096];while(ok){if(!WinHttpReadData(request,bytes,sizeof(bytes),&got)){ok=false;break;}if(!got)break;if(result.size()+got>65536){ok=false;break;}result.append(bytes,got);}}
    if(request)WinHttpCloseHandle(request);if(connection)WinHttpCloseHandle(connection);WinHttpCloseHandle(session);
    if(!ok)return false;
#else
    CURL *request=curl_easy_init();if(!request)return false;curl_slist *headers=nullptr;
    headers=curl_slist_append(headers,"Content-Type: application/json");
    headers=curl_slist_append(headers,("apikey: "+std::string(publishable)).c_str());
    if(authenticated)headers=curl_slist_append(headers,("Authorization: Bearer "+identity.access).c_str());
    std::string url=std::string(project)+path;
    curl_easy_setopt(request,CURLOPT_URL,url.c_str());curl_easy_setopt(request,CURLOPT_HTTPHEADER,headers);
    curl_easy_setopt(request,CURLOPT_POSTFIELDS,data.c_str());curl_easy_setopt(request,CURLOPT_POSTFIELDSIZE,(long)data.size());
    curl_easy_setopt(request,CURLOPT_TIMEOUT,8L);curl_easy_setopt(request,CURLOPT_CONNECTTIMEOUT,4L);curl_easy_setopt(request,CURLOPT_NOSIGNAL,1L);
    curl_easy_setopt(request,CURLOPT_WRITEFUNCTION,+[](char *data,size_t size,size_t n,void *p)->size_t{auto &out=*static_cast<std::string*>(p);if(n>65536||size>65536||size*n>65536-out.size())return 0;out.append(data,size*n);return size*n;});
    curl_easy_setopt(request,CURLOPT_WRITEDATA,&result);
    auto rc=curl_easy_perform(request);curl_easy_getinfo(request,CURLINFO_RESPONSE_CODE,&code);
    curl_easy_cleanup(request);curl_slist_free_all(headers);if(rc!=CURLE_OK)return false;
#endif
    answer=Sj::parse(result,nullptr,false);
    return code>=200&&code<300&&!answer.is_discarded();
}
static bool authenticate(){
    if(!identity.access.empty()&&SDL_GetTicks()<identity.renewAt)return true;
    if(!identitySlot())return false;
    Sj reply;bool refreshing=!identity.refresh.empty();
    if(!http(refreshing?"/auth/v1/token?grant_type=refresh_token":"/auth/v1/signup",refreshing?Sj{{"refresh_token",identity.refresh}}:Sj{{"data",{{"client","BT4decomp"}}}},false,reply)){
        auto code=reply.is_object()?str(reply,"error_code"):std::string();
        if(!refreshing||(code!="refresh_token_not_found"&&code!="refresh_token_already_used"))return false;
        identity.refresh.clear();if(!http("/auth/v1/signup",{{"data",{{"client","BT4decomp"}}}},false,reply))return false;
    }
    std::string access=str(reply,"access_token"),refresh=str(reply,"refresh_token");auto user=reply.find("user");
    if(access.empty()||access.size()>8192||refresh.empty()||refresh.size()>1024||user==reply.end()||!user->is_object())return false;
    std::string id=str(*user,"id");if(id.size()!=36)return false;
    identity.access=std::move(access);identity.refresh=std::move(refresh);identity.user=std::move(id);
    unsigned ttl=300;auto duration=reply.find("expires_in");if(duration!=reply.end()&&duration->is_number_unsigned())ttl=duration->get<unsigned>();
    if(ttl>3600)ttl=3600;if(ttl<120)ttl=120;identity.renewAt=SDL_GetTicks()+(Uint64)(ttl-60)*1000;saveIdentity();return true;
}
static int entry(void*){
    std::unique_ptr<VoiceWs> ws;unsigned serial=0;std::string joinRef;bool joined=false;
    Uint64 nextBeat=0,joinAt=0,reconnect=0;unsigned failures=0;
    auto post=[&](const char *event,const Sj &payload,const std::string &topic){std::string ref=std::to_string(++serial);if(ws)ws->text(Sj{{"topic",topic},{"event",event},{"payload",payload},{"ref",ref},{"join_ref",joinRef}}.dump());return ref;};
    while(!state.quit){
        Uint64 now=SDL_GetTicks();
        if(!ws&&now>=reconnect){
            if(!authenticate()){state.status=-1;reconnect=now+30000;continue;}
            std::string url=std::string(project)+"/realtime/v1/websocket?apikey="+publishable+"&vsn=1.0.0";
            ws=std::make_unique<VoiceWs>(url,"",0,"",true,wake,nullptr);joined=false;joinAt=0;state.status=0;
        }
        if(ws&&ws->status<0){ws.reset();joined=false;state.status=0;failures=std::min(5u,failures+1);reconnect=now+(1000u<<failures);}
        if(ws&&ws->status==1&&!joinAt){joinRef=std::to_string(serial+1);post("phx_join",{{"config",{{"private",true},{"broadcast",{{"ack",false},{"self",false}}},{"presence",{{"enabled",false}}},{"postgres_changes",Sj::array()}}},{"access_token",identity.access}},"realtime:bt4:inbox:"+identity.user);joinAt=now;nextBeat=now+20000;}
        VoiceWsMessage message;unsigned count=0;
        while(ws&&count++<128&&ws->read(message)){
            if(message.binary||message.bytes.size()>24576){state.status=-1;continue;}
            Sj j=Sj::parse(message.bytes.begin(),message.bytes.end(),nullptr,false);if(!j.is_object())continue;
            auto payload=j.find("payload");if(payload==j.end()||!payload->is_object())continue;
            if(str(j,"event")=="phx_reply"&&str(j,"ref")==joinRef){joined=str(*payload,"status")=="ok";state.status=joined?1:-1;failures=0;
                if(joined)for(int consumer=0;consumer<3;++consumer)emit(consumer,{{"event","bt4.ready"},{"body",Sj::object()}});}
            if(str(j,"event")=="broadcast"&&joined){
                auto event=str(*payload,"event");auto body=payload->find("payload");if(body==payload->end()||!body->is_object())continue;
                if(event=="bt4.signal"){auto channel=str(*body,"channel");int consumer=channel=="tag"?1:channel=="voice"?2:channel=="duel"?0:-1;if(consumer>=0)emit(consumer,{{"event",event},{"body",*body}});}
                else if(event=="bt4.room"||event=="bt4.closed")for(int consumer=0;consumer<3;++consumer)emit(consumer,{{"event",event},{"body",*body}});
            }
        }
        if(ws&&ws->status==1&&joinAt&&!joined&&now>joinAt+10000){ws.reset();state.status=-1;reconnect=now+30000;}
        if(joined&&now>=nextBeat){post("heartbeat",Sj::object(),"phoenix");nextBeat=now+20000;}
        if(joined&&now>=identity.renewAt){if(authenticate())post("access_token",{{"access_token",identity.access}},"realtime:bt4:inbox:"+identity.user);else{ws.reset();joined=false;state.status=-1;reconnect=now+30000;}}
        Job job;bool have=false;
        if(joined){std::lock_guard<std::mutex> g(state.lock);if(!state.jobs.empty()){job=std::move(state.jobs.front());state.jobs.pop_front();have=true;}}
        if(have){Sj reply;if(!http("/rest/v1/rpc/bt4_api",{{"p_op",job.op},{"p_args",job.args}},true,reply))reply={{"error","SIGNALING_UNAVAILABLE"}};
            emit(job.consumer,{{"event","reply"},{"serial",job.serial},{"body",reply}});}
        SDL_WaitSemaphoreTimeout(state.wake,have?0:20);
    }
    ws.reset();state.status=0;return 0;
}
}
extern "C" const char *Bt4Signal_Project(void){return project;}
extern "C" int Bt4Signal_Open(void){
    std::lock_guard<std::mutex> lifecycleGuard(lifecycle);
    if(state.thread)return 1;state.quit=false;state.status=0;state.wake=SDL_CreateSemaphore(0);if(!state.wake)return 0;
    state.thread=SDL_CreateThread(entry,"BT4 Supabase signaling",nullptr);if(!state.thread){SDL_DestroySemaphore(state.wake);state.wake=nullptr;return 0;}return 1;
}
extern "C" int Bt4Signal_Status(void){return state.status;}
extern "C" void Bt4Signal_Listen(int consumer,int enabled){if(consumer<0||consumer>2)return;std::lock_guard<std::mutex> g(state.lock);if(enabled)state.consumers|=1u<<consumer;else{state.consumers&=~(1u<<consumer);state.events[consumer].clear();}}
extern "C" int Bt4Signal_Command(int consumer,unsigned serial,const char *op,const char *args){
    if(consumer<0||consumer>2||!op||!args||strlen(args)>18000)return 0;
    static const char *allowed[]={"create","join","get","lease","leave","list","queue","cancel","signal","counts","voice"};bool found=false;
    for(auto kind:allowed)if(!strcmp(op,kind)){found=true;break;}if(!found)return 0;
    Sj body=Sj::parse(args,nullptr,false);if(!body.is_object())return 0;
    if(!Bt4Signal_Open())return 0;std::lock_guard<std::mutex> g(state.lock);if(state.jobs.size()>=64)return 0;state.consumers|=1u<<consumer;
    state.jobs.push_back({consumer,serial,op,std::move(body)});wake(nullptr);return 1;
}
extern "C" int Bt4Signal_Read(int consumer,char *out,size_t capacity){
    if(consumer<0||consumer>2||!out||!capacity)return 0;std::lock_guard<std::mutex> g(state.lock);
    auto &events=state.events[consumer];if(events.empty())return 0;if(events.front().size()+1>capacity)return -1;
    memcpy(out,events.front().c_str(),events.front().size()+1);events.pop_front();return 1;
}
extern "C" void Bt4Signal_Close(void){
    std::lock_guard<std::mutex> lifecycleGuard(lifecycle);
    state.quit=true;wake(nullptr);if(state.thread)SDL_WaitThread(state.thread,nullptr);state.thread=nullptr;
    if(state.wake)SDL_DestroySemaphore(state.wake);state.wake=nullptr;std::lock_guard<std::mutex> g(state.lock);state.jobs.clear();state.consumers=0;for(auto &q:state.events)q.clear();
}
