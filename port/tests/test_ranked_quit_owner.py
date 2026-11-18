"""Compile the actual concession identity helpers, including transport-abort regressions."""
from pathlib import Path
import os,shutil,subprocess,tempfile,unittest,sys
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/review/tests'))
from test_potara import function
class RankedQuitOwnerTests(unittest.TestCase):
    def test_only_explicit_pause_exit_can_identify_the_quitter(self):
        cc=shutil.which('gcc') or shutil.which('cc')
        if not cc:self.skipTest('C compiler required')
        hud=(ROOT/'src/battle/hud_0.c').read_text(encoding='utf-8')
        seq=(ROOT/'src/battle/btl_seq.c').read_text(encoding='utf-8')
        code='''
#include <assert.h>
#include <stdio.h>
typedef int s32;
static int ranked=1,aborted=1,rematch=0;
static s32 sRankedPauseQuitPad=-1;
int Port_RankedActive(void){return ranked;}
int BattleResult_IsAborted(void){return aborted;}
int Battle_IsRematchRequested(void){return rematch;}
'''+function(hud,'static void PauseMenu_RecordRankedQuit(')+'\n'+function(hud,'s32 PauseMenu_ConfirmedQuitPad(')+'\n'+function(seq,'s32 Port_RankedPauseAbortPad(')+'''
int main(void){
  for(int local=0;local<2;local++){
    sRankedPauseQuitPad=-1;
    /* A generic connection abort, including one while our pause menu is open. */
    assert(Port_RankedPauseAbortPad()==-1);
    for(int id=0;id<13;id++)PauseMenu_RecordRankedQuit(local,0,id);
    assert(Port_RankedPauseAbortPad()==-1);
    /* End-of-match navigation must not be treated as a surrender. */
    PauseMenu_RecordRankedQuit(local,1,0xF);assert(Port_RankedPauseAbortPad()==-1);
    /* Confirmed exit is attributed to its actual owner on either local peer. */
    for(int quitter=0;quitter<2;quitter++){
      sRankedPauseQuitPad=-1;PauseMenu_RecordRankedQuit(quitter,0,0xF);
      assert(Port_RankedPauseAbortPad()==quitter);
      assert((Port_RankedPauseAbortPad()==local)==(quitter==local));
    }
  }
  for(int id=0xD;id<=0xF;id++){
    sRankedPauseQuitPad=-1;PauseMenu_RecordRankedQuit(1,0,id);assert(Port_RankedPauseAbortPad()==1);
  }
  rematch=1;assert(Port_RankedPauseAbortPad()==-1);rematch=0;
  aborted=0;assert(Port_RankedPauseAbortPad()==-1);aborted=1;
  ranked=0;sRankedPauseQuitPad=-1;PauseMenu_RecordRankedQuit(0,0,0xF);assert(Port_RankedPauseAbortPad()==-1);
  ranked=1;PauseMenu_RecordRankedQuit(-1,0,0xF);PauseMenu_RecordRankedQuit(2,0,0xF);assert(Port_RankedPauseAbortPad()==-1);
  puts("PASS: generic disconnects cannot become local concessions; both peer identities verified");
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp);src=p/'quit.c';exe=p/('quit.exe' if os.name=='nt' else 'quit')
            src.write_text(code,encoding='utf-8')
            opts={'creationflags':0x08004000} if os.name=='nt' else {}
            cmd=(['rtk','proxy'] if shutil.which('rtk') else [])
            r=subprocess.run(cmd+[cc,'-std=c99',str(src),'-o',str(exe)],capture_output=True,text=True,**opts)
            self.assertEqual(r.returncode,0,r.stderr)
            r=subprocess.run(cmd+[str(exe)],capture_output=True,text=True,timeout=10,**opts)
            self.assertEqual(r.returncode,0,r.stdout+r.stderr)
