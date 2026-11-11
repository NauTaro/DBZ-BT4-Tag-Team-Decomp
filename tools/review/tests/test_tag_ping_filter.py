"""A single delayed pong must not prequeue seconds of stale fighter input."""
from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[3]

class TagPingTests(unittest.TestCase):
    def test_recent_minimum_recovers_after_spikes_and_tracks_sustained_rtt(self):
        cc=shutil.which('c++') or shutil.which('g++')
        if not cc:self.skipTest('A C++ compiler is required')
        source=(ROOT/'port/src/gs/tag_team_p2p.cpp').read_text(encoding='utf8')
        start=source.index('    void pingSample(');brace=source.index('{',start);end=brace+1;depth=1
        while depth:
            depth+=(source[end]=='{')-(source[end]=='}');end+=1
        code='''#include <array>
#include <cassert>
#include <cstdio>
static bool routeLog=false;
static struct {int ping=-1;} service;
struct Session {
std::array<unsigned,8> pingSamples={};unsigned pingCount=0,pingCursor=0;
'''+source[start:end]+'''
};
int main(){
 Session s;s.pingSample(40);assert(service.ping==40);
 s.pingSample(300);assert(service.ping==40);
 s.pingSample(35);assert(service.ping==35);
 s.pingSample(10000);assert(service.ping==35&&s.pingCursor==3);
 for(int i=0;i<7;++i)s.pingSample(150);assert(service.ping==35);
 s.pingSample(150);assert(service.ping==150);
 s.pingSample(20);assert(service.ping==20);
 Session next;next.pingSample(100);assert(service.ping==100);
}
'''
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);src=root/'ping.cpp';exe=root/('ping.exe' if os.name=='nt' else 'ping');src.write_text(code)
            flags={'creationflags':0x08000000} if os.name=='nt' else {}
            r=subprocess.run([cc,'-std=c++17',str(src),'-o',str(exe)],capture_output=True,text=True,timeout=30,**flags)
            self.assertEqual(r.returncode,0,r.stderr)
            r=subprocess.run([str(exe)],capture_output=True,text=True,timeout=15,**flags)
            self.assertEqual(r.returncode,0,r.stdout+r.stderr)

if __name__=='__main__':unittest.main()
