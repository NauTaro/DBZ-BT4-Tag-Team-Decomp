"""Shared developer rules must be deterministic and affect every fighter alike."""
from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[3]
class SharedDevRulesTests(unittest.TestCase):
    def test_render_requests_and_network_capture_are_atomic(self):
        cc=os.environ.get('CC') or shutil.which('gcc') or shutil.which('cc')
        sdk=Path(os.environ.get('SDL3_DIR',''))
        if not cc or not (sdk/'include/SDL3/SDL.h').exists():self.skipTest('A C compiler and SDL3 SDK are required')
        source=(ROOT/'port/src/gs/build_identity.c').read_text(encoding='utf8')
        production=source[source.index('static unsigned sDevRequestedFlags;'):source.index('extern FILE *Port_FileOpenHost')]
        code='''#include <SDL3/SDL.h>
#include <assert.h>
#include <stdio.h>
#include "bt4_dev_rules.h"
'''+production+r'''
static SDL_AtomicInt finished;
static int writer(void *unused){
 for(unsigned n=1;n<=100000;n++)Port_DevRulesRequest(n&15,0);
 SDL_SetAtomicInt(&finished,1);return 0;
}
int main(void){
 SDL_Init(0);SDL_Thread *t=SDL_CreateThread(writer,"rule-writer",NULL);assert(t);
 unsigned reads=0;do{
  unsigned char p[8]={0};unsigned flags;uint32_t seq;Port_DevRulesEncode(p);
  if(Bt4Dev_Unpack(p,&flags,&seq))assert(flags==(seq&15));++reads;
 }while(!SDL_GetAtomicInt(&finished));
 SDL_WaitThread(t,NULL);assert(sDevRequestedSequence==100000&&reads);
 Port_DevRulesRequest(7,1);assert(Port_DevRulesRequested()==31);
 puts("Dev controls: 100000 concurrent render/network updates preserve each flags/sequence pair PASS");SDL_Quit();
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            d=Path(tmp);(d/'probe.c').write_text(code,encoding='utf8');exe=d/('probe.exe' if os.name=='nt' else 'probe')
            subprocess.run([cc,'-std=gnu99','-O2','-I'+str(ROOT/'port/include'),'-I'+str(sdk/'include'),str(d/'probe.c'),'-L'+str(sdk/'lib'),'-lSDL3','-o',str(exe)],check=True)
            env=dict(os.environ,PATH=str(sdk/'bin')+os.pathsep+os.environ['PATH'])
            subprocess.run([str(exe)],env=env,check=True,timeout=30)

    def test_replication_pause_step_all_actors_and_rollback(self):
        cc=os.environ.get('CC') or shutil.which('gcc') or shutil.which('cc')
        if not cc:self.skipTest('A C compiler is required')
        code=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "bt4_dev_rules.h"
typedef int s32;typedef unsigned char u8;
enum {BATTLE_FLAG_PAUSE=0x100};static struct {unsigned flags;} work;
#define Battle_GetWork() (&work)
static int online,tag=1,actors=10,state=3;
static unsigned char delivered[10][18];static int gauges[10][7];
static int Port_NetActive(void){return online&&!tag;}
static int TagBattle_Network(void){return online&&tag;}
static int TagBattle_Active(void){return tag;}
static int TagBattle_Count(void){return actors;}
static int BtlSeq_GetState(void){return state;}
static void *BtlChar_Get(int a){return gauges[a];}
static int *BtlMember_GetActiveGauge(void *a){return a;}
static unsigned char *TagBattle_NetPad(int a){return delivered[a];}
static int Port_NetDevRulesPad(int a,unsigned char *p){memcpy(p,delivered[a]+9,8);return 1;}
'''+(ROOT/'src/battle/dev_rules.inc').read_text(encoding='utf8').replace('#include "../../port/include/bt4_dev_rules.h"','').replace('extern s32 Port_NetActive(void),Port_NetDevRulesPad(s32,u8 *);','')+r'''
static void command(int actor,unsigned mask,unsigned seq){Bt4Dev_Pack(delivered[actor]+9,mask,seq);}
int main(void){
 unsigned char p[8];unsigned flags;uint32_t seq;
 for(int i=0;i<10;i++){gauges[i][0]=20+i;gauges[i][1]=100+i;gauges[i][3]=0;gauges[i][4]=60000;gauges[i][5]=0;gauges[i][6]=500000;}
 command(1,7,1);assert(!DevRules_NetworkTick());assert(!DevRules_ProtectHealth());assert(gauges[0][0]==20);
 online=1;assert(!DevRules_NetworkTick());assert(DevRules_ProtectHealth());
 for(int i=0;i<10;i++)assert(gauges[i][0]==gauges[i][1]&&gauges[i][3]==60000&&gauges[i][5]==500000);
 gauges[0][3]=90000;assert(!DevRules_NetworkTick());assert(gauges[0][3]==90000); // preserve Maximum Power
 command(1,15,2);assert(DevRules_NetworkTick());assert(DevRules_NetworkTick());
 command(1,31,3);assert(!DevRules_NetworkTick());assert(DevRules_NetworkTick()); // exactly one step
 Bt4DevRules snapshot=sDevRules;
 command(1,7,4);assert(!DevRules_NetworkTick());Bt4DevRules advanced=sDevRules;
 sDevRules=snapshot;assert(!DevRules_NetworkTick());assert(!memcmp(&advanced,&sDevRules,sizeof(sDevRules)));
 command(1,15,2);assert(!DevRules_NetworkTick());assert(DevRules_Flags()==7); // stale delivery cannot undo a rule
 command(0,0,1);assert(!DevRules_NetworkTick());assert(!DevRules_ProtectHealth());
 DevRules_Reset();assert(!DevRules_Flags());
 tag=0;actors=2;command(0,7,8);memset(delivered[1],0,18);assert(!DevRules_NetworkTick());
 assert(gauges[0][0]==gauges[0][1]&&gauges[1][0]==gauges[1][1]);
 Bt4Dev_Pack(p,7,99);assert(Bt4Dev_Unpack(p,&flags,&seq)&&flags==7&&seq==99);
 for(int i=0;i<8;i++){p[i]^=0x40;assert(!Bt4Dev_Unpack(p,&flags,&seq));p[i]^=0x40;}
 Bt4Dev_Pack(p,7,0);assert(!Bt4Dev_Unpack(p,&flags,&seq));
 puts("Shared Dev rules: ten equal actors, 1v1, pause/resume, single-step, rollback, stale packets and corruption PASS");
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            d=Path(tmp);(d/'probe.c').write_text(code,encoding='utf8');exe=d/('probe.exe' if os.name=='nt' else 'probe')
            subprocess.run([cc,'-std=gnu99','-I'+str(ROOT/'port/include'),str(d/'probe.c'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)
if __name__=='__main__':unittest.main()
