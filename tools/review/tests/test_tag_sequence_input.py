"""This file is copied into both source trees as a portable regression test."""
from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[3]
def function(source,name):
    start=source.index('s32 '+name+'(');brace=source.index('{',start);depth=1;end=brace+1
    while depth:
        if source[end]=='{':depth+=1
        elif source[end]=='}':depth-=1
        end+=1
    return source[start:end]
class TagSequenceInputTests(unittest.TestCase):
    def test_remote_start_is_shared_and_local_start_cannot_skip_alone(self):
        cc=shutil.which('cc') or shutil.which('gcc')
        if not cc:self.skipTest('A C compiler is required')
        online=(ROOT/'src/battle/tag_team_online.inc').read_text(encoding='utf8')
        pause=(ROOT/'src/battle/hud_e_d.c').read_text(encoding='utf8')
        code='''#include <assert.h>
#include <string.h>
#define PORT 1
#define BTL_PAUSE_START 0x1000
typedef int s32;
typedef unsigned char u8;
typedef struct {s32 padCount;} BtlPause;
typedef struct {unsigned gamePressed;} Pad;
static BtlPause work={2};static Pad gPad[2];
static u8 sTagOnlinePads[10][18];static int count=10,network=1,replay=0;
static BtlPause *BtlPause_GetWork(void){return &work;}
int TagBattle_Count(void){return count;}
int TagBattle_Network(void){return network;}
static int BattleReplay_IsActive(void){return replay;}
static int BattleReplay_IsLoaded(void){return replay;}
'''+function(online,'TagBattle_NetIntroSkip')+'\n'+function(pause,'BtlPause_CheckStart')+'''
int main(void){
 int actor,local;
 /* The same network frame must give the same transition on every client,
    independent of which local pad was used and who has keyboard focus. */
 for(actor=0;actor<10;++actor){
   memset(sTagOnlinePads,0,sizeof(sTagOnlinePads));sTagOnlinePads[actor][8]=1;
   for(local=0;local<3;++local){
     gPad[0].gamePressed=local==1?BTL_PAUSE_START:0;
     gPad[1].gamePressed=local==2?BTL_PAUSE_START:0;
     assert(BtlPause_CheckStart()==1);
   }
 }
 memset(sTagOnlinePads,0,sizeof(sTagOnlinePads));
 gPad[0].gamePressed=gPad[1].gamePressed=BTL_PAUSE_START;
 assert(BtlPause_CheckStart()==0);
 /* Offline and replay keep the original local-pad behavior. */
 network=0;assert(BtlPause_CheckStart()==1);
 gPad[0].gamePressed=0;assert(BtlPause_CheckStart()==1);
 replay=1;assert(BtlPause_CheckStart()==0);
 gPad[0].gamePressed=BTL_PAUSE_START;assert(BtlPause_CheckStart()==1);
 return 0;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);src=root/'sequence.c';exe=root/('sequence.exe' if os.name=='nt' else 'sequence');src.write_text(code,encoding='utf8')
            flags={'creationflags':0x08000000} if os.name=='nt' else {}
            p=subprocess.run([cc,'-std=c99',str(src),'-o',str(exe)],capture_output=True,text=True,timeout=30,**flags);self.assertEqual(p.returncode,0,p.stderr)
            p=subprocess.run([str(exe)],capture_output=True,text=True,timeout=15,**flags);self.assertEqual(p.returncode,0,p.stdout+p.stderr)
if __name__=='__main__':unittest.main()
