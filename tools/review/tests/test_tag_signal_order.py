"""Compile the production signal receiver with a deterministic RTC transport."""
from pathlib import Path
import os, shutil, subprocess, tempfile, unittest
ROOT=Path(__file__).resolve().parents[3]

def method(source, name):
    start=source.index('    '+name+'(');brace=source.index('{',start);end=brace+1;depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]

class TagSignalOrderTests(unittest.TestCase):
    def test_early_offer_candidates_expiry_membership_and_bounds(self):
        cc=shutil.which('c++') or shutil.which('g++')
        if not cc:self.skipTest('A C++ compiler is required')
        source=(ROOT/'port/src/gs/tag_team_p2p.cpp').read_text(encoding='utf8')
        production='\n'.join(method(source,name) for name in ('bool validSignal','bool takeSignal','void drainSignals','void receiveSignal'))
        code=r'''#include <array>
#include <deque>
#include <vector>
#include <string>
#include <cassert>
#define JSON_NOEXCEPTION
#include "JSON"
using Pj=nlohmann::json;using Uint64=unsigned long long;
static Uint64 now=1000;static Uint64 SDL_GetTicks(){return now;}
static unsigned number(const Pj &j,const char *k){auto p=j.find(k);return p!=j.end()&&p->is_number_unsigned()?p->get<unsigned>():0;}
static std::string text(const Pj &j,const char *k){auto p=j.find(k);return p!=j.end()&&p->is_string()?p->get<std::string>():std::string();}
struct Peer{unsigned id=0;int pc=-1;bool remote=false;std::vector<Pj> candidates;};
struct PendingSignal{Pj body;Uint64 at;};
static int descriptions=0,candidates=0,answers=0;
static struct{
 int rtcSetRemoteDescription(int,const char*,const char*){++descriptions;return 0;}
 void rtcAddRemoteCandidate(int,const char*,const char*){++candidates;}
 void rtcSetLocalDescription(int,const char *kind){assert(std::string(kind)=="answer");++answers;}
}rtc;
struct Session{
 unsigned self=0;std::string generation;Peer peer;std::deque<PendingSignal> earlySignals;
 Peer *find(unsigned id){return peer.id==id?&peer:nullptr;}
PRODUCTION
};
static const char *gen="00000000-0000-0000-0000-000000000001";
static Pj signal(const char *kind,unsigned from=1,unsigned to=2){
 Pj body=std::string(kind)=="candidate"?Pj{{"candidate","candidate:fixture"},{"mid","0"}}:Pj{{"sdp","fixture-sdp"}};
 return {{"channel","tag"},{"kind",kind},{"generation",gen},{"from",from},{"to",to},{"body",body}};
}
int main(){
 Session s;s.receiveSignal(signal("candidate"));s.receiveSignal(signal("offer"));
 assert(s.earlySignals.size()==2&&descriptions==0);
 s.generation=gen;s.self=2;s.drainSignals();assert(s.earlySignals.size()==2);
 s.peer.id=1;s.peer.pc=42;s.drainSignals();
 assert(s.earlySignals.empty()&&descriptions==1&&candidates==1&&answers==1&&s.peer.remote);
 auto foreign=signal("offer");foreign["generation"]="00000000-0000-0000-0000-000000000002";
 s.receiveSignal(foreign);s.receiveSignal(signal("offer",1,3));s.receiveSignal(signal("answer"));
 assert(s.earlySignals.empty()&&descriptions==1);
 Session host;host.self=1;host.generation=gen;host.peer.id=2;host.peer.pc=43;
 host.receiveSignal(signal("offer",2,1));assert(descriptions==1);
 host.receiveSignal(signal("answer",2,1));assert(descriptions==2&&answers==1);
 Session expired;expired.receiveSignal(signal("offer"));now+=10001;
 expired.generation=gen;expired.self=2;expired.peer.id=1;expired.peer.pc=44;expired.drainSignals();
 assert(expired.earlySignals.empty()&&descriptions==2);
 Session bounded;for(int i=0;i<100;++i)bounded.receiveSignal(signal("candidate"));assert(bounded.earlySignals.size()==32);
 Session invalid;auto bad=signal("offer");bad["body"]["sdp"]=std::string(14001,'x');invalid.receiveSignal(bad);
 invalid.receiveSignal(signal("offer",11,2));invalid.receiveSignal(signal("unknown"));assert(invalid.earlySignals.empty());
}
'''.replace('"JSON"','"'+(ROOT/'port/third_party/voice/json.hpp').as_posix()+'"').replace('PRODUCTION',production)
        with tempfile.TemporaryDirectory() as directory:
            d=Path(directory);src=d/'order.cpp';exe=d/('order.exe' if os.name=='nt' else 'order');src.write_text(code,encoding='utf8')
            flags={'creationflags':0x08000000} if os.name=='nt' else {}
            result=subprocess.run([cc,'-std=c++17','-O2',str(src),'-o',str(exe)],capture_output=True,text=True,timeout=90,**flags)
            self.assertEqual(result.returncode,0,result.stderr)
            result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=15,**flags)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)

if __name__=='__main__':unittest.main()
