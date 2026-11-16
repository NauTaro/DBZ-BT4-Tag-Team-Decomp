"""Public online pad delivery must not depend on optional developer controls."""
from pathlib import Path
import os, shutil, subprocess, tempfile, unittest

ROOT = Path(__file__).resolve().parents[3]

class PublicBattlePadTests(unittest.TestCase):
    def test_each_battle_delivers_input_without_developer_rules(self):
        cc = shutil.which('gcc') or shutil.which('cc')
        if not cc: self.skipTest('C compiler required')
        source = (ROOT/'src/battle/battle.c').read_text()
        loop = source[source.index('void Battle_Loop(void)'):]
        block = loop[loop.index('Gfx_BeginFrame();'):loop.index('if(TagBattle_Active()){')]
        code = r'''
#include <assert.h>
#include <stdio.h>
static int network,tag,developer,reads,frame,checks,devPauseOwned;
struct { unsigned flags; } work;
#define BATTLE_FLAG_PAUSE 256
void Gfx_BeginFrame(void){++frame;}
int TagBattle_Network(void){return tag;}
int Port_NetActive(void){return network;}
void Pad_Update(void){++reads;}
int DevRules_Online(void){return developer;}
int DevRules_NetworkTick(void){return 0;}
int DevPause_Begin(int unused){return 0;}
void Gsc_Update(void){}
void BtlScript_Update(void){}
void BtlChars_CheckStart(void){++checks;}
#define Battle_GetWork() (&work)
static void sample(void){
''' + block + r'''
}
int main(void){
  for(network=0;network<2;++network)for(tag=0;tag<2;++tag)for(developer=0;developer<2;++developer){
    reads=frame=checks=0;work.flags=0;
    /* Tag captures before its shared frame barrier, outside this block. */
    if(tag)Pad_Update();
    sample();
    assert(reads==1&&frame==1&&checks==1);
  }
  puts("PASS: public and Dev duel/Tag/offline input delivered exactly once");
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp);src=p/'probe.c';exe=p/('probe.exe' if os.name=='nt' else 'probe')
            src.write_text(code)
            hidden={'creationflags':0x08000000} if os.name=='nt' else {}
            prefix=['rtk','proxy'] if shutil.which('rtk') else []
            build=subprocess.run(prefix+[cc,str(src),'-o',str(exe)],capture_output=True,text=True,**hidden)
            self.assertEqual(build.returncode,0,build.stdout+build.stderr)
            run=subprocess.run(prefix+[str(exe)],capture_output=True,text=True,timeout=10,**hidden)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)
