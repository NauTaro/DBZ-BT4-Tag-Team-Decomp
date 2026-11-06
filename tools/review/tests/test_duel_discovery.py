"""Exercise client room routing with current and legacy signaling descriptors."""
from pathlib import Path
import os, shutil, subprocess, tempfile, unittest

ROOT = Path(__file__).resolve().parents[3]

class DuelDiscoveryTests(unittest.TestCase):
    def test_duel_lobby_spectator_and_admission_order(self):
        cc = shutil.which('c++') or shutil.which('g++')
        if not cc:
            self.skipTest('A C++ compiler is required')
        code = r'''#include <cassert>
#include <deque>
#include <string>
#include <vector>
#include "SOURCE"
struct Command {unsigned id;std::string op;};
static std::vector<Command> commands;
static std::deque<std::string> events;
static bool acceptLeave=true;
static Uint64 now=1000;
Uint64 SDL_GetTicks(void){return now;}
extern "C" int Bt4Signal_Open(void){return 1;}
extern "C" int Bt4Signal_Status(void){return 1;}
extern "C" int Bt4Signal_Command(int,unsigned id,const char *op,const char *){
    commands.push_back({id,op});return acceptLeave||strcmp(op,"leave");
}
extern "C" int Bt4Signal_Read(int,char *out,size_t capacity){
    if(events.empty())return 0;
    assert(events.front().size()+1<=capacity);
    strcpy(out,events.front().c_str());events.pop_front();return 1;
}
extern "C" void PortVoice_P2PSession(int,const char*,const PortVoiceRoomMember*,int){}
static void begin(int role){
    commands.clear();events.clear();
    assert(Bt4Discovery_Begin(d.generation+1,role,1234,123,"Tester","ABCDEF","[\"127.0.0.1:12\"]","S"));
    assert(commands.size()==2&&commands[0].op=="leave");
    assert(commands[0].id<commands[1].id&&commands[1].id==d.init);
}
static Dj room(unsigned self,const char *mode){
    Dj j={{"self",self},{"host",1u},{"generation","00000000-0000-0000-0000-000000000001"},{"code","ABCDEF"},
          {"members",Dj::array({{{"peer",1u},{"name","Host"},{"role","player"}},
                                {{"peer",2u},{"name","Observer"},{"role","spectator"}},
                                {{"peer",3u},{"name","Fighter"},{"role","player"}}})}};
    if(mode)j["mode"]=mode;return j;
}
static Bt4DiscoveryStatus deliver(Dj j){
    events.push_back(Dj({{"event","reply"},{"serial",d.init},{"body",j}}).dump());
    Bt4DiscoveryStatus status={};assert(Bt4Discovery_Poll(d.generation,&status));return status;
}
int main(){
    begin(1);auto status=deliver(room(1,nullptr));
    assert(status.state==2&&status.lobby==0); // Missing mode is still a duel.
    d.addresses[2]=Dj::array({"127.0.0.1:22"});refreshStatus();
    assert(d.status.state==2&&d.status.watchCount==2); // Candidate plus the native lobby's seen marker.
    d.addresses[3]=Dj::array({"127.0.0.1:33"});refreshStatus();
    assert(d.status.state==3&&std::string(d.status.peerName)=="Fighter");
    begin(2);d.addresses[1]=Dj::array({"127.0.0.1:11"});status=deliver(room(3,"duel"));
    assert(status.state==3&&status.role==2&&!status.lobby);
    begin(4);d.addresses[1]=Dj::array({"127.0.0.1:11"});status=deliver(room(2,"duel"));
    assert(status.state==3&&!status.lobby); // Spectator routes to host.
    begin(2);d.addresses[1]=Dj::array({"127.0.0.1:11"});status=deliver(room(2,"lobby"));
    assert(status.state==3&&status.lobby); // Server mode overrides the join default.
    begin(5);status=deliver(room(1,nullptr));assert(status.state==2&&status.lobby);
    begin(1);Dj legacy=room(1,nullptr);legacy["members"]=Dj::array({{{"peer",1u},{"name","Host"}},{{"peer",2u},{"name","Fighter"}}});
    d.addresses[2]=Dj::array({"127.0.0.1:22"});status=deliver(legacy);
    assert(status.state==3&&!status.lobby);
    // Private broadcasts can beat the create/join reply on the other transport.
    begin(2);
    Dj udp={{"generation","00000000-0000-0000-0000-000000000001"},{"channel","duel"},{"kind","udp"},
            {"from",1u},{"body",{{"addresses",Dj::array({"127.0.0.1:11"})}}}};
    events.push_back(Dj({{"event","bt4.signal"},{"body",udp}}).dump());
    assert(Bt4Discovery_Poll(d.generation,&status)&&status.state==1);
    status=deliver(room(3,"duel"));assert(status.state==3&&status.peerCount==1);
    // A malformed or unrelated early packet cannot become this room's candidate.
    begin(2);udp["generation"]="00000000-0000-0000-0000-000000000002";
    events.push_back(Dj({{"event","bt4.signal"},{"body",udp}}).dump());
    status=deliver(room(3,"duel"));assert(status.state==1);
    // A failed endpoint RPC must be retried; enqueue success is not server success.
    begin(1);status=deliver(room(1,"duel"));assert(!d.announcements.empty());
    auto failed=d.announcements.begin()->first;auto peer=d.announcements.begin()->second;
    events.push_back(Dj({{"event","reply"},{"serial",failed},{"body",{{"error","SIGNALING_UNAVAILABLE"}}}}).dump());
    Bt4Discovery_Poll(d.generation,&status);assert(!d.announced[peer]);
    auto before=commands.size();now+=2001;Bt4Discovery_Poll(d.generation,&status);
    assert(commands.size()>before&&!d.announcements.empty());
    // Credentials stay local; only the relay endpoint is announced to participants.
    assert(Bt4Discovery_RelayAsk(d.generation));auto turnRequest=d.turnRequest;
    events.push_back(Dj({{"event","reply"},{"serial",turnRequest},{"body",{{"server","turn.example:3478"},{"username","test-user"},{"credential","test-pass"}}}}).dump());
    Bt4Discovery_Poll(d.generation,&status);Bt4TurnCredentials turn={};
    assert(Bt4Discovery_RelayPoll(d.generation,&turn)==1);
    assert(std::string(turn.server)=="turn.example:3478");
    assert(Bt4Discovery_PublishRelay(d.generation,"127.0.0.1:9999"));
    assert(d.localAddresses.back()=="relay=127.0.0.1:9999"&&d.turn.password[0]==0);
    begin(2);assert(!Bt4Discovery_RelayAsk(d.generation));
    // Departure clears stale peer candidates rather than retaining the last fighter.
    begin(1);d.addresses[3]=Dj::array({"127.0.0.1:33"});status=deliver(room(1,"duel"));assert(status.peerCount==1);
    auto alone=room(1,"duel");alone["members"]=Dj::array({{{"peer",1u},{"name","Host"},{"role","player"}}});
    status=deliver(alone);assert(status.state==2&&status.peerCount==0&&status.peerAddr[0][0]==0);
    acceptLeave=false;commands.clear();
    assert(!Bt4Discovery_Begin(d.generation+1,1,1234,123,"Tester","","[\"127.0.0.1:12\"]","S"));
    assert(commands.size()==1&&commands[0].op=="leave");
}
'''.replace('SOURCE', (ROOT/'port/src/gs/match_supabase.cpp').as_posix())
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);include=root/'SDL3';include.mkdir()
            (include/'SDL.h').write_text('#pragma once\n#include <stdint.h>\ntypedef uint64_t Uint64;\ntypedef struct SDL_Event {int type;} SDL_Event;\nUint64 SDL_GetTicks(void);\n')
            source=root/'discovery.cpp';source.write_text(code,encoding='utf8')
            exe=root/('discovery.exe' if os.name=='nt' else 'discovery')
            flags={'creationflags':0x08000000} if os.name=='nt' else {}
            result=subprocess.run([cc,'-std=c++17','-O2','-I'+str(root),str(source),'-o',str(exe)],capture_output=True,text=True,timeout=60,**flags)
            self.assertEqual(result.returncode,0,result.stderr)
            result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=15,**flags)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)

if __name__=='__main__':unittest.main()
