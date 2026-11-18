"""Exercise the actual signaling loop with a blocked socket and deterministic HTTPS."""
from pathlib import Path
import os,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[2]
class TransportFallbackTests(unittest.TestCase):
    def test_socket_failure_and_rate_limit_do_not_starve_https_jobs(self):
        text=(ROOT/'port/src/gs/cloudflare_signal.inc').read_text(encoding='utf8')
        entry=text[text.index('static int cloudEntry('):]
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
using Sj=nlohmann::json;using Uint64=uint64_t;
#define BT4_CANONICAL_VERSION "0.1.39.6-R1.5"
struct Job{int consumer=4;unsigned serial=1;std::string op="nickname_get";Sj args=Sj::object();unsigned session=0;Uint64 notBefore=0;};
struct State{std::mutex lock;bool quit=false;unsigned realtime=0;std::deque<Job> jobs;std::array<unsigned,6> sessions={};std::string roomGeneration;std::deque<std::string> releasedGenerations;int status=0;void *wake=nullptr;}state;
struct Identity{std::string access="cached";Uint64 renewAt=840000;};static std::array<Identity,3> identities;
static Uint64 clockNow=0,rateUntil=0;static int sockets=0,authCalls=0,replies=0,mode=0;static long lastHttpStatus=0;static bool forced=false;
static const char *cloudUrl="https://fixture.invalid",*cloudHost="fixture.invalid";
static Uint64 SDL_GetTicks(){return clockNow;}
static const char *SDL_getenv(const char *name){return forced&&!strcmp(name,"BT4_SIGNAL_HTTPS_ONLY")?"1":nullptr;}
static void SDL_WaitSemaphoreTimeout(void*,int){clockNow+=20;if(clockNow>125000)state.quit=true;}
static void wake(void*){}
static std::string str(const Sj &j,const char *k){auto i=j.find(k);return i!=j.end()&&i->is_string()?i->get<std::string>():std::string();}
static bool emit(int,const Sj &event){assert(event["body"]["state"]=="ready");++replies;state.quit=true;return true;}
static int Update_OnlineRequired(){return 0;}static void Update_SetOnlineRequired(int){}
namespace bt4route{static bool admission(const std::string &op){return op=="create"||op=="join"||op=="queue"||op=="ranked_queue";}}
static bool cloudAuthenticate(){++authCalls;if(clockNow<rateUntil){lastHttpStatus=429;return false;}identities[2].access="renewed";identities[2].renewAt=clockNow+840000;lastHttpStatus=200;return true;}
static bool http(const std::string &,const Sj &request,bool,Sj &answer,bool=false,const char *=nullptr,const std::string *=nullptr){assert(request["op"]=="nickname_get");answer={{"state","ready"},{"name","Fixture"}};return true;}
struct VoiceWsMessage{bool binary=false;std::vector<unsigned char> bytes;};
struct VoiceWs{
 int status;
 VoiceWs(const std::string &,const std::string &,int,const std::string &,bool,void(*)(void*),void*):status(mode==1?1:-1){++sockets;}
 void text(const std::string &){}
 bool read(VoiceWsMessage &){status=-1;return false;}
};
PRODUCTION_ENTRY
static void reset(int m,bool force=false){state.quit=false;state.realtime=0;state.jobs={Job{}};state.sessions={};state.roomGeneration.clear();state.releasedGenerations.clear();clockNow=rateUntil=0;sockets=authCalls=replies=0;mode=m;forced=force;identities={};identities[2].access="cached";}
int main(){
 reset(0);cloudEntry(nullptr);assert(replies==1&&sockets==1&&clockNow<5000);
 reset(1);cloudEntry(nullptr);assert(replies==1&&sockets==1&&authCalls<=3&&clockNow<5000);
 reset(0,true);cloudEntry(nullptr);assert(replies==1&&sockets==0);
 reset(0,true);rateUntil=60000;cloudEntry(nullptr);assert(replies==1&&sockets==0&&authCalls<=3&&clockNow>=60000);
 puts("PASS: blocked upgrades, failed socket authentication, HTTPS-only operation and bounded rate-limit retries");
}
'''.replace('"JSON_HEADER"','"'+(ROOT/'port/third_party/voice/json.hpp').as_posix()+'"').replace('PRODUCTION_ENTRY',entry).replace('const char *=nullptr','const char *host=nullptr').replace('const std::string *=nullptr','const std::string *token=nullptr')
        with tempfile.TemporaryDirectory() as tmp:
            d=Path(tmp);src=d/'fallback.cpp';src.write_text(fixture,encoding='utf8');exe=d/('fallback.exe' if os.name=='nt' else 'fallback')
            opts={'creationflags':0x08004000} if os.name=='nt' else {}
            prefix=['rtk','proxy'] if os.name=='nt' else []
            result=subprocess.run(prefix+[os.environ.get('CXX','g++'),'-std=c++17','-O1',str(src),'-o',str(exe)],capture_output=True,text=True,timeout=120,**opts)
            self.assertEqual(result.returncode,0,result.stderr)
            result=subprocess.run(prefix+[str(exe)],capture_output=True,text=True,timeout=20,**opts)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
if __name__=='__main__':unittest.main()
