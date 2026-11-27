"""Exercise the actual signaling loop with a blocked socket and deterministic HTTPS."""
from pathlib import Path
import os,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[2]
class TransportFallbackTests(unittest.TestCase):
    def test_socket_failure_and_rate_limit_do_not_starve_https_jobs(self):
        text=(ROOT/'port/src/gs/cloudflare_signal.inc').read_text(encoding='utf8')
        entry=text[text.index('static int cloudEntry('):]
        signal=(ROOT/'port/src/gs/supabase_signal.cpp').read_text(encoding='utf8')
        emit=signal[signal.index('static bool emit('):signal.index('static bool http(')]
        read=signal[signal.index('extern "C" int Bt4Signal_Read('):signal.index('extern "C" void Bt4Signal_Close(')]
        fixture=r'''
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "JSON_HEADER"
#include "PROFILE_HEADER"
using Sj=nlohmann::json;using Uint64=uint64_t;
#define BT4_CANONICAL_VERSION "0.1.39.6-R1.5"
struct Job{int consumer=4;unsigned serial=1;std::string op="nickname_get";Sj args=Sj::object();unsigned session=0;Uint64 notBefore=0,expires=0;};
struct State{std::mutex lock;bool quit=false;unsigned realtime=0,consumers=1u<<4;std::deque<Job> jobs;std::array<unsigned,6> sessions={};std::array<std::deque<std::string>,6> events;std::string roomGeneration;std::deque<std::string> releasedGenerations;int status=0;void *wake=nullptr;}state;
struct Identity{std::string access="cached";Uint64 renewAt=840000;};static std::array<Identity,3> identities;
static Uint64 clockNow=0,rateUntil=0;static int sockets=0,authCalls=0,replies=0,mode=0;static long lastHttpStatus=0;static bool forced=false;
static const char *cloudUrl="https://fixture.invalid",*cloudHost="fixture.invalid";
static Uint64 SDL_GetTicks(){return clockNow;}
static const char *SDL_getenv(const char *name){return forced&&!strcmp(name,"BT4_SIGNAL_HTTPS_ONLY")?"1":nullptr;}
PRODUCTION_EMIT
PRODUCTION_READ
static void SDL_WaitSemaphoreTimeout(void*,int){
 clockNow+=20;
 static std::vector<char> buffer(BT4_PROFILE_MEDIA_EVENT_CAP);
 if(Bt4Signal_Read(4,buffer.data(),buffer.size())==1){
  const auto event=Sj::parse(buffer.data());const auto &body=event.at("body");
  if(mode==2)assert(body.at("premium")==false);
  else if(mode==3){
   assert(body.at("frames").size()==6);
   assert(body.at("animation").at("atlas").get_ref<const std::string&>().size()==2200000);
  }
  else assert(body.at("state")=="ready");
  ++replies;state.quit=true;
 }
 if(clockNow>125000)state.quit=true;
}
static void wake(void*){}
static std::string str(const Sj &j,const char *k){auto i=j.find(k);return i!=j.end()&&i->is_string()?i->get<std::string>():std::string();}
static int Update_OnlineRequired(){return 0;}static void Update_SetOnlineRequired(int){}
namespace bt4route{static bool admission(const std::string &op){return op=="create"||op=="join"||op=="queue"||op=="ranked_queue";}}
static bool cloudAuthenticate(){++authCalls;if(clockNow<rateUntil){lastHttpStatus=429;return false;}identities[2].access="renewed";identities[2].renewAt=clockNow+840000;lastHttpStatus=200;return true;}
static bool http(const std::string &,const Sj &request,bool,Sj &answer,bool=false,const char *=nullptr,const std::string *=nullptr){
 if(mode==2){
  assert(request["op"]=="profile_media_save");
  assert(request["args"]["frames"][0].get<std::string>().size()==17600);
  answer={{"premium",false}};
 }else if(mode==3){
  assert(request["op"]=="profile_media_get"&&request["args"]["detail"]==true);
  auto frames=Sj::array();
  for(int i=0;i<6;++i)frames.push_back(std::string(2732,char('A'+i)));
  answer={{"kind","motion"},{"frames",frames},{"delays",{100,100,100,100,100,100}}};
  answer["animation"]={{"atlas",std::string(2200000,'A')}};
 }else{
  assert(request["op"]=="nickname_get");
  answer={{"state","ready"},{"name","Fixture"}};
 }
 return true;
}
struct VoiceWsMessage{bool binary=false;std::vector<unsigned char> bytes;};
struct VoiceWs{
 int status;
 bool delivered=false;
 VoiceWs(const std::string &,const std::string &,int,const std::string &,bool,void(*)(void*),void*):status(mode>=1?1:-1){++sockets;}
 void text(const std::string &message){if(mode>=2)assert(Sj::parse(message)["op"]=="authenticate");}
 bool read(VoiceWsMessage &packet){
  if(mode<2){status=-1;return false;}
  if(delivered)return false;
  delivered=true;
  const std::string ready=R"({"event":"bt4.ready","body":{}})";
  packet.bytes.assign(ready.begin(),ready.end());
  return true;
 }
};
PRODUCTION_ENTRY
static void reset(int m,bool force=false){state.quit=false;state.realtime=0;state.jobs={Job{}};state.sessions={};state.roomGeneration.clear();state.releasedGenerations.clear();clockNow=rateUntil=0;sockets=authCalls=replies=0;mode=m;forced=force;identities={};identities[2].access="cached";}
int main(){
 reset(0);cloudEntry(nullptr);assert(replies==1&&sockets==1&&clockNow<5000);
 reset(1);cloudEntry(nullptr);assert(replies==1&&sockets==1&&authCalls<=3&&clockNow<5000);
 reset(0,true);cloudEntry(nullptr);assert(replies==1&&sockets==0);
 reset(0,true);rateUntil=60000;cloudEntry(nullptr);assert(replies==1&&sockets==0&&authCalls<=3&&clockNow>=60000);
 reset(2);
 state.jobs.front().op="profile_media_save";
 state.jobs.front().args={{"frames",Sj::array({std::string(17600,'A')})}};
 state.jobs.front().expires=19000;
 cloudEntry(nullptr);
 assert(replies==1&&sockets==1&&clockNow<1000);
 assert(!emit(0,{{"event","reply"},{"body",{{"animation",std::string(25000,'A')}}}}));
 assert(!emit(4,{{"event","reply"},{"body",{{"unrelated",std::string(25000,'A')}}}}));
 reset(3);
 state.jobs.front().op="profile_media_get";
 state.jobs.front().args={{"detail",true}};
 cloudEntry(nullptr);
 assert(replies==1&&sockets==1&&clockNow<1000);
 puts("PASS: blocked upgrades, failed socket authentication, HTTPS-only operation and bounded rate-limit retries");
}
'''.replace('"JSON_HEADER"','"'+(ROOT/'port/third_party/voice/json.hpp').as_posix()+'"').replace(
            '"PROFILE_HEADER"','"'+(ROOT/'port/src/gs/profile_cards.h').as_posix()+'"').replace(
            'PRODUCTION_ENTRY',entry).replace('PRODUCTION_EMIT',emit).replace('PRODUCTION_READ',read).replace(
            'const char *=nullptr','const char *host=nullptr').replace('const std::string *=nullptr','const std::string *token=nullptr')
        with tempfile.TemporaryDirectory() as tmp:
            d=Path(tmp);src=d/'fallback.cpp';src.write_text(fixture,encoding='utf8');exe=d/('fallback.exe' if os.name=='nt' else 'fallback')
            opts={'creationflags':0x08004000} if os.name=='nt' else {}
            prefix=['rtk','proxy'] if os.name=='nt' else []
            result=subprocess.run(prefix+[os.environ.get('CXX','g++'),'-std=c++17','-O1',str(src),'-o',str(exe)],capture_output=True,text=True,timeout=120,**opts)
            self.assertEqual(result.returncode,0,result.stderr)
            result=subprocess.run(prefix+[str(exe)],capture_output=True,text=True,timeout=20,**opts)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
if __name__=='__main__':unittest.main()
